# PR-00 Evidence — Qt / Plasma Wayland spikes

**狀態**：⏳ **待 Codex 執行**（Qt lane）。本檔目前只有規格，沒有任何結果；在填入實測紀錄之前，不得引用為已驗證。

## 環境（Claude Code 於 2026-10-15 查詢，未編譯任何 Qt 程式）

Debian 13.7、Plasma 6.3.6、Wayland；已安裝 `qt6-base-dev`／`qt6-declarative-dev` 6.8.2、`qt6-tools-dev`、`libkf6statusnotifieritem-dev` 6.13、`liblayershellqtinterface-dev` 6.3.4、`ninja-build`、`pkg-config`。

## 規則

- spike 原始碼放在 repository **外**的暫存目錄；只把結果寫進本檔。
- 不連結 Qt Charts／Graphs／Quick 3D／Network Authorization／Virtual Keyboard；不使用自訂 shader（`qsb` 為 GPLv3）。
- 每項都要記錄命令、exit code、Qt 平台外掛（`QT_QPA_PLATFORM`）、scene graph backend，並附截圖或文字觀察。

## B1 托盤

`QSystemTrayIcon` 與 `KStatusNotifierItem` 各做一個最小程式。記錄：圖示、tooltip、左鍵開窗、右鍵選單；**開出的視窗是否取得鍵盤焦點**（xdg-activation）；面板放在上／下／左／右四個位置的表現。

## B2 單實例

`QLocalServer` 與 KF6 `KDBusService(Unique)` 兩種做法：第二次啟動時能否聚焦既有視窗、是否會被 focus-stealing prevention 擋下。

## B3 視窗模式

一般 xdg-toplevel 視窗與 LayerShellQt 錨定面板兩種：記錄 KWin 的放置位置、關閉後再開是否記得位置、多螢幕行為。

## B4 3D 方案 A

53×7 方塊，CPU 正交 orbit 投影 + painter's algorithm 排序 + `QQuickPaintedItem`；參數見 ADR-006。在硬體加速、`QT_QUICK_BACKEND=software`、`LIBGL_ALWAYS_SOFTWARE=1` 三種環境，以 1x 和 2x 縮放各拖曳 10 秒，記錄 frame time 的 p50／p95；隨機抽 20 格做命中測試。若 p95 ≥ 16 ms，改測方案 B（`QSGGeometryNode` + `QSGVertexColorMaterial`）。

## B5 Qt 模組 BOM

以 `ldd` 列出 spike 實際連結的 Qt／KF6 程式庫，並記錄各自的授權。

## 結果

（待填）
