# DouyuMonitor 现代深色 UI 改造 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在不改变播放、房间管理和监控业务逻辑的前提下，将 Qt Quick/QML 主界面统一为现代深色工作台，并让最大化与全屏播放具备明确、不同的图标语义。

**Architecture:** 保留 `Main.qml`、`AppHeader`、`RoomSidebar`、`RoomTile`、`WorkspaceGrid`、Drawer/Popup/Dialog 的现有边界。新增集中式视觉令牌 QML 单例供组件引用；窗口控制图标使用独立 SVG 资源；视觉改造按壳层、管理区、播放区、面板四个阶段推进，每阶段都运行对应的 QML 测试。

**Tech Stack:** Qt 6.8.3, Qt Quick/QML, Qt Quick Controls 2, C++/Qt Test, CMake, CTest, Playwright/截图验证。

---

## 文件范围

**新增：**

- `native/app/qml/Theme.qml`：QML 单例视觉令牌，集中颜色、尺寸、圆角和状态色。
- `native/app/qml/assets/icons/window-maximize.svg`：闭合方框最大化图标。
- `native/app/qml/assets/icons/window-fullscreen.svg`：四角分离全屏图标。
- `native/app/qml/assets/icons/window-minimize.svg`：最小化横线图标。
- `native/app/qml/assets/icons/window-close.svg`：关闭 X 图标。

**修改：**

- `native/app/qml/qmldir`：注册 `Theme.qml` 为单例。
- `native/app/qml/Main.qml`：改用 Theme 令牌，并统一窗口背景与最小尺寸。
- `native/app/qml/components/AppHeader.qml`：深色顶栏、程序图标、工具栏间距和全屏入口。
- `native/app/qml/components/WindowControls.qml`：替换窗口控制资源，确保最大化和全屏使用不同图形与可访问名称。
- `native/app/qml/components/RoomSidebar.qml`：深色管理区、列表层级、标题省略和固定动作区。
- `native/app/qml/components/RoomLibraryView.qml`：历史/收藏复用相同条目视觉规范。
- `native/app/qml/components/RoomTile.qml`：深色播放卡片、底部信息带、操作区宽度约束。
- `native/app/qml/components/WorkspaceGrid.qml`：使用画布令牌，保持自动/主直播间布局算法不变。
- `native/app/qml/components/ToastViewport.qml`：统一 toast 表面和关闭动作。
- `native/app/qml/panels/DanmakuSettingsPanel.qml`：统一 Popup 表面、标签页和分组控件。
- `native/app/qml/panels/MonitoringStatusPanel.qml`：统一 Drawer 表面、指标块和关闭按钮。
- `native/app/qml/panels/WorkspacePresetsPanel.qml`：统一预设列表和删除动作。
- `native/app/qml/dialogs/AddRoomDialog.qml`：统一 Dialog、搜索结果和加入动作。
- `native/app/qml/dialogs/NotificationSettingsDialog.qml`：统一设置行和按钮状态。
- `native/app/qml/dialogs/GroupManagerDialog.qml`：统一分组管理表面和危险动作。
- `native/tests/qml_interaction_test.cpp`：补充窗口控制图标资源、名称和不复用断言。
- `native/tests/qml_visual_smoke_test.cpp`：补充深色令牌和关键区域不重叠断言。
- `native/CMakeLists.txt`：将 Theme、SVG 资源纳入 QML 资源列表。

## Task 1: 建立视觉令牌和资源基线

**Files:**
- Create: `native/app/qml/Theme.qml`
- Create: `native/app/qml/assets/icons/window-maximize.svg`
- Create: `native/app/qml/assets/icons/window-fullscreen.svg`
- Create: `native/app/qml/assets/icons/window-minimize.svg`
- Create: `native/app/qml/assets/icons/window-close.svg`
- Modify: `native/app/qml/qmldir`
- Modify: `native/CMakeLists.txt`

- [ ] **Step 1: 写入 Theme 单例**

`Theme.qml` 使用以下公开属性，组件只引用这些属性，不再散落颜色字面量：

```qml
pragma Singleton
import QtQuick

QtObject {
    readonly property color appBar: "#11151b"
    readonly property color managementSurface: "#1b2028"
    readonly property color canvas: "#171b22"
    readonly property color controlSurface: "#202731"
    readonly property color well: "#12171e"
    readonly property color border: "#303845"
    readonly property color borderStrong: "#344252"
    readonly property color text: "#eef2f7"
    readonly property color mutedText: "#8996a6"
    readonly property color accent: "#f27522"
    readonly property color online: "#50cf91"
    readonly property color warning: "#f0b26c"
    readonly property color danger: "#d78383"
    readonly property int topBarHeight: 52
    readonly property int controlHeight: 32
    readonly property int roomRowHeight: 60
    readonly property int radiusSmall: 6
    readonly property int radiusMedium: 9
    readonly property int radiusLarge: 11
    readonly property int gap: 8
}
```

- [ ] **Step 2: 注册单例**

在 `native/app/qml/qmldir` 增加 `singleton Theme 1.0 Theme.qml`，并在 `native/CMakeLists.txt` 的 QML 资源列表加入 `app/qml/Theme.qml` 与 4 个 SVG。

- [ ] **Step 3: 写入四个独立 SVG**

最大化图标必须是完整闭合方框；全屏图标必须由四组不相连的角线组成；最小化为横线；关闭为两条交叉线。四个文件的 `viewBox` 统一为 `0 0 16 16`。

- [ ] **Step 4: 运行资源加载测试**

Run: `cmake --build --preset windows-x64-release --target qml_engine_smoke_test`

Expected: exit code `0`，QML 单例和 SVG 资源可加载。

- [ ] **Step 5: Commit**

```powershell
git add native/app/qml/Theme.qml native/app/qml/qmldir native/app/qml/assets/icons/window-*.svg native/CMakeLists.txt
git commit -m "feat: add dark UI theme tokens and window icons"
```

## Task 2: 重做顶栏和窗口控制区

**Files:**
- Modify: `native/app/qml/Main.qml`
- Modify: `native/app/qml/components/AppHeader.qml`
- Modify: `native/app/qml/components/WindowControls.qml`
- Test: `native/tests/qml_interaction_test.cpp`

- [ ] **Step 1: 先补充交互回归断言**

在 `qml_interaction_test.cpp` 的窗口控制测试中加入：

```cpp
QObject *windowControls = window->findChild<QObject *>(QStringLiteral("windowControls"));
QVERIFY(windowControls != nullptr);
QObject *maximize = windowControls->findChild<QObject *>(QStringLiteral("maximizeButton"));
QObject *fullscreen = window->findChild<QObject *>(QStringLiteral("fullscreenButton"));
QVERIFY(maximize != nullptr);
QVERIFY(fullscreen != nullptr);
QVERIFY(maximize->property("Accessible.name").toString().contains(QStringLiteral("最大化")));
QCOMPARE(fullscreen->property("Accessible.name").toString(), QStringLiteral("全屏播放"));
QVERIFY(maximize->findChild<QObject *>(QStringLiteral("maximizeIcon")) != fullscreen->findChild<QObject *>(QStringLiteral("fullscreenIcon")));
```

- [ ] **Step 2: 更新 `WindowControls.qml`**

为三个按钮增加 `objectName`：`minimizeButton`、`maximizeButton`、`closeButton`；最大化使用 `window-maximize.svg`，全屏入口仍在 `AppHeader.qml`，使用 `window-fullscreen.svg`。不得让两个按钮引用同一资源。

- [ ] **Step 3: 更新 `AppHeader.qml`**

使用 `Theme.appBar`、`Theme.border`、`Theme.controlHeight`；品牌标记替换为 `../assets/douyu_monitor.svg`；全屏按钮的 `objectName` 保持 `fullscreenButton`，`Accessible.name` 固定为“全屏播放”，tooltip 与图标一致。

- [ ] **Step 4: 更新 `Main.qml`**

将 `canvasColor`、`surfaceColor`、`borderColor`、`accentColor`、文字颜色替换为 Theme 属性；保持 `Qt.FramelessWindowHint`、现有信号和窗口行为不变。

- [ ] **Step 5: 运行窗口交互测试**

Run: `ctest --preset windows-x64-release -R "qml_interaction_test|qml_close_regression_test" --output-on-failure`

Expected: 两个测试全部通过；最大化和全屏行为仍能被触发且名称不同。

- [ ] **Step 6: Commit**

```powershell
git add native/app/qml/Main.qml native/app/qml/components/AppHeader.qml native/app/qml/components/WindowControls.qml native/tests/qml_interaction_test.cpp
git commit -m "feat: clarify dark header and fullscreen controls"
```

## Task 3: 重做房间库和历史/收藏列表

**Files:**
- Modify: `native/app/qml/components/RoomSidebar.qml`
- Modify: `native/app/qml/components/RoomLibraryView.qml`
- Test: `native/tests/qml_visual_smoke_test.cpp`
- Test: `native/tests/qml_interaction_test.cpp`

- [ ] **Step 1: 固定列表条目几何**

每个房间条目固定为 `Theme.roomRowHeight`，文本列使用 `anchors.right: actionBar.left` 和 `Text.ElideRight`；动作区固定宽度，不能被主播名或标题挤压。

- [ ] **Step 2: 统一深色层级**

管理区使用 `Theme.managementSurface`；列表条目使用 `Theme.controlSurface`；选中条目使用 `Theme.accent` 的低明度表面；输入框使用 `Theme.well`；当前/收藏/历史标签页使用同一选中态。

- [ ] **Step 3: 保留业务入口**

不改变 `addRoom`、`setFavorite`、`moveRoom`、`setAudioRoom`、`requestRemoveRoom`、历史打开和删除按钮的调用；仅替换样式和图标资源。

- [ ] **Step 4: 增加长标题视觉回归**

在 `qml_visual_smoke_test.cpp` 中创建长主播名和长标题模型，断言条目矩形与动作栏矩形不相交，且标题文本对象的宽度不超过动作栏左边界。

- [ ] **Step 5: 运行测试**

Run: `ctest --preset windows-x64-release -R "qml_visual_smoke_test|qml_interaction_test" --output-on-failure`

Expected: 视觉和交互测试通过，历史删除与收藏入口仍存在。

- [ ] **Step 6: Commit**

```powershell
git add native/app/qml/components/RoomSidebar.qml native/app/qml/components/RoomLibraryView.qml native/tests/qml_visual_smoke_test.cpp native/tests/qml_interaction_test.cpp
git commit -m "feat: refresh dark room library surfaces"
```

## Task 4: 重做直播卡片和工作区画布表面

**Files:**
- Modify: `native/app/qml/components/RoomTile.qml`
- Modify: `native/app/qml/components/WorkspaceGrid.qml`
- Modify: `native/app/qml/components/PrimaryRoomDivider.qml`
- Test: `native/tests/qml_visual_smoke_test.cpp`

- [ ] **Step 1: 保持现有布局算法**

不得修改 `autoRowCounts`、`primarySecondaryRowCounts`、`primaryZoneWidths`、`primarySideCounts` 和 `geometryFor` 的房间分配逻辑；只替换画布背景、卡片表面和状态层。

- [ ] **Step 2: 统一卡片信息层级**

卡片顶部保留在线/未开播徽标和路数；底部信息列设置最大宽度，动作栏固定宽度；标题使用 `Text.ElideRight`；主画面使用橙色细边和明确标签。

- [ ] **Step 3: 统一操作图标**

继续使用现有 `pin.svg`、`volume-2.svg`、`message-circle.svg`、`ellipsis.svg` 等资源；只调整尺寸、悬浮态和颜色，不改变点击处理。

- [ ] **Step 4: 更新画布和分隔条**

画布使用 `Theme.canvas`，卡片使用 `Theme.controlSurface` 与 `Theme.borderStrong`；主次分隔条使用 `Theme.accent` 的低透明度颜色。

- [ ] **Step 5: 运行布局验证**

Run: `ctest --preset windows-x64-release -R "qml_visual_smoke_test|multi_room_coordinator_test|mpv_quick_item_test" --output-on-failure`

Expected: 1、2、5、8、9 路布局测试通过，卡片无重叠、无黑屏相关回归。

- [ ] **Step 6: Commit**

```powershell
git add native/app/qml/components/RoomTile.qml native/app/qml/components/WorkspaceGrid.qml native/app/qml/components/PrimaryRoomDivider.qml
git commit -m "feat: polish dark multi-room canvas"
```

## Task 5: 统一 Drawer、Popup、Dialog 和 Toast

**Files:**
- Modify: `native/app/qml/components/ToastViewport.qml`
- Modify: `native/app/qml/panels/DanmakuSettingsPanel.qml`
- Modify: `native/app/qml/panels/MonitoringStatusPanel.qml`
- Modify: `native/app/qml/panels/WorkspacePresetsPanel.qml`
- Modify: `native/app/qml/dialogs/AddRoomDialog.qml`
- Modify: `native/app/qml/dialogs/NotificationSettingsDialog.qml`
- Modify: `native/app/qml/dialogs/GroupManagerDialog.qml`
- Test: `native/tests/qml_interaction_test.cpp`

- [ ] **Step 1: 统一容器表面**

所有 Drawer/Popup/Dialog 使用 `Theme.controlSurface`、`Theme.border`、`Theme.radiusLarge` 和统一内边距；标题、辅助说明、内容分组和底部动作保持明确层级。

- [ ] **Step 2: 修复关闭入口一致性**

每个浮层在右上角提供可访问关闭按钮；Toast 的关闭动作继续保留；点击关闭后 `visible` 或 `opened` 状态应立即变为 false。

- [ ] **Step 3: 统一表单和分段控件**

输入框使用 `Theme.well`；选中态使用低明度橙色表面；主要动作使用 `Theme.accent`；危险动作使用 `Theme.danger`，不改变原有信号连接。

- [ ] **Step 4: 运行面板交互测试**

Run: `ctest --preset windows-x64-release -R "qml_interaction_test|qml_close_regression_test|qml_danmaku_overlay_test" --output-on-failure`

Expected: 面板打开、关闭、弹幕切换、通知设置和预设删除测试通过。

- [ ] **Step 5: Commit**

```powershell
git add native/app/qml/components/ToastViewport.qml native/app/qml/panels native/app/qml/dialogs native/tests/qml_interaction_test.cpp
git commit -m "feat: unify dark popup and dialog surfaces"
```

## Task 6: 完整视觉验证和回归检查

**Files:**
- Modify: `native/tests/qml_visual_smoke_test.cpp`（仅在前面新增断言需要调整时）
- Modify: `README.md` 或 `native/README.md`（如需记录新的 UI 入口）

- [ ] **Step 1: 构建 Release**

Run: `cmake --build --preset windows-x64-release --target douyu_monitor_native stream_service -- -j1`

Expected: exit code `0`，无 QML 编译错误。

- [ ] **Step 2: 运行完整 CTest**

Run: `ctest --preset windows-x64-release --output-on-failure`

Expected: 所有已注册测试通过。

- [ ] **Step 3: 运行多分辨率截图**

使用项目现有 Playwright/截图脚本，至少捕获 1280×720、1600×900、1920×1080 三个尺寸；检查顶栏、侧栏、卡片、抽屉、长标题和按钮没有重叠。

- [ ] **Step 4: 检查窗口控制语义**

确认最大化图标是闭合方框，全屏图标是四角分离角标；两个 tooltip 和 `Accessible.name` 分别为“最大化或还原”和“全屏播放”。

- [ ] **Step 5: 检查运行时依赖**

确认 Release 输出没有新增 Electron、Chromium、Node 或 Qt WebEngine 文件。

- [ ] **Step 6: 提交最终验证记录**

```powershell
git status --short
git diff --check
git log --oneline -6
```

Expected: 只有本计划涉及的 UI 文件有改动；无空白冲突标记或格式错误。

## 自检结果

- 规格中的深色管理区、程序图标、最大化/全屏区分、房间库、直播卡片、状态栏、面板和验证要求均已映射到任务。
- 计划没有引入 Electron、Chromium、Qt WebEngine 或业务层重构。
- 现有 9 路布局算法和控制器调用保持不变。
- 所有任务都给出了文件路径、具体修改方向、测试命令和预期结果。
