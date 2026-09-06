# Full QML Visual Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn the actual Qt Quick application into the approved modern dark multi-room monitor UI, including true empty and occupied workspace states, without changing playback or room-management behavior.

**Architecture:** Keep the existing C++ `AppController`, `RoomListModel`, `WorkspaceModel`, `MonitoringModel`, playback, and danmaku interfaces. Extend the QML shell with one reusable status-bar component, then update the header, library, canvas, tiles, and overlays to use `Theme` tokens and stable object names. Move visual regression tests onto the same `DouyuMonitor/Main` module path used by the released executable.

**Tech Stack:** Qt 6.8.3, Qt Quick/QML, Qt Test, CMake/CTest, native QQuickWindow screenshots.

---

## File structure

- `native/app/qml/Theme.qml`: visual tokens shared by all QML components.
- `native/app/qml/Main.qml`: root layout, status-bar bindings, empty-workspace state, and QML composition.
- `native/app/qml/components/WorkspaceStatusBar.qml`: new compact global status surface.
- `native/app/qml/components/AppHeader.qml`: grouped top-bar controls and sound popover surface.
- `native/app/qml/components/WindowControls.qml`: window-control geometry and semantic icon usage.
- `native/app/qml/components/RoomSidebar.qml`: room-library shell and current-room rows.
- `native/app/qml/components/RoomLibraryView.qml`: history and favorites rows.
- `native/app/qml/components/WorkspaceGrid.qml`: canvas surface and empty-state handoff.
- `native/app/qml/components/RoomTile.qml`: room-card metadata, fixed action area, and state hierarchy.
- `native/app/qml/components/ToastViewport.qml`, `native/app/qml/panels/*.qml`, `native/app/qml/dialogs/*.qml`: unified transient-surface styling.
- `native/tests/qml_engine_smoke_test.cpp`: module loading and token regression.
- `native/tests/qml_visual_smoke_test.cpp`: release-module layout, status-bar, image, and overlap regression.
- `native/tests/qml_interaction_test.cpp`: control semantics, panel behavior, room library, and retained workflows.
- `native/CMakeLists.txt`: QML module and visual-test resource registration.

### Task 1: Make visual tests use the released QML module

**Files:**
- Modify: `native/CMakeLists.txt:159-208`
- Modify: `native/tests/qml_visual_smoke_test.cpp:91-109,175-186`
- Modify: `native/tests/qml_engine_smoke_test.cpp:22-64`

- [x] **Step 1: Add a failing release-module visual test**

Add a new slot and assertion to `QmlVisualSmokeTest` before changing its load path:

```cpp
void loadsReleasedModuleWithVisualAnchors();

void QmlVisualSmokeTest::loadsReleasedModuleWithVisualAnchors()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    engine.loadFromModule(QStringLiteral("DouyuMonitor"), QStringLiteral("Main"));

    QVERIFY2(!engine.rootObjects().isEmpty(), "Released QML module did not create Main");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("appHeader")) != nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("roomSidebar")) != nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("workspaceGrid")) != nullptr);
}
```

- [x] **Step 2: Run the new test and record the expected module-link failure**

Run:

```powershell
cmake --build --preset windows-x64-release --target qml_visual_smoke_test -- -j1
ctest --preset windows-x64-release -R '^qml_visual_smoke_test$' --output-on-failure
```

Expected before the CMake fixture update: `No module named "DouyuMonitor" found` or a missing-root-object assertion.

- [x] **Step 3: Link the visual test to the QML module and load it consistently**

In the `qml_visual_smoke_test` target, add the same module dependencies used by `qml_engine_smoke_test`:

```cmake
target_link_libraries(qml_visual_smoke_test PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::OpenGL Qt6::Test
    douyu_qml douyu_qmlplugin remote_playback mpv::mpv
)
qt_import_qml_plugins(qml_visual_smoke_test)
```

Replace the helper load call with the released module entry point:

```cpp
engine.loadFromModule(QStringLiteral("DouyuMonitor"), QStringLiteral("Main"));
```

Keep the existing `qrc:/qml` resources only for isolated component fixtures that cannot load through the module.

- [x] **Step 4: Add a module-color assertion to the existing engine smoke test**

Extend `loadsModuleWithThemeSingletonColors()` so it also verifies the generated module contains the `workspaceStatusBar` object after Task 2:

```cpp
QObject *statusBar = window->findChild<QObject *>(QStringLiteral("workspaceStatusBar"));
QVERIFY(statusBar != nullptr);
QCOMPARE(statusBar->property("color").value<QColor>(), QColor(QStringLiteral("#202731")));
```

Leave this assertion disabled until Task 2 adds the object; enable it in that task's green step.

- [x] **Step 5: Run module regressions**

Run:

```powershell
ctest --preset windows-x64-release -R '^(qml_engine_smoke_test|qml_visual_smoke_test)$' --output-on-failure
```

Expected: both tests pass and the visual test loads `DouyuMonitor/Main` instead of relying only on the hand-written `qmldir` resource path.

- [x] **Step 6: Commit the test-path correction**

```powershell
git add native/CMakeLists.txt native/tests/qml_engine_smoke_test.cpp native/tests/qml_visual_smoke_test.cpp
git commit -m "test: render visual checks from released QML module"
```

### Task 2: Add the root shell, real empty state, and global status bar

**Files:**
- Create: `native/app/qml/components/WorkspaceStatusBar.qml`
- Modify: `native/app/qml/Main.qml:10-255`
- Modify: `native/CMakeLists.txt:66-101,174-201,232-256`
- Modify: `native/tests/qml_visual_smoke_test.cpp`

- [x] **Step 1: Add failing empty and occupied status-bar checks**

Add two visual test slots. The empty-state test must locate `emptyWorkspaceState` and `workspaceStatusBar`; the occupied-state test must load the preview-room fixture and assert the empty state is hidden.

```cpp
void showsStructuredEmptyWorkspace();
void showsStatusBarForOccupiedWorkspace();

void QmlVisualSmokeTest::showsStructuredEmptyWorkspace()
{
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine, QSize(1280, 720));
    QVERIFY(window != nullptr);
    QObject *empty = window->findChild<QObject *>(QStringLiteral("emptyWorkspaceState"));
    QObject *status = window->findChild<QObject *>(QStringLiteral("workspaceStatusBar"));
    QVERIFY(empty != nullptr);
    QVERIFY(status != nullptr);
    QVERIFY(empty->property("visible").toBool());
    QCOMPARE(status->property("onlineCount").toInt(), 0);
}
```

Run `ctest --preset windows-x64-release -R '^qml_visual_smoke_test$' --output-on-failure`. Expected: failure because neither object exists.

- [x] **Step 2: Create the reusable status-bar component**

Create `WorkspaceStatusBar.qml` with only display properties. It must not own playback or status state:

```qml
import QtQuick
import ".."

Rectangle {
    id: root
    objectName: "workspaceStatusBar"
    property int onlineCount: 0
    property int offlineCount: 0
    property int danmakuCount: 0
    property string audioLabel: "无"
    property bool silentChecking: true
    height: 28
    radius: Theme.radiusSmall
    color: Theme.controlSurface
    border.color: Theme.border

    Row {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 14
        Text { text: root.onlineCount + " 直播中"; color: Theme.online; font.pixelSize: 10 }
        Text { text: root.offlineCount + " 未开播"; color: Theme.mutedText; font.pixelSize: 10 }
        Text { text: "弹幕 " + root.danmakuCount + " 已连接"; color: Theme.mutedText; font.pixelSize: 10 }
        Text { text: "声音焦点：" + root.audioLabel; color: Theme.mutedText; font.pixelSize: 10 }
        Text { text: root.silentChecking ? "静默检查" : "检查中"; color: Theme.mutedText; font.pixelSize: 10 }
    }
}
```

- [x] **Step 3: Compose the shell in `Main.qml`**

Add `WorkspaceStatusBar` below `WorkspaceGrid`, reduce the grid bottom anchor to the status-bar top, and add an explicit empty state inside the canvas region. Bind existing C++ properties without changing their interfaces:

```qml
readonly property var monitoringModel: appController ? appController.monitoring : null

WorkspaceStatusBar {
    id: workspaceStatusBar
    anchors.left: sidebar.right
    anchors.right: parent.right
    anchors.bottom: parent.bottom
    anchors.leftMargin: Theme.gap
    anchors.rightMargin: Theme.gap
    anchors.bottomMargin: Theme.gap
    onlineCount: monitoringModel ? monitoringModel.onlineCount : 0
    offlineCount: monitoringModel ? monitoringModel.offlineCount : 0
    danmakuCount: 0
    audioLabel: workspaceModel && workspaceModel.globalMuted ? "全局静音" : "无"
}
```

The empty state must have `objectName: "emptyWorkspaceState"`, use the approved copy, and set `visible: roomModel && roomModel.count === 0`.

- [x] **Step 4: Register the component in every QML resource path**

Add `app/qml/components/WorkspaceStatusBar.qml` to `douyu_qml` QML files and to each QML test resource list that loads `Main.qml` from resources.

- [x] **Step 5: Save and inspect three shell screenshots**

Extend the visual test to save `empty-shell-1280x720.png`, `occupied-shell-1600x900.png`, and `occupied-shell-1920x1080.png` with `saveScreenshot(window->grabWindow(), name)`. Verify the status bar is inside the viewport and does not overlap the canvas rectangle.

- [x] **Step 6: Run shell regressions**

Run:

```powershell
ctest --preset windows-x64-release -R '^(qml_engine_smoke_test|qml_visual_smoke_test|qml_interaction_test)$' --output-on-failure
```

Expected: all selected tests pass; the new screenshots contain the correct dark surfaces instead of a black fallback.

- [x] **Step 7: Commit the shell**

```powershell
git add native/CMakeLists.txt native/app/qml/Main.qml native/app/qml/components/WorkspaceStatusBar.qml native/tests/qml_engine_smoke_test.cpp native/tests/qml_visual_smoke_test.cpp
git commit -m "feat: add structured workspace shell and status bar"
```

### Task 3: Rebuild header and window-control hierarchy

**Files:**
- Modify: `native/app/qml/components/AppHeader.qml`
- Modify: `native/app/qml/components/WindowControls.qml`
- Modify: `native/tests/qml_interaction_test.cpp:505-760`

- [x] **Step 1: Add failing semantic and geometry assertions**

Add `usesGroupedHeaderControls()` to assert all top-bar controls use a `32px` hit area, fullscreen and maximize icon sources differ, and the sound popover starts below its button:

```cpp
QObject *fullscreen = window->findChild<QObject *>(QStringLiteral("fullscreenButton"));
QObject *controls = window->findChild<QObject *>(QStringLiteral("windowControls"));
QObject *sound = window->findChild<QObject *>(QStringLiteral("soundMasterButton"));
QObject *popover = window->findChild<QObject *>(QStringLiteral("soundMasterPopover"));
QVERIFY(fullscreen != nullptr && controls != nullptr && sound != nullptr && popover != nullptr);
QVERIFY(fullscreen->property("Accessible.name").toString() == QStringLiteral("全屏播放"));
QVERIFY(popover->property("y").toDouble() >= sound->property("y").toDouble() + sound->property("height").toDouble());
```

Run `ctest --preset windows-x64-release -R '^qml_interaction_test$' --output-on-failure`. Expected: failure until the fixed hit-area and visual-group properties exist.

- [x] **Step 2: Apply shared header geometry**

Set `width: Theme.controlHeight`, `height: Theme.controlHeight`, `radius: Theme.radiusSmall`, and consistent hover/pressed colors on every top-bar `ToolButton`. Preserve the existing signals and object names: `sidebarToggleButton`, `danmakuButton`, `monitoringButton`, `workspaceButton`, `layoutMenuButton`, `soundMasterButton`, and `fullscreenButton`.

- [x] **Step 3: Keep window-control semantics distinct**

Use `window-minimize.svg`, `window-maximize.svg`, and `window-close.svg` only in `WindowControls.qml`. Keep `window-fullscreen.svg` only in the `fullscreenButton` content item. Set accessible names to `最小化窗口`, `最大化窗口` or `还原窗口`, `关闭窗口`, and `全屏播放`.

- [x] **Step 4: Make the sound popover a proper above-canvas surface**

Keep it as an anchored QML `Popup` or a high-z `Rectangle`; set `z` above the workspace and `y: root.height + Theme.gap`. Retain the existing global mute, single-channel, and multi-channel method calls. Add an explicit close button with `objectName: "closeSoundMasterButton"` and `onClicked: soundMasterPopover.close()`.

- [x] **Step 5: Run header interactions and screenshot checks**

Run:

```powershell
ctest --preset windows-x64-release -R '^(qml_interaction_test|qml_visual_smoke_test)$' --output-on-failure
```

Expected: header tests pass; the 1280px screenshot shows no top-bar overlap.

- [x] **Step 6: Commit header work**

```powershell
git add native/app/qml/components/AppHeader.qml native/app/qml/components/WindowControls.qml native/tests/qml_interaction_test.cpp
git commit -m "feat: unify header controls and sound panel"
```

### Task 4: Rebuild room-library information hierarchy

**Files:**
- Modify: `native/app/qml/components/RoomSidebar.qml`
- Modify: `native/app/qml/components/RoomLibraryView.qml`
- Modify: `native/tests/qml_interaction_test.cpp:470-1190`
- Modify: `native/tests/qml_visual_smoke_test.cpp`

- [x] **Step 1: Add a failing long-metadata bounds regression**

Extend the existing long-title interaction fixture with a long `anchorName` and `title`. Assert that the title text right edge is before the fixed action bar left edge:

```cpp
QObject *title = window->findChild<QObject *>(QStringLiteral("sidebarRoomTitle"));
QObject *actions = window->findChild<QObject *>(QStringLiteral("sidebarRoomActionBar"));
QVERIFY(title != nullptr && actions != nullptr);
QVERIFY(title->property("x").toDouble() + title->property("width").toDouble()
        <= actions->property("x").toDouble());
```

Run `ctest --preset windows-x64-release -R '^qml_interaction_test$' --output-on-failure`. Expected: failure until the new object names and fixed action geometry exist.

- [x] **Step 2: Give current-room rows fixed structure**

In `RoomSidebar.qml`, use `Theme.managementSurface` for the shell, a 60px row height, a fixed avatar column, a text column anchored between avatar and actions, and a fixed `roomActionBar` width. Add `objectName: "sidebarRoomTitle"` to the visible title text and `objectName: "sidebarRoomActionBar"` to the action container. Use `Text.ElideRight` for both metadata lines.

- [x] **Step 3: Add state and selection hierarchy**

Render a compact status dot plus `直播中`, `未开播`, or `检查中`. Use `Theme.online`, `Theme.mutedText`, and `Theme.warning`; selected rows use a restrained accent border and control-surface background. Do not change favorite, ordering, remove, history, or search signal handlers.

- [x] **Step 4: Apply the same geometry to favorites and history**

In `RoomLibraryView.qml`, give favorite and history rows the same avatar/text/action layout. Preserve `openHistoryRoomButton`, `removeHistoryButton`, and all existing library actions.

- [x] **Step 5: Run room-library regressions**

Run:

```powershell
ctest --preset windows-x64-release -R '^(qml_interaction_test|room_list_model_test|qml_visual_smoke_test)$' --output-on-failure
```

Expected: current, favorite, and history lists remain functional; long metadata does not overlap controls at 1600x900 or 1920x1080.

- [x] **Step 6: Commit room-library work**

```powershell
git add native/app/qml/components/RoomSidebar.qml native/app/qml/components/RoomLibraryView.qml native/tests/qml_interaction_test.cpp native/tests/qml_visual_smoke_test.cpp
git commit -m "feat: rebuild room library visual hierarchy"
```

### Task 5: Rebuild canvas and room-card hierarchy without changing layouts

**Files:**
- Modify: `native/app/qml/components/WorkspaceGrid.qml`
- Modify: `native/app/qml/components/RoomTile.qml`
- Modify: `native/app/qml/components/PrimaryRoomDivider.qml`
- Modify: `native/tests/qml_interaction_test.cpp:1247-1325`
- Modify: `native/tests/qml_visual_smoke_test.cpp`

- [x] **Step 1: Add failing card-layer tests**

Add a visual test that finds `roomTopMetadata`, `roomActionBar`, `roomAnchorName`, and `roomTitleText`, then asserts that the title rectangle ends before the action bar and that primary cards have an accent border:

```cpp
QQuickItem *tile = previewRoomTile(window->contentItem());
QVERIFY(tile != nullptr);
QObject *title = tile->findChild<QObject *>(QStringLiteral("roomTitleText"));
QObject *actions = tile->findChild<QObject *>(QStringLiteral("roomActionBar"));
QVERIFY(title != nullptr && actions != nullptr);
QVERIFY(itemRect(title).right() < itemRect(actions).left());
QCOMPARE(tile->property("primary").toBool(), true);
```

Run `ctest --preset windows-x64-release -R '^qml_visual_smoke_test$' --output-on-failure`. Expected: the new geometry assertion fails before the bottom information band is rebuilt.

- [x] **Step 2: Preserve the geometry algorithm exactly**

Do not edit `autoRowCounts`, `primarySecondaryRowCounts`, `primaryZoneWidths`, `primarySideCounts`, or `geometryFor`. Restrict changes in `WorkspaceGrid.qml` to `Theme.canvas`, card gaps, the canvas border, and status-bar spacing.

- [x] **Step 3: Rebuild `RoomTile.qml` as three stable zones**

Use a top metadata row for status and route number, a center playback region, and a bottom information band. Anchor the text column to the action bar rather than giving it the full card width:

```qml
Column {
    id: roomTextColumn
    anchors.left: parent.left
    anchors.right: tileActions.left
    anchors.bottom: parent.bottom
    anchors.margins: Theme.gap
    spacing: 2
    Text { objectName: "roomAnchorName"; width: parent.width; elide: Text.ElideRight; color: Theme.text }
    Text { objectName: "roomTitleText"; width: parent.width; elide: Text.ElideRight; color: Theme.mutedText }
}
```

Retain the existing player attachment, volume, quality, refresh, danmaku, primary-room, and remove handlers.

- [x] **Step 4: Style primary and secondary cards**

Use `Theme.borderStrong` for ordinary cards, `Theme.accent` for the primary card border, and an explicit `主画面` label. Keep the primary divider as a low-opacity accent line without altering drag behavior or room assignment.

- [x] **Step 5: Run layout and playback regressions**

Run:

```powershell
ctest --preset windows-x64-release -R '^(qml_visual_smoke_test|qml_interaction_test|multi_room_coordinator_test|mpv_quick_item_test)$' --output-on-failure
```

Expected: 5-route automatic layout remains three-over-two; primary layouts preserve their current 2 through 9 route placement and cards remain visible after model changes.

- [x] **Step 6: Commit canvas work**

```powershell
git add native/app/qml/components/WorkspaceGrid.qml native/app/qml/components/RoomTile.qml native/app/qml/components/PrimaryRoomDivider.qml native/tests/qml_interaction_test.cpp native/tests/qml_visual_smoke_test.cpp
git commit -m "feat: rebuild multi-room canvas visual hierarchy"
```

### Task 6: Unify panels, dialogs, and toast behavior

**Files:**
- Modify: `native/app/qml/components/ToastViewport.qml`
- Modify: `native/app/qml/panels/DanmakuSettingsPanel.qml`
- Modify: `native/app/qml/panels/MonitoringStatusPanel.qml`
- Modify: `native/app/qml/panels/WorkspacePresetsPanel.qml`
- Modify: `native/app/qml/dialogs/AddRoomDialog.qml`
- Modify: `native/app/qml/dialogs/NotificationSettingsDialog.qml`
- Modify: `native/tests/qml_interaction_test.cpp:273-623,1114-1190`

- [x] **Step 1: Add failing surface and close-action tests**

For each panel, add an object-name lookup and close assertion. The shared test pattern is:

```cpp
click(window->findChild<QObject *>(QStringLiteral("danmakuButton")));
QObject *panel = window->findChild<QObject *>(QStringLiteral("danmakuSettingsPanel"));
QObject *closeButton = panel->findChild<QObject *>(QStringLiteral("closeDanmakuSettingsButton"));
QVERIFY(panel != nullptr && closeButton != nullptr);
QTRY_VERIFY(panel->property("visible").toBool());
click(closeButton);
QTRY_VERIFY(!panel->property("visible").toBool());
```

Repeat for monitoring, workspace presets, sound master, add-room dialog, and notification settings with their own stable object names.

- [x] **Step 2: Apply the same container contract**

Each Popup, Drawer, and Dialog uses `Theme.controlSurface`, `Theme.border`, `Theme.radiusLarge`, an explicit title, internal `Theme.gap` padding, and a top-right close button. Inputs use `Theme.well`; primary buttons use `Theme.accent`; destructive buttons use `Theme.danger`.

- [x] **Step 3: Preserve all existing functionality**

Keep calls to `saveWorkspacePreset`, `applyWorkspacePreset`, `deleteWorkspacePreset`, `searchRooms`, `addRoomCandidate`, notification setting updates, danmaku setting updates, and monitoring data bindings unchanged. The task changes QML presentation and close behavior only.

- [x] **Step 4: Keep normal room-state checks silent**

In `ToastViewport.qml`, render only explicit warning, error, and user-action messages. Do not add a toast for normal refresh success or normal playback state.

- [x] **Step 5: Run panel and workflow regressions**

Run:

```powershell
ctest --preset windows-x64-release -R '^(qml_interaction_test|qml_danmaku_overlay_test|app_controller_test|notification_policy_test)$' --output-on-failure
```

Expected: each surface opens and closes; preset deletion and deferred application remain safe; normal refresh does not create a persistent toast.

- [x] **Step 6: Commit transient-surface work**

```powershell
git add native/app/qml/components/ToastViewport.qml native/app/qml/panels native/app/qml/dialogs native/tests/qml_interaction_test.cpp
git commit -m "feat: unify dark panels dialogs and notifications"
```

### Task 7: Run release and visual acceptance checks

**Files:**
- Modify: `native/tests/qml_visual_smoke_test.cpp`
- Create: `native/docs/visual-validation.md`

- [x] **Step 1: Extend screenshot capture coverage**

Save the following QQuickWindow images in `QML_VERIFICATION_DIR` from release-module tests:

```text
empty-shell-1280x720.png
one-room-1280x720.png
three-rooms-1600x900.png
five-rooms-1600x900.png
nine-rooms-1920x1080.png
danmaku-panel-1600x900.png
sound-panel-1600x900.png
```

Each capture must assert non-black canvas pixels, viewport containment, and no intersection between room text and action bars.

- [x] **Step 2: Build the released executable and run selected CTest suites**

Run:

```powershell
cmake --build --preset windows-x64-release --target douyu_monitor_native -- -j1
ctest --preset windows-x64-release -R '^(qml_engine_smoke_test|qml_visual_smoke_test|qml_interaction_test|qml_close_regression_test|multi_room_coordinator_test|mpv_quick_item_test)$' --output-on-failure
```

Expected: build exits with code 0. If `qml_close_regression_test` fails only under `QT_QPA_PLATFORM=offscreen` because nine OpenGL contexts cannot initialize, run the same test once with the Windows desktop backend and record that distinction in the validation document.

- [x] **Step 3: Check the packaged module metadata**

Run:

```powershell
rg -n '^singleton Theme 1\.0 app/qml/Theme\.qml$' out/build/windows-x64-release/DouyuMonitor/qmldir
```

Expected: exactly one singleton registration. This confirms the released module uses the same theme contract as the tests.

- [x] **Step 4: Write visual validation evidence**

Create `native/docs/visual-validation.md` with the exact build command, CTest result count, screenshot file names, viewport dimensions, and any environment-only limitation. Do not state that a browser mockup validates the Qt executable.

- [x] **Step 5: Commit validation artifacts**

```powershell
git add native/tests/qml_visual_smoke_test.cpp native/docs/visual-validation.md
git commit -m "test: document full QML visual acceptance"
```

## Plan self-review

- The approved empty and occupied workspace states map to Task 2.
- Header, window controls, icon semantics, and sound surface map to Task 3.
- Room-library hierarchy, long text, favorites, and history map to Task 4.
- Canvas, card hierarchy, primary-room styling, and existing layout algorithms map to Task 5.
- Popup, drawer, dialog, toast, and silent normal checks map to Task 6.
- Released-module parity, builds, native QQuickWindow screenshots, and the nine-route close limitation map to Tasks 1 and 7.
- The plan uses only existing C++ interfaces and adds no Electron, Chromium, Qt WebEngine, or simulated live-room data.
