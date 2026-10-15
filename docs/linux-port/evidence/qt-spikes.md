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

### 執行紀錄（2026-10-15，Qt lane 部分完成）

基線：`docs/linux-feasibility`，commit `092b7da65587f2f41241b858784ba0957cf48f1d`。本次僅執行 B1–B5 暫存 spike；未實作產品、未修改 ADR、未安裝套件、未 commit／push／開 PR。原始碼、binary、完整 stdout／stderr、逐幀 CSV、截圖及 SHA-256 manifest 保留在 repository 外的暫存目錄；本節採文字觀察，不新增圖片檔。

實測環境：Debian 13.7、Linux 6.12.111+deb13-amd64、Qt 6.8.2、KF6 StatusNotifierItem／WindowSystem 6.13.0、LayerShellQt 6.3.4；KDE Wayland，單一輸出，邏輯尺寸 1800×1125、120 Hz，桌面原生縮放 160%。硬體 renderer 為 `Mesa Intel(R) Graphics (LNL)`；Mesa 25.0.7。強制 Mesa 軟體渲染時，實際 renderer 為 `llvmpipe (LLVM 19.1.7, 256 bits)`。

以下 `$SPIKE_DIR` 代表 repository 外的暫存 spike 目錄。所有 B1–B4 正式執行均明設 `QT_QPA_PLATFORM=wayland`，程式回報的平台也均為 `wayland`。B1–B3 使用 Widgets，scene graph backend **不適用**；B4 另列後端。建置、`ldd` 及文件檢查不啟動圖形平台，平台／scene graph backend 不適用。

```bash
cmake -S "$SPIKE_DIR" -B "$SPIKE_DIR/build" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$SPIKE_DIR/build" -j 2
```

Configure exit 0；最初 build exit 1（spike 的 QFlags／connect 呼叫寫法錯誤），修正暫存程式後 build exit 0；DPR 檢查與桌面補驗版本再建置亦 exit 0。未把編譯失敗解讀為 Qt 不支援。Configure 顯示 `KF6DBusAddons_FOUND=0`，另有非致命的 XKB 開發檔偵測警告。

### B1：托盤協定可用，真實點擊矩陣未執行

```bash
QT_QPA_PLATFORM=wayland "$SPIKE_DIR/build/trayqt"
QT_QPA_PLATFORM=wayland "$SPIKE_DIR/build/traykf"
python3 "$SPIKE_DIR/run_trays.py"
```

兩個最小程式皆自動退出，exit 0；補驗 runner exit 0。Watcher 的 `IsStatusNotifierHostRegistered=true`；各程式執行期間均新增一筆 `/StatusNotifierItem` 註冊。下表為補驗的協定查詢與程式事件觀察，**不是人工目視托盤的驗收**。

| 觀察 | QSystemTrayIcon | KStatusNotifierItem |
| --- | --- | --- |
| 圖示 | `isSystemTrayAvailable=true`、圖示非空；匯出 `IconName=applications-science` | 匯出 `IconName=applications-science`、`Status=Active` |
| Tooltip | 匯出標題 `Syrtis Qt tray spike` | 匯出標題 `Syrtis KF tray spike`、副標題 `Temporary verification` |
| 選單 | `/MenuBar` 的 `GetLayout` 含 `Open`、`Close window` | 相同兩個動作，另有隱藏 separator |
| 程式化 Activate | 收到 `QSystemTrayIcon::Trigger`，開窗 | 收到 `activateRequested(true)`，開窗 |
| 呼叫後約 0.8 秒 | `QWindow::isActive=true`、文字輸入框 `hasFocus=true` | 同左；`providedToken()` 為空 |
| 程式化 ContextMenu | D-Bus 回覆 exit 0 | D-Bus 回覆 exit 0 |

每個 `Properties.Get`、修正後的 `GetLayout`、`Activate(0,0)` 與 `ContextMenu(0,0)` 均 exit 0。呼叫模式如下，service 名稱由 watcher 當次註冊取得，不硬編碼 session bus ID：

```bash
gdbus call --session --dest "$SERVICE" --object-path /StatusNotifierItem --method org.freedesktop.DBus.Properties.Get -- org.kde.StatusNotifierItem ToolTip
gdbus call --session --dest "$SERVICE" --object-path /MenuBar --method com.canonical.dbusmenu.GetLayout -- 0 -1 '[]'
gdbus call --session --dest "$SERVICE" --object-path /StatusNotifierItem --method org.kde.StatusNotifierItem.Activate -- 0 0
gdbus call --session --dest "$SERVICE" --object-path /StatusNotifierItem --method org.kde.StatusNotifierItem.ContextMenu -- 0 0
```

初次 `GetLayout` 漏加 `--`，`-1` 被當成 CLI 選項，兩次均 exit 1；補驗保留該失敗紀錄並以正確命令成功。所有啟動移除繼承的 `XDG_ACTIVATION_TOKEN`／`DESKTOP_STARTUP_ID`。這只能證明本次無輸入 serial／token 的程式化路徑能開窗並取得焦點；**真實左鍵及右鍵點擊、鍵盤輸入、tooltip／圖示／選單目視、Plasma 面板上／下／左／右四個位置：未執行**。不能據此判定真實托盤點擊的 xdg-activation 已通過，也不據此選定托盤後端。

### B2：單實例

```bash
QT_QPA_PLATFORM=wayland "$SPIKE_DIR/build/local"  # primary
QT_QPA_PLATFORM=wayland "$SPIKE_DIR/build/local"  # secondary
python3 "$SPIKE_DIR/run_local_focus.py"          # 補驗已顯示視窗的焦點競爭
```

`QLocalServer` 首次 listen 成功；第二個程序透過 `QLocalSocket` 發出 `activate`，primary 收到一次請求並回覆 `received`，secondary 不另開視窗。初次隱藏視窗情境：primary／secondary exit 0，primary 開窗後 `isActive=true`、輸入框 `hasFocus=true`。

已顯示視窗的補驗：primary 先顯示約 15 秒，再啟動不同 application name 的競爭視窗，等 1.5 秒後啟動 secondary。primary 收到請求前 `isActive=false`，呼叫 `raise()`／`activateWindow()` 後約 1 秒仍為 `isActive=false`、`hasFocus=false`；競爭視窗維持焦點，直到競爭程序自動退出後 primary 才再次回報 active=true。primary／secondary／競爭程序均 exit 0，IPC 成功不代表聚焦成功。所有程序均移除繼承的 activation token。這次觀察符合焦點取得受限制的現象，沒有診斷證據可將原因唯一歸因於特定 KWin 設定。

補驗程式的建置命令如下，exit 0；它使用相同 `QLocalServer`／`QLocalSocket` 路徑，沒有新增產品元件：

```bash
c++ -O2 -std=c++20 "$SPIKE_DIR/local-focus.cpp" -o "$SPIKE_DIR/local-focus" $(pkg-config --cflags --libs Qt6Widgets Qt6Network)
```

KF6 `KDBusService(Unique)` 的 **runtime：未執行**。`dpkg-query -W libkf6dbusaddons-dev` exit 1，未安裝開發套件；CMake 查找未找到，以下最小編譯探測亦 exit 1，診斷為 `KDBusService: No such file or directory`，未安裝套件補齊：

```bash
c++ -std=c++20 $(pkg-config --cflags Qt6Widgets) -fsyntax-only "$SPIKE_DIR/kdbus.cpp"
```

目前 KWin `FocusStealingPreventionLevel`／`FocusPolicy` 設定鍵唯讀查詢均為未明設，沒有變更設定。**不同 focus-stealing prevention 等級、Desktop Entry／桌面啟動器傳遞 activation token 的第二次啟動：未執行**；本節不保證其他設定下能取得焦點。

### B3：一般視窗與 LayerShellQt

```bash
QT_QPA_PLATFORM=wayland WAYLAND_DEBUG=client "$SPIKE_DIR/build/plain"
QT_QPA_PLATFORM=wayland WAYLAND_DEBUG=client "$SPIKE_DIR/build/layer"
QT_QPA_PLATFORM=wayland WAYLAND_DEBUG=client "$SPIKE_DIR/build/layer" --reapply-layer
python3 "$SPIKE_DIR/run_kwin.py"
```

觀察 helper 以 `c++ -O2 -std=c++20 "$SPIKE_DIR/recorder.cpp" -o "$SPIKE_DIR/recorder" $(pkg-config --cflags --libs Qt6Core Qt6DBus)` 建置，exit 0。上述程式均 exit 0；補驗 runner exit 0。暫存 KWin 觀察腳本只回傳 spike 視窗資訊；`loadScript`、`run`、`unloadScript` 均 exit 0，卸載回覆 `true`，未變更桌面配置。觀察 helper 完成後由 runner 終止，exit -15（SIGTERM，預期清理）。使用的介面見 [KWin scripting API](https://develop.kde.org/docs/plasma/kwin/api/)。

- 一般視窗：Wayland trace 確認 `xdg_surface.get_toplevel`。KWin `frameGeometry` 在首次開啟與同程序 `close()`→`show()` 後均為 `(688,446,422.5,188.75)`，client 尺寸 420×160；這是沒有人工移動視窗的同程序結果，不能當成跨程序位置持久化。
- Qt client 回報的位置始終為 `(0,0)`，**沒有**把它當成 compositor 的實際位置。一般視窗的上述座標來自 KWin 觀察腳本。
- LayerShellQt：首次顯示的 trace 確認 `get_layer_surface`，layer=top、anchor=top|right（9）、margin top/right 各 20、exclusive zone=0、keyboard interactivity=on-demand（2）；compositor 回覆 `configure(420,160)` 並收到 ack。其實際絕對座標未由觀察腳本取得；**首次錨定位置目視／絕對座標驗證：未執行**，不能只以 anchor 請求值宣稱定位正確。
- 該 QWidget spike 在 `close()`→`show()` 後，trace 改為 `xdg_surface.get_toplevel`；補測在重開前再次取得 LayerShellQt 附加物件並套用參數，仍觀察到一般 toplevel。KWin 也將重開後的視窗辨識為 `normalWindow=true`，位置與一般視窗相同。這是目前 spike 的生命週期結果，未證明所有 LayerShellQt 使用方式皆失敗；保留錨定的關閉重開方案尚未驗證。

本機只枚舉到一個輸出。**多螢幕移動、拔插、不同縮放輸出間切換、人工移動後重開、跨程序重啟的位置記憶：未執行**。

### B4：53×7 方塊與 fallback 實測

共同資料為 371 格，cell 1.0、gap 0.15，高度 `0.15 + 3.0·frac^0.6`，固定合成強度 `frac=((cellIndex*37)%101)/100`，色階 `#BFDBFE→#1E3A8A`。CPU 正交 orbit 投影、每格六個面依平均相機深度排序；A 用抗鋸齒 `QQuickPaintedItem`，B 用三角形 `QSGGeometryNode`＋內建 `QSGVertexColorMaterial`，未使用自訂 shader／qsb。此 spike 未做背面剔除，不能將其結果解讀為所有 A 實作的效能上限。

相機預設 azimuth=π/4、elevation=`atan2(0.45,sqrt(2)*0.7)`、scale=26、target=(0,0,0)，orbit 速度 0.007 rad／邏輯像素、elevation 限制 ±89°；以 `tokenbar.orbit.v1` 六欄 CSV 表達相機值，僅做記憶體內編碼檢查，未寫使用者設定。**持久化還原、跨平台相機互通、pan／zoom 互動：未執行**。

每組先暖機 1 秒，再以 8 ms precise timer 經 `QMouseEvent` 注入拖曳約 10 秒，路徑為中心點加 `(100·sin(0.8t),25·sin(1.1t))`，不與其他 spike 同時執行。視窗／item 固定 800×360 邏輯像素；顯示後檢查實際 DPR=1 或 2，截圖分別為 800×360、1600×720。**真人拖曳與整個 compositor 輸入鏈延遲：未執行**。

```bash
python3 "$SPIKE_DIR/run_graphs.py"
# runner 對 A 的六組逐一執行；遇 p95 >= 16 ms 後，再執行 B 的同一矩陣。
# 以下為其中一組的完整環境模式；DPR 2 改 QT_SCALE_FACTOR=1.25、SPIKE_EXPECTED_DPR=2。
env -u QT_QUICK_BACKEND -u LIBGL_ALWAYS_SOFTWARE -u QT_SCREEN_SCALE_FACTORS -u XDG_ACTIVATION_TOKEN -u DESKTOP_STARTUP_ID QT_QPA_PLATFORM=wayland QSG_RHI_BACKEND=opengl QSG_RENDER_LOOP=basic QSG_INFO=1 QT_SCALE_FACTOR=0.625 SPIKE_EXPECTED_DPR=1 XDG_CACHE_HOME="$SPIKE_DIR/cache" SPIKE_OUTPUT="$SPIKE_DIR/A-hardware-dpr1" "$SPIKE_DIR/build/graph"
# software：加 QT_QUICK_BACKEND=software；llvmpipe：加 LIBGL_ALWAYS_SOFTWARE=1。
# 方案 B：同一命令加 --geometry；SPIKE_OUTPUT 指向對應的已建立暫存子目錄。
```

Runner exit 0；以下十二組程式 exit 均為 0。縮放因子 0.625／1.25 是針對本次 160% 桌面的補償，**以曝光後的 `devicePixelRatio` 和截圖尺寸為準**，不是可套用到所有桌面的通用設定。

後端確認：hardware 為 RHI/OpenGL、Intel renderer；software 為 Qt scene graph Software（`graphicsApi=Software`）；llvmpipe 為 RHI/OpenGL、上述 LLVM renderer。全部採 `QSG_RENDER_LOOP=basic`；其他 render loop **未執行**。

主指標為取樣期間連續 `frameSwapped` 的單調時鐘間隔，包含排程／垂直同步；p50／p95 使用 nearest-rank，不剔除慢幀。另列 `beforeSynchronizing`→`afterFrameEnd` 的 CPU frame span，該值不含全部 GUI 投影工作，也不是 GPU timestamp 或實際螢幕呈現延遲。每組單次量測，非多輪穩定性保證；逐幀原始間隔保留為 CSV。

| 方案 | 模式 | 實際 DPR | 間隔樣本數 | 間隔 p50 ms | 間隔 p95 ms | CPU span p50／p95 ms | 拖曳 move 數 | 觀察 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| A | hardware | 1x | 1174 | 8.356 | 9.886 | 3.275／4.065 | 1246 | p95 < 16 ms |
| A | hardware | 2x | 507 | 17.759 | 26.888 | 14.353／18.348 | 733 | p95 ≥ 16 ms |
| A | software | 1x | 1179 | 8.319 | 9.678 | 2.672／3.360 | 1247 | p95 < 16 ms |
| A | software | 2x | 1144 | 8.354 | 11.868 | 4.468／6.060 | 1233 | p95 < 16 ms |
| A | llvmpipe | 1x | 1137 | 8.408 | 12.904 | 3.904／5.910 | 1249 | p95 < 16 ms |
| A | llvmpipe | 2x | 426 | 23.710 | 31.586 | 17.142／25.884 | 496 | p95 ≥ 16 ms |
| B | hardware | 1x | 1195 | 8.318 | 9.387 | 0.409／0.648 | 1249 | p95 < 16 ms |
| B | hardware | 2x | 1177 | 8.334 | 10.806 | 0.584／2.927 | 1247 | p95 < 16 ms |
| B | software | 1x | 1191 | 8.339 | 9.923 | 0.215／0.608 | 1247 | 未繪出，不合格 |
| B | software | 2x | 1197 | 8.333 | 9.890 | 0.200／0.897 | 1249 | 未繪出，不合格 |
| B | llvmpipe | 1x | 1126 | 8.476 | 13.591 | 2.927／6.911 | 1223 | p95 < 16 ms |
| B | llvmpipe | 2x | 1123 | 8.392 | 13.447 | 3.039／6.731 | 1225 | p95 < 16 ms |

實際取樣長度 9,998–10,009 ms；負載較高的 A 2x 少收到 timer／move 事件，未刪除其慢幀。A 的 hardware／llvmpipe 2x 超過 16 ms，依規格觸發 B 補測；不能將 CPU span 較短當成整體達標。

命中：固定亂數 seed `604007` 打亂 371 格，取前 20 個在 viewport 內且未被遮住的格子，以頂面中心測試 polygon hit；獨立 oracle 使用 3D ray／AABB slab intersection，不重用 painter 排序。每組在拖曳前後各測 20 格，十二組均各 40／40 一致，viewport 外負向檢查回傳 -1。這不是對被遮住格子、所有角度或側面點擊的全面驗收；**真人點擊：未執行**。B software 的 CPU 命中成功不代表畫面成功。

文字／截圖觀察：A 六組均有方塊，B hardware／llvmpipe 四組亦有方塊；已檢視代表性截圖。**B software 1x、2x 的整張截圖均只有單一背景色 `#101820`（各 1 種 RGB 值），沒有方塊**，所以表中約 9.9 ms 的空白畫面不算通過；A software 對應截圖分別有 3,293／5,339 種 RGB 值。截圖不含其他應用程式內容。以上只能支持各測試路徑的觀察，尚未選定產品 renderer，也未修改 ADR-006。

預跑紀錄另存且不納入上表：第一支較大視窗被 KWin 縮小；以 `QT_SCREEN_SCALE_FACTORS=1/2` 標記的預跑，實際 buffer 比例為 1.6／3.2，並非要求的 1x／2x，因此依尺寸而非效能數字判定不符合矩陣，停止 runner（SIGTERM）。另一次 DPR 校正探測與 B3 視窗同時執行，僅用於確認實際 DPR=1，不作效能證據。正式矩陣全部重新執行，未挑選預跑中的較佳數字。

### B5：實際動態連結 Qt／KF6 模組 BOM

```bash
ldd "$SPIKE_DIR/build/trayqt"
ldd "$SPIKE_DIR/build/traykf"
ldd "$SPIKE_DIR/build/local"
ldd "$SPIKE_DIR/build/plain"
ldd "$SPIKE_DIR/build/layer"
ldd "$SPIKE_DIR/build/graph"
ldd "$SPIKE_DIR/local-focus"
ldd "$SPIKE_DIR/recorder"
dpkg-query -W libqt6core6t64 libqt6quick6 libqt6waylandclient6 libkf6statusnotifieritem6 libkf6windowsystem6 liblayershellqtinterface6
```

上述 `ldd`／版本查詢均 exit 0，無 `not found`；表中列出 union，含 `ldd` 遞迴顯示的間接依賴。B1–B3 均為 Widgets spike，因此有 Widgets；此結果不表示產品主要 UI 應使用 Widgets。

| 實際 SONAME（版本） | 出現於 spike | 模組授權依據 |
| --- | --- | --- |
| `libQt6Core.so.6`（6.8.2） | 全部 | LGPL-3.0 路徑可用 |
| `libQt6Gui.so.6`（6.8.2） | 全部圖形程式 | LGPL-3.0 路徑可用 |
| `libQt6Widgets.so.6`（6.8.2） | B1、B2、B3、local-focus | LGPL-3.0 路徑可用 |
| `libQt6DBus.so.6`（6.8.2） | CMake 六個程式、local-focus 間接依賴、recorder | LGPL-3.0 路徑可用 |
| `libQt6Network.so.6`（6.8.2） | B2 local／local-focus、B4 | LGPL-3.0 路徑可用 |
| `libQt6OpenGL.so.6`（6.8.2） | B4 | LGPL-3.0 路徑可用 |
| `libQt6Quick.so.6`（6.8.2） | B4 | LGPL-3.0 路徑可用 |
| `libQt6Qml.so.6`（6.8.2） | B4 | LGPL-3.0 路徑可用 |
| `libQt6QmlMeta.so.6`（6.8.2） | B4，Qt Declarative runtime | LGPL-3.0 路徑可用 |
| `libQt6QmlModels.so.6`（6.8.2） | B4 | LGPL-3.0 路徑可用 |
| `libQt6QmlWorkerScript.so.6`（6.8.2） | B4 | LGPL-3.0 路徑可用 |
| `libQt6WaylandClient.so.6`（6.8.2） | B3 layer 的 `ldd` | LGPL-3.0 路徑可用 |
| `libKF6StatusNotifierItem.so.6`（6.13.0） | B1 traykf | LGPL-2.0-or-later |
| `libKF6WindowSystem.so.6`（6.13.0） | B1 traykf 間接依賴 | 主體 LGPL-2.1-or-later；部分檔案 LGPL-2.0-or-later |
| `libLayerShellQtInterface.so.6`（6.3.4） | B3 layer | Debian 主體標記 `LGPL-3+KDEeV`；安裝標頭列 `LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL` |

Qt 授權查閱 [Qt 6.8 licensing](https://doc.qt.io/qt-6.8/licensing.html)，並比對已安裝套件的 `/usr/share/doc/<package>/copyright`（qtbase／qtdeclarative／qtwayland）；KF6／LayerShellQt 亦讀取對應套件 copyright 及安裝標頭。表內 Qt 欄列可用的 LGPL 路徑，非省略第三方授權的完整 SPDX 表達式。`ldd` 未見 Charts、Graphs、Quick 3D、Network Authorization、Virtual Keyboard；spike 未宣告這些依賴、未定義自訂 shader、未執行 qsb。**dlopen 外掛、所有非 Qt／KF6 依賴與嵌入第三方程式碼的完整發行 SBOM：未執行**，此表不能取代發行查核。

量測 binary SHA-256（完整 manifest 另含觀察 helper）：

| Binary | SHA-256 |
| --- | --- |
| `trayqt` | `6897b69618ed29a4761c1719d33fecc1d9a062d966ccd573a612aa803dab2c29` |
| `traykf` | `730526f45dc936950b4f2ce750a8b6c6f95231128ff7885083b31d4a17f9bf0b` |
| `local` | `a67ce13f02d0a22e11ec272a65d066d8f5a36d3aac8a4f00b1cb69e38d5cfdf2` |
| `plain` | `6d737818425144c6bb8cb2ad326a4fd9a7cca014bc97fbfc1f5810bdc2aca051` |
| `layer` | `5d10732c1bb62de7f44ca2079c1290c18b04f44575145e6ac05d41c15846a8d3` |
| `graph` | `bd792d81a4c4722932fe08c392b807b3f2aaeb79ef7c0ac357945d54bac40526` |
| `local-focus` | `c9161e5f731c943d0bc103c7d1b45de73bb0d6392b87974a126e454ff5abff91` |

### 本次驗證界限

B1 真實點擊／面板位置矩陣、B2 KDBusService runtime／activation token 路徑、B3 多螢幕與 layer-shell 絕對定位仍為 **未執行**；B4 A 的部分 2x 路徑超過門檻，B 的 Qt software 路徑未繪出。**Qt lane 尚未全數驗收通過**。本節保留已執行證據與缺口，不更新 ADR 決策、不新增產品程式碼。
