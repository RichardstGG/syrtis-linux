# Syrtis Linux port

這是 [Syrtis](https://github.com/Nanako0129/syrtis) 的**非官方** Linux（Debian 13／KDE Plasma 6／Wayland）移植，使用 Qt 6.8／QML 與 C++20，資料核心沿用 Syrtis 的 Rust C ABI 與 pinned `tokscale-core`。上游的 macOS app、`docs/knowledge/` 與 `AGENTS.md` 保持不動；Linux 專屬內容只放在 `docs/linux-port/` 與（之後的）`linux/`。

**目前階段**：PR-00 Feasibility。還沒有任何 Linux 產品程式碼。

| 文件 | 內容 |
| --- | --- |
| [`PRD.md`](PRD.md) | 產品規格、功能／非功能需求 |
| [`ADR.md`](ADR.md) | 架構與決策紀錄（ADR-001～014）、風險表 |
| [`ROADMAP.md`](ROADMAP.md) | PR-00～PR-08 相依、工時、停止條件 |
| [`TEST-MATRIX.md`](TEST-MATRIX.md) | 驗收測試與 Stop／Go |
| [`evidence/rust-feasibility.md`](evidence/rust-feasibility.md) | PR-00 Rust／C ABI 實測紀錄 |
| [`evidence/qt-spikes.md`](evidence/qt-spikes.md) | PR-00 Qt／Wayland spike（待執行） |

## PR-00 重點結論

1. 基線在 Linux 無法編譯：一個非 macOS stub 的簽名和 macOS 版不一致。修正一行後 release build、2,383 項測試、clippy、C smoke 全部通過。
2. **Linux 上 quota 完全不可用**：帳號範圍金鑰沒有亂數來源（ADR-012）。這項修正排在 PR-01。
3. Claude、Codex 的本機 Token 資料可以讀到；Antigravity 的 Token 在 `tokscale antigravity sync` 之後可以讀到。
4. 原規劃沒有包含 `TokenBarCore` 的 C++ 移植（ADR-011），工時重估為 242–488h。

## 協作規則

- 上游 remote 只用來 fetch；PR 和 issue 只開在本 fork。
- 公開文件不得包含私人路徑、憑證、session 內容或真實用量數字。
- 每個 PR 的實作、push、merge、release 都需要維護者個別授權。
