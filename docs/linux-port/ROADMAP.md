# Syrtis Linux — PR Roadmap（v1.1，PR-00 修訂）

**狀態**：Planning。工時為粗估區間，**不是實測**；PR-00 合併後依實際速度重估。每個 PR 的實作、push、開 PR 都需要維護者個別授權，不得自動 merge。

## 相依與工時

| PR | Owner | 內容 | 依賴 | 粗估 |
| --- | --- | --- | --- | ---: |
| PR-00 | Claude（Rust）＋ Codex（Qt spike）＋ Antigravity（審查） | Feasibility 證據、ADR 修訂 | — | 12–24h |
| PR-01 | Claude | Linux 編譯修正、ADR-012 RNG、ADR-013 Antigravity 路徑、C smoke、Linux Rust CI | 00 | 16–32h |
| PR-02 | Codex | Qt shell、C++ Bridge（**先接假 FFI**）、托盤、單實例（FR-01）、Qt CI | 00 | 24–40h |
| PR-03 | Codex（Claude 審數值） | C++ Core 移植 + cross-check harness（ADR-011） | 00 | 40–90h |
| PR-04 | Claude（Rust）＋ Codex（UI） | Quota：Claude／Codex／Antigravity、last-good、帳號隔離、quota 卡 | 01、02、03 | 40–80h |
| PR-05 | Codex | Overview、Models、Daily、Monthly、Hourly、**Stats、Agents**、2D 圖 | 01、02、03 | 50–100h |
| PR-06 | Codex | 3D 貢獻圖（ADR-006 選型） | 05 | 24–50h |
| PR-07 | Codex | 開機啟動、通知、設定、繁中／英文 | 02、04 | 20–40h |
| PR-08 | Claude ＋ Codex | `.deb`、SBOM／NOTICE、release dry-run | 全部 | 16–32h |
| **合計** | | | | **242–488h**（另加 20–30% buffer） |

和 v1.0 規劃的差異：

- 新增 PR-03（C++ Core）；原規劃沒有任何 PR 負責 `Sources/TokenBarCore` 的移植。
- Stats／Agents（FR-06，v0.2）從 3D PR 移到 PR-05。
- 單實例（FR-01，v0.1）從桌面整合 PR 移到 PR-02。
- PR-02 改為先接假 FFI，可與 PR-01 平行。
- CI 隨 PR 逐步加入，不集中在最後一個 PR。

版本對應：v0.1 = PR-01、02、03、04（不含 pace 歷史圖），v0.2 = PR-05，v0.3 = PR-06、07，v1.0 = PR-08。

## 每個 PR 都必須

- 附上實際執行的編譯／測試命令與 exit code；沒有執行的測試不得寫成通過。
- 只使用合成 fixture；不得提交私人路徑、session、憑證、真實用量數字。
- `gh` 操作一律帶 `--repo RichardstGG/syrtis-linux`；不得對 `Nanako0129/syrtis` 開 PR 或留言。
- 不修改 `vendor/tokscale-core`、`Sources/`（macOS app）。
- UI PR 必須附 Plasma Wayland 實機證據；headless CI 不能取代。

## 停止與降階

- **Stop-1**：Rust FFI 無法在 Linux 以合理成本維持 → 維護者裁決替代後端。（PR-00：**未觸發**，一行修正即可建置。）
- **Stop-2**：provider 無法可靠提供 quota → 標示 Partial，由維護者決定 v1.0 定義。
- **Stop-3**：3D 需要 GPL-only 模組或 `qsb` → 禁止引入，改用方案 A／B 或延後。
- **Stop-4**：憑證外洩、跨帳號快取、C++ Core 對拍失敗、不可追溯成本 → 阻止發行。
- **Stop-5**：任一 PR 實際時數超過**該 PR 粗估上限的 30%** → 停止新增功能，提出 scope-cut。
- **Stop-6**：發現 PR 或留言誤送到上游 repo → 立即停止，回報維護者。
