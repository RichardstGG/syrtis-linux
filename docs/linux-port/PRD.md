# Syrtis Linux Qt6 — Product Requirements Document (PRD)

**文件版本**：v1.1.0 / 2026-10-15（PR-00 修訂；變更見文末「修訂紀錄」）

**階段**：PR-00 Feasibility / Implementation Not Authorized

**目標**：在 Debian 13 + KDE Plasma 6 + Wayland 下，提供 Syrtis 主要 Token/成本/額度監控和桌面 GUI 功能。

## 1. 問題與目標

Syrtis 目前的原生 AppKit/SwiftUI UI 僅支援 macOS；使用者已能在 Debian 13 上使用 Tokscale CLI，但希望擁有常駐 KDE System Tray 的多 Agent 儀表板，而非反覆啟動 TUI。優先沿用 Syrtis／tokscale-core 已成熟的解析與計價邏輯，不重寫計價引擎。

**成功定義**：一個可獨立安裝與移除的 MIT 應用，無需註冊本產品服務；能安全讀取本機來源、顯示準確的 Claude Code／Codex／Antigravity（有能力時）報表與可確認的 Quota，並提供 2D / 3D 活動視覺化。

## 2. 平台範圍

| 分類 | v1.0 需求 |
| --- | --- |
| 首要平台 | Debian 13 (Trixie) / x86_64 / KDE Plasma 6 / Wayland |
| 次要測試 | KDE X11；其他發行版不宣告正式支援 |
| 核心 | 既有 `vendor/tokscale-core` pinned gitlink；既有 `crates/tb_core_ffi` 優先重用 |
| UI | C++20、Qt 6.8 系列、Qt Quick/QML、原生 Qt System Tray |
| 發行 | Debian `.deb`，依賴發行版 Qt 動態程式庫，不綑綁不必要 Qt runtime |
| 介面語言 | 英文、繁體中文 |
| 資料保存 | XDG config/cache/state 路徑，不更改 Claude/Codex/Antigravity 原始檔 |

## 3. 功能需求（FR）

| ID | 功能 | 驗收語意 | 目標 |
| --- | --- | --- | --- |
| FR-01 | 單實例常駐 System Tray | 登入後可選擇常駐；重複啟動時聚焦已存在視窗；KDE Wayland 實測 | v0.1 |
| FR-02 | Overview 卡片 | 今天與累計 Token、模型用量、成本、快取 token、來源；有缺漏明確顯示 | v0.1 |
| FR-03 | 來源 | Claude Code、Codex、Antigravity；每個來源可分別停用/篩選 | v0.1 |
| FR-04 | 配額 | 若 Provider 可驗證，顯示 5H/Weekly/剩餘比例/重置時間；失效不可假裝即時 | v0.1 |
| FR-05 | 歷史頁面 | Models、Daily、Monthly、Hourly；日期/模型/Client 篩選 | v0.2 |
| FR-06 | 延伸頁面 | Stats、Agents；顯示活躍天數、連續使用、Agent 分組 | v0.2 |
| FR-07 | 圖表 | 2D 時間趨勢、年度貢獻熱圖、每日 Tooltip/詳細資訊 | v0.2 |
| FR-08 | 3D 年度貢獻圖 | 可旋轉/縮放/重設視角；圖表點選日期；保留無 3D 降級路徑 | v0.3 |
| FR-09 | 桌面整合 | 開機啟動、KDE 通知、輕/暗主題、高 DPI、設定儲存 | v0.3 |
| FR-10 | 打包與發布 | `.deb` 完整安裝/升級/解除安裝；授權、版本、來源記錄齊全 | v1.0 |

### 3.1 Antigravity 功能邊界

Antigravity quota（供應商報告的剩餘額度）與 Token history（可歸屬於訊息的 Token 數量）**不是同一資料來源**。Syrtis 目前 Rust 模組部分路徑依賴 macOS 的 `agy` 與 Keychain；Linux 首版必須實測運作中的 IDE Language Server、本機認證來源與可取得的項目。無法驗證的值應呈現「不可取得／需要登入／不支援」，不可合成假資料。P0 需決定每項功能的 Supported、Partial、Blocked 狀態。

**PR-00 決議（維護者 2026-10-15）**：Antigravity IDE 的 Token history 接受依賴外部 `tokscale antigravity sync` 產生的 `~/.config/tokscale/antigravity-cache/sessions`；未執行 sync 時顯示「需要同步」空狀態而非 0。`antigravity-cli` lane 直接讀 `$GEMINI_CLI_HOME/antigravity-cli/conversations`。Antigravity quota 僅支援 Language Server（IDE 執行中）與 `~/.gemini/oauth_creds.json` 兩條路徑；macOS `agy`/Keychain 路徑與 capture/binding 系列 C ABI 在 Linux 不暴露。

### 3.2 成本語意

Token 費用是依模型單價的估算（estimated API-equivalent cost），**不代表 ChatGPT Plus 或 Claude Pro 訂閱的實際扣款**。模型單價來源與更新時間必須可追溯；無可靠報價時標記未知，不能猜測。前端不得重新定價或在混合資料後倒推單個 Client 用量。

### 3.3 額度語意

每張配額卡標示 Provider、帳號（可遮蔽）、時間窗、資料新鮮度、取得錯誤。切換帳號後不延用其他帳號快取。暫時錯誤時僅允許在同一驗證帳號範圍內顯示明確過期的 last-good 值。使用者可停用配額網路請求。

## 4. 使用者互動情境

**UC-A：登入後監控**。開機啟動僅存在一個實例；Tray 顯示選定的 Claude/Codex 配額狀態；點擊開啟完整儀表板；關閉視窗後仍常駐；退出後無殘留程式。

**UC-B：查看歷史**。選擇 2026 年 9/9～10/9；各 Client 模型報表總和與 Rust 報表一致；跨午夜歸屬依系統本地時區處理（測試與 cross-check fixture 固定 `TZ=Asia/Taipei`，與上游 harness 一致）；可輸出/複製非敏感彙整內容。

**UC-C：供應商失效**。網路中斷、登入過期、HTTP 429 時，UI 展示來源、錯誤和最後更新時間；不顯示虛假的 0% 或完全可用 100%。

**UC-D：Antigravity IDE 開啟**。在 IDE 已登入且 Language Server 可用時檢查 quota；關閉 IDE 後只依經驗證的 Linux fallback 處理。

## 5. 非功能需求（NFR）

| ID | 要求 | 測試方式 |
| --- | --- | --- |
| NFR-01 隱私 | 預設本機統計，無產品遙測、不自動將 Session 發布到雲端。已知非遙測 egress：定價資料（LiteLLM／OpenRouter 價目表）與使用者已登入 provider 的 quota API；兩者皆須可在設定中停用 | 網路流量測試、檢查 log 路徑及服務清單 |
| NFR-02 安全 | 憑證只從受支援的既有來源讀取；不在 log/PR/argv 暴露 Token；不改寫其他 CLI 登入資料 | 合成 credential fixture、安全審查、權限測試 |
| NFR-03 正確性 | 原始 Token/成本聚合由 Rust 產生；相同 fixture 的 Windows/macOS/Linux payload 主要欄位一致 | Golden fixture + parity 測試 |
| NFR-04 響應 | UI 執行緒不得進行阻塞 FFI／網路／大檔掃描；背景請求可取消 | profiling、關閉視窗及錯誤注入 |
| NFR-05 可靠性 | UI 關閉/重開、斷線、切換帳號、多螢幕時不能崩潰或混資料 | KDE 真機矩陣 |
| NFR-06 效能 | PR-00 建立測試機與語料基線；初期目標 warm popup <1 秒、閒置 CPU <2%，不得聲稱已達標直到量測 | 指定樣本、P50/P95、冷熱快取 |
| NFR-07 授權 | 應用自有程式碼 MIT；不得引入只提供 GPLv3/商業授權的 Qt 模組；對 LGPL 動態連結提供聲明 | SBOM / cargo license / Qt 模組審核 |
| NFR-08 可維護 | 文件先行、固定核心 gitlink、跨語言契約測試、每 PR 最小變更 | CI 與 Review |
| NFR-09 無障礙 | 鍵盤導覽、可見焦點、不同縮放下可讀、圖表替代文字資訊 | QML 手動 UI 測試 |

## 6. 明確不納入 v1.0

完整 macOS Liquid Glass 1:1、macOS Sparkle、任意長字串直接顯示在 Plasma 工作列、Plasma 小工具、Discord Rich Presence、多帳號進階同步、Windows 版發布、完整跨發行版發行，以及 Tokscale leaderboard 自動上傳。

## 7. 版本與 Exit Criteria

- **v0.1**：Rust FFI/Qt Shell/三個 Client 檢查與可信 Quota；其中不支援者必須清楚標示。
- **v0.2**：完整歷史頁面與 2D 圖表；篩選與數值對拍。
- **v0.3**：可操作 3D、設定、通知、Wayland 可靠性。
- **v1.0.0**：可重現 `.deb`、CI 測試、第三方授權清單、繁中/英文、文件與真機驗收記錄。任何關鍵阻斷項未解決不得宣稱功能完整。

## 8. 工程估算與關卡

PR-00～PR-08 工程投入合計 **242～488 小時（粗估，非實測；依 `Sources/TokenBarCore` 8,180 行 Swift 與 Windows C# Core 15,944 行的移植規模推估）**；另保留 20～30% 風險緩衝。原 114～247h 估算未計入 C++ Core 移植與 cross-check，已作廢。這是累計人機開發、整合、測試時間，不是 API Token 用量，也不是必須由人手動輸入這些時間。

若 Rust FFI 在 Linux 無法可行地編譯，或實際需要的三個 Agent 不具備可用資料來源，P0 應暫停進入大型 UI 開發，提交選項分析，由使用者重新決策。

## 9. 來源與證據

- https://github.com/Nanako0129/syrtis/blob/main/docs/knowledge/architecture.md
- https://github.com/Nanako0129/syrtis/blob/main/crates/tb_core_ffi/src/agent_antigravity.rs
- https://github.com/Nanako0129/syrtis/blob/main/Sources/CTB/include/ctb.h
- https://doc.qt.io/qt-6.8/licensing.html
- https://docs.github.com/en/pull-requests/reference/forks

## 10. 修訂紀錄

| 版本 | 日期 | 變更 |
| --- | --- | --- |
| v1.0.0 | 2026-10-14 | 初版規劃 |
| v1.1.0 | 2026-10-15 | PR-00：Antigravity Token／quota 範圍決議；時區語意改為系統本地時區；列出非遙測 egress；工時重估（見 [`ROADMAP.md`](ROADMAP.md)） |
