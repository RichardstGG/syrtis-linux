# Syrtis Linux — 驗收測試與 Stop／Go

**標記**：✅ 已在 PR-00 實測｜⏳ 待執行

## PR-00 Go／No-Go

| # | 問題 | 判準 | 狀態 |
| --- | --- | --- | --- |
| Q1 | Rust FFI 能否在 Debian 13 建置？ | pinned toolchain release build exit 0；26／26 symbol 匯出；C smoke 全過；`cargo test` 無失敗 | ✅ **Go（需 PR-01 一行修正）** |
| Q2 | 三個 Client 是否有資料來源？ | 本機 Token：Claude、Codex、Antigravity（sync 後）皆有資料；quota：至少一條路徑在修正後可驗證 | ✅ Token Go／⏳ quota 待 PR-01 修正 RNG 後重測 |
| Q3 | 3D renderer 選型？ | 方案 A 在 llvmpipe、2x 縮放下拖曳 p95 < 16 ms；20／20 命中測試正確 | ❌ **單一方案未達標**：A 在 hardware、llvmpipe 的 2x 下 p95 ≥ 16 ms；B 在 Qt software 後端沒有畫出方塊。「B 用於 RHI、A 用於 software」的組合在 6 組量測中全部合格，**已採用（ADR-006，2026-10-15）→ Go**。命中測試 12 組皆 40/40 |
| Q4 | 托盤與單實例在 Plasma Wayland 可用？ | 點托盤 → 視窗出現並取得焦點；第二次啟動聚焦既有視窗 | ⚠️ **部分**：兩種托盤後端都能透過 D-Bus 程式化 Activate 開窗並取得焦點；QLocalServer 單實例 IPC 成功，但有其他視窗持有焦點時**聚焦失敗**；真人點擊、面板四方向、KDBusService、activation token 傳遞：未執行 |

**PR-00 整體 Go 條件**：Q1、Q2 達成，Q3、Q4 有實測結論（Go 或明確的降階方案），且維護者確認 ADR 修訂。

## 各 PR 驗收

| PR | 必要測試 |
| --- | --- |
| PR-01 | `cargo build/test/clippy --locked`（Linux CI）；C smoke 26 個 entry point；真實 `SystemBackend` 搭配暫存目錄建立金鑰（`0700`／`0600`）；`/usr/bin/security` 不存在時的降級訊息不含秘密；live quota 重測（維護者同意） |
| PR-02 | 假 FFI 的 envelope 成功／錯誤／malformed 解碼；`tb_free` 計數相等；GUI thread 不呼叫 `tb_*`（測試中斷言 thread id）；generation 防亂序；Plasma Wayland 啟動、托盤、單實例、結束時無殘留 process |
| PR-03 | C++ harness 對 `usage-pace`、`format`、`provider-quota-pace-v3` fixture 的輸出與 Swift 參考值以 `diff.py` 比對全部相同（`TZ=Asia/Taipei`、`en`） |
| PR-04 | Expired、No credential、401／403、429、Offline、Wrong account、帳號切換 fixture 各自可區分；不跨帳號重用 last-good；quota 停用開關有效 |
| PR-05 | 24h、跨午夜、跨年、365／366 天、Cache Read／Write、per-client 合計與 Rust 輸出一致；hourly／Agents 的 client 篩選在 Rust fold 前生效；空資料顯示空狀態 |
| PR-06 | 365／366 天 mapping；命中測試；`QT_QUICK_BACKEND=software` 與 `LIBGL_ALWAYS_SOFTWARE=1` 可用；關閉視窗後停止繪製 |
| PR-07 | 登出／重登後自動啟動；通知開關；繁中／英文；設定版本遷移；閒置 CPU 量測 |
| PR-08 | 乾淨 Debian 13 安裝／升級／移除；`ldd` 只依賴發行版 Qt；NOTICE 涵蓋 `Cargo.lock` 全部 crate；checksum 與 commit 可追溯 |
