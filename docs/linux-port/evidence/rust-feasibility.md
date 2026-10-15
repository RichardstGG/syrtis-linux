# PR-00 Evidence — Rust core / C ABI on Linux

**執行日期**：2026-10-15　**執行者**：Claude Code（Rust lane）　**分支**：`docs/linux-feasibility`
**標記**：✅ 實測｜🔶 原始碼支持、未執行｜❓ 未驗證

本文件只記錄實際執行過的命令與結果。所有本機路徑以 `~` 或 repository 相對路徑表示；真實使用量只記錄「有無資料」，不記錄數字、帳號或憑證內容。

## 1. 基線

| 項目 | 值 |
| --- | --- |
| Fork HEAD（= upstream `main`） | `b426c297717e7cc91b087ecd4bf7603972d5d7d8` |
| `vendor/tokscale-core` gitlink | `fcb85923e5544488f98001cfa390740d0009b9da`（與 Syrtis-Windows 相同） |
| `Cargo.lock` sha256 | `7a74ee19e61c6466225e6f3f94ebf912583e13f461fe38d28dd9fe6fa12f44d9` |
| OS／桌面 | Debian 13.7 x86_64、KDE Plasma 6.3.6、Wayland session |
| Toolchain | rustup，`rustc 1.96.1 (31fca3adb 2026-06-26)`（engine `rust-toolchain.toml`） |
| C toolchain | g++ 14.2.0、cmake 3.31.6 |

## 2. 編譯

| 命令 | 結果 |
| --- | --- |
| `cargo build --release -p tb_core_ffi --locked`（基線） | ❌ exit 101，3 × E0061 |
| 同上，套用 §3 一行修正 | ✅ exit 0，162 s；32 個 warning 全為 dead code（macOS 專用路徑） |
| `cargo test --workspace --locked --no-fail-fast`（隔離 `HOME`／`XDG_*`／`TOKSCALE_CONFIG_DIR`） | ✅ exit 0，121 s；**2,383 passed／0 failed／6 ignored**（tb_core_ffi 620、tokscale-core 1,708、整合測試 55） |
| `cargo clippy --workspace --all-targets --locked` | ✅ exit 0；47 個 warning（dead code、lifetime elision、參數數量等），無 error |

產物：`libtb_core_ffi.a`（44 MB）、`libtb_core_ffi.so`（19 MB）。
`--print native-static-libs`（命令見 §11，exit 0）：`-lgcc_s -lutil -lrt -lpthread -lm -ldl -lc`。

## 3. 基線編譯失敗的根因與最小修正（PR-01 範圍）

`crates/tb_core_ffi/src/agent_usage.rs` 的 `load_claude_credentials_from_keychain_item` 在 macOS 版接受 `(service, account)`，`#[cfg(not(target_os = "macos"))]` stub 只接受 `(account)`。上游 CI 只建置 macOS，因此簽名漂移從未被發現。

```diff
 #[cfg(not(target_os = "macos"))]
 fn load_claude_credentials_from_keychain_item(
+    _service: &str,
     _account: Option<&str>,
 ) -> Result<Option<String>, String> {
```

本修正**未**提交到 PR-00；僅在 detached 實驗 worktree 中套用以繼續驗證。

## 4. C ABI symbol 與 smoke

- `nm -D --defined-only libtb_core_ffi.so`：`ctb.h` 宣告 26 個 `tb_*`，`.so` 匯出 26 個，**完全一致** ✅
- C smoke（`gcc -std=c11` 連結 staticlib；合成 `HOME`，含一筆 Claude JSONL；`TZ=Asia/Taipei`）：**12/12 PASS** ✅

| 呼叫 | 結果 |
| --- | --- |
| `tb_probe` | `{"ok":true,"messages":1}`（保留 legacy shape） |
| `tb_client_ids` | ok，含 `antigravity`、`antigravity-cli` |
| `tb_quota_provider_ids` | ok：`codex, claude, antigravity, copilot, grok, grok-bot, kiro, opencode` |
| `tb_graph(NULL)`／`tb_graph("2026")` | ok，合成訊息計入 claude，成本已定價 |
| `tb_hourly_report(NULL,"claude")` | ok，小時桶依 `TZ` 落在本地時間 |
| `tb_agents_report`、`tb_tokens_per_min` | ok |
| `tb_set_extra_scan_paths("{}")` | ok；`("not json")` → `ok:false` 錯誤 envelope |
| `tb_quota_curve`（無 binding） | `ok:false`，`quota curve binding is unavailable` |
| `tb_antigravity_login_marker` | `ok:false`，`marker_unavailable`（Linux 無 agy Keychain，符合預期） |
| `tb_free(NULL)` | 無 crash |

## 5. Linux entry point 支援矩陣

| Entry point | Linux v1 | 依據 |
| --- | --- | --- |
| `tb_probe`、`tb_graph`、`tb_refresh_graph`、`tb_model_report`、`tb_hourly_report`、`tb_agents_report`、`tb_usage_trace`、`tb_tokens_per_min`、`tb_client_ids`、`tb_quota_provider_ids`、`tb_filter_parity_probe`、`tb_free` | Supported | ✅ smoke（`tb_model_report`、`tb_usage_trace`、`tb_filter_parity_probe` 為 🔶） |
| `tb_set_extra_scan_paths` | Supported（v1 UI 可不暴露） | ✅ smoke |
| `tb_agent_usage` | **Blocked → PR-01 修正後 Partial** | ✅ 見 §6 |
| `tb_quota_curve`、`tb_window_usage` | Blocked（依賴 account scope） | 🔶 |
| `tb_set_claude_config_dirs` | 不暴露（Keychain-only） | 🔶 |
| `tb_set_antigravity_accounts`、`tb_set_antigravity_binding`、`tb_antigravity_capture`、`tb_antigravity_remove`、`tb_antigravity_login_marker`、`tb_antigravity_auto_capture` | 不暴露（macOS agy／Keychain） | ✅ marker／🔶 其他 |
| `tb_set_keychain_consent` | 不暴露 | 🔶 |
| `tb_set_cursor_sync`、`tb_cursor_sync` | v1 範圍外 | 🔶 |

原始碼中寫死的 macOS 執行檔：`/usr/bin/security`、`/usr/bin/dscl`、`/Applications/Antigravity.app`；在 Linux 屬於「spawn 失敗即降級」，PR-01 需逐一以測試確認降級訊息不含路徑或秘密。

## 6. Live provider 實測（維護者同意；輸出已遮蔽）

真實 `HOME`，連續兩次 `tb_agent_usage()`，只記錄狀態欄位：

| Provider | source | windows | error |
| --- | --- | --- | --- |
| codex | `oauth` | 0 | `Codex account identity could not be verified.` |
| claude | `unconfigured` | 0 | `Claude OAuth credentials not found. …` |
| antigravity | `cli`（Language Server 路徑已找到） | 0 | `Antigravity account identity could not be verified.` |

`publicationGeneration` 依序為 1、2 ✅。整次呼叫約 1 秒，**未發出 provider 請求**。

**根因（✅ 實測 + 原始碼）**：`agent_account_scope.rs` 的 `SystemBackend::random_bytes` 在非 macOS 回傳 `Err(UnsupportedPlatform)`。實測後 `~/.local/share/com.nyanako.tokenbar/` 只有 `quota-account-scope-v1.lock`，沒有安裝金鑰；provider 的 cache binding 依賴此金鑰，因此在請求前終止。**Linux 上整個 quota 功能目前不可用**，不只是 last-good。修正見 ADR-012。

單元測試全數通過卻沒有抓到此問題，是因為測試注入 mock backend。PR-01 需新增一個以真實 `SystemBackend` 為對象、但使用暫存目錄的測試。

Claude：維護者使用 Claude desktop 帳號；現行程式只認 `~/.claude/.credentials.json`、`CLAUDE_CODE_OAUTH_TOKEN` 與 Keychain，本機前兩者都不存在。Linux 憑證來源 ❓，排入 PR-04。

## 7. Antigravity Token history（維護者接受依賴 tokscale sync）

| 步驟 | 結果 |
| --- | --- |
| `tokscale antigravity status`（tokscale 4.18.0） | exit 0；偵測到 Language Server 連線；首次執行時快取為空 |
| `tokscale antigravity sync` | ✅ exit 0（以 `$?` 直接取得，見 §11）；快取 session 數非零 |
| FFI `tb_refresh_graph(NULL)`（真實 `HOME`） | ✅ `antigravity` client 出現且有非零訊息；`claude`、`codex` 同樣有資料。7 s（快取冷熱未控制） |

`~/.gemini/antigravity-cli/conversations` 在本機不存在，因此 `antigravity-cli` lane 只做到原始碼確認（🔶）。

## 8. 觀察到的副作用（皆位於使用者目錄、可移除）

- `~/.local/share/com.nyanako.tokenbar/quota-account-scope-v1.lock`（空檔）由 `tb_agent_usage` 建立。
- `~/.config/tokscale/cache/pricing-litellm.json`、`pricing-openrouter.json` 由 FFI 定價刷新改寫；engine 與上游 tokscale CLI 共用此目錄（ADR-014）。
- `~/.config/tokscale/antigravity-cache/` 由 `tokscale antigravity sync` 建立（可用 `tokscale antigravity purge-cache` 移除）。

## 9. PR-01 最小變更集（提案）

1. §3 的 stub 簽名修正。
2. ADR-012：Linux `random_bytes`，並加入以真實 backend 和暫存目錄執行的測試。
3. ADR-013：Antigravity OAuth client 掃描根加入 Linux 安裝位置（PR-00 觀察到 `/opt/antigravity`）。
4. 確認 `/usr/bin/security`、`dscl` 降級路徑的錯誤訊息不含秘密。
5. Linux CI：Debian trixie container、`cargo build --locked`、`cargo test`、`cargo clippy`、C smoke。

修正後必須以 live 實測重新驗證 Codex／Antigravity quota。注意：Codex 在 access token 需要刷新時，會依既有設計把新憑證寫回 `auth.json`，執行前需維護者同意。

## 10. 未驗證項目

- Antigravity Language Server Connect-RPC 呼叫本身（被 §6 的阻斷擋在前面）❓
- Claude desktop 帳號在 Linux 的憑證位置 ❓
- Debian 套件 rustc 能否建置（本 PR 直接採用 rustup 鎖定版本，未測試）❓
- 冷／熱快取的效能基線（NFR-06）❓

## 11. 命令與 exit code 附錄（回應 Antigravity 審查，2026-10-15 重跑）

第一次記錄 `tokscale antigravity sync` 時，命令後面接了 `| tail`，當時寫下的 `exit=0` 其實是 `tail` 的結束碼。以下所有 exit code 都是重跑後直接取 `$?`，命令輸出改寫入檔案，不再經過 pipe。`$LIB` 指向 `target/release`，`$INC` 指向 `Sources/CTB/include`，`$SMOKE_HOME` 是合成的家目錄。

| 步驟 | 命令 | exit |
| --- | --- | --- |
| 匯出 symbol | `nm -D --defined-only $LIB/libtb_core_ffi.so > nm.txt` | 0 |
| 與 header 比對 | `diff hdr.txt so.txt`（`hdr.txt` = `grep -o 'tb_[a-z_]*(' ctb.h \| tr -d '(' \| sort -u`；`so.txt` = `nm.txt` 中型別 `T`、`tb_` 開頭的名稱） | 0（無差異，26 = 26） |
| 連結參數 | `cargo rustc --release -p tb_core_ffi --locked --crate-type staticlib -- --print native-static-libs` | 0 |
| 編譯 smoke | `gcc -std=c11 -Wall -O1 -I$INC smoke.c $LIB/libtb_core_ffi.a -lgcc_s -lutil -lrt -lpthread -lm -ldl -lc -o smoke` | 0 |
| 執行 smoke | `env -i PATH=/usr/bin:/bin HOME=$SMOKE_HOME XDG_CONFIG_HOME=… XDG_DATA_HOME=… XDG_CACHE_HOME=… TOKSCALE_CONFIG_DIR=… TZ=Asia/Taipei ./smoke` | 0（`fails=0`） |
| 編譯 live／graph harness | 同 smoke 的 gcc 命令，來源為 `live.c`、`graph.c` | 0、0 |
| live quota | `./live run1.json run2.json`（真實 `HOME`） | 0（外層 envelope `ok:true`；三個 provider 皆帶 error、0 個 window；generation 1、2） |
| 真實資料 graph | `./graph g.json` | 0（`ok:true`；`claude`、`codex`、`antigravity` 皆有非零訊息） |
| lock 檔存在 | `test -f ~/.local/share/com.nyanako.tokenbar/quota-account-scope-v1.lock` | 0；同目錄中 lock 以外的檔案數為 0，也就是沒有安裝金鑰 |
| Antigravity status | `tokscale antigravity status` | 0 |
| Antigravity sync | `tokscale antigravity sync` | 0 |

smoke 的 C 原始碼會在 PR-01 以 `linux/tests/ffi-smoke/smoke.c` 提交。live／graph harness 很短，全文如下：

```c
/* live.c — one tb_agent_usage() per output file; only redacted status fields are reported */
#include <stdio.h>
#include "ctb.h"
int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        char *p = tb_agent_usage();
        FILE *f = fopen(argv[i], "w");
        if (!f || !p) return 2;
        fputs(p, f); fclose(f); tb_free(p);
    }
    return 0;
}
```

```c
/* graph.c — tb_refresh_graph(NULL) to a file; only per-client non-zero presence is reported */
#include <stdio.h>
#include "ctb.h"
int main(int argc, char **argv) {
    char *p = tb_refresh_graph(NULL);
    FILE *f = fopen(argv[1], "w");
    if (!f || !p) return 2;
    fputs(p, f); fclose(f); tb_free(p);
    return 0;
}
```

輸出檔建立時 `umask 077`，讀取完狀態欄位後立即刪除。
