# Two-Mode Workspace Layout Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restrict the Qt/QML workspace to automatic layout and primary-plus-secondary layout, with deterministic full-canvas geometry for one through nine rooms.

**Architecture:** The C++ coordinator owns the two persisted layout mode values and maps stored legacy choices to a supported mode. `WorkspaceGrid.qml` owns pure geometry selection: it maps the room count to a row specification, then positions existing `RoomTile` delegates without changing their model identity. `AppHeader.qml` exposes only the two supported choices.

**Tech Stack:** C++20, Qt 6, Qt Quick/QML, Qt Test, CMake/Ninja, libmpv-backed `MpvQuickItem`.

---

### Task 1: Normalize layout modes at the coordinator boundary

**Files:**
- Modify: `native/src/workspace/multi_room_coordinator.cpp:15-22,110-116,147-153,319-325,369-383`
- Modify: `native/tests/multi_room_coordinator_test.cpp:490-530`
- Modify: `native/tests/app_controller_test.cpp:218-251`

- [x] **Step 1: Write failing coordinator tests for the two supported modes and legacy migration.**

```cpp
void MultiRoomCoordinatorTest::acceptsOnlyTwoCurrentLayoutModes()
{
    FakeStreamgetClient client;
    MultiRoomCoordinator coordinator(&client);

    QVERIFY(coordinator.setLayout(QStringLiteral("auto")));
    QCOMPARE(coordinator.layoutMode(), QStringLiteral("auto"));
    QCOMPARE(coordinator.layoutId(), QStringLiteral("auto"));

    QVERIFY(coordinator.setLayout(QStringLiteral("primary")));
    QCOMPARE(coordinator.layoutMode(), QStringLiteral("primary"));
    QCOMPARE(coordinator.layoutId(), QStringLiteral("primary"));
}

void MultiRoomCoordinatorTest::mapsStoredLegacyLayoutsToTwoModes()
{
    FakeStreamgetClient client;
    MultiRoomCoordinator coordinator(&client);

    QVERIFY(coordinator.setLayout(QStringLiteral("primary-two")));
    QCOMPARE(coordinator.layoutMode(), QStringLiteral("primary"));
    QVERIFY(coordinator.setLayout(QStringLiteral("grid-3x3")));
    QCOMPARE(coordinator.layoutMode(), QStringLiteral("auto"));
}
```

- [x] **Step 2: Run the focused test and verify it fails because `primary` is unsupported and legacy modes are preserved.**

Run:

```powershell
$env:PATH='D:\Qt\6.8.3\msvc2022_64\bin;'+$env:PATH
& 'native\out\build\windows-x64-release\multi_room_coordinator_test.exe' -v1
```

Expected: the new test methods fail before the production change.

- [x] **Step 3: Implement the smallest supported-mode normalizer.**

```cpp
QString normalizeLayoutMode(const QString &layoutId)
{
    const QString normalized = layoutId.trimmed().toLower();
    if (normalized == QStringLiteral("primary")
        || normalized == QStringLiteral("primary-two")) {
        return QStringLiteral("primary");
    }
    if (normalized == QStringLiteral("auto") || normalized == QStringLiteral("single")
        || normalized == QStringLiteral("grid-2x2")
        || normalized == QStringLiteral("grid-3x2")
        || normalized == QStringLiteral("grid-3x3")
        || normalized == QStringLiteral("split-horizontal")
        || normalized == QStringLiteral("split-vertical")) {
        return QStringLiteral("auto");
    }
    return {};
}

bool MultiRoomCoordinator::setLayout(const QString &layoutId)
{
    const QString normalized = normalizeLayoutMode(layoutId);
    if (normalized.isEmpty()) return false;
    const QString previousLayout = layoutId_;
    const QString previousMode = layoutMode_;
    layoutMode_ = normalized;
    layoutId_ = normalized;
    if (previousLayout != layoutId_) emit layoutChanged(layoutId_);
    return previousLayout != layoutId_ || previousMode != layoutMode_;
}
```

Replace the automatic `recommendedGridId(order_.size())` assignments in room add, remove, replacement, and `setLayout` with `layoutId_ = layoutMode_`; keep `recommendedGridId` untouched because it is outside this UI migration.

- [x] **Step 4: Update controller/preset assertions to store `primary` instead of `primary-two`; run both focused suites.**

Run:

```powershell
$env:PATH='D:\Qt\6.8.3\msvc2022_64\bin;'+$env:PATH
& 'native\out\build\windows-x64-release\multi_room_coordinator_test.exe' -v1
& 'native\out\build\windows-x64-release\app_controller_test.exe' -v1
```

Expected: both executables exit with code `0`.

- [ ] **Step 5: Commit the focused coordinator change.**

```powershell
git add native/src/workspace/multi_room_coordinator.cpp native/tests/multi_room_coordinator_test.cpp native/tests/app_controller_test.cpp
git commit -m "feat: reduce workspace layout modes"
```

### Task 2: Add deterministic full-canvas QML geometry

**Files:**
- Modify: `native/app/qml/components/WorkspaceGrid.qml:7-159,179-191`
- Modify: `native/tests/qml_interaction_test.cpp:24-45,1161-1204`

- [x] **Step 1: Add failing QML geometry tests for automatic 5 rooms and primary 1/3/5/9 rooms.**

```cpp
void QmlInteractionTest::laysOutFiveAutomaticRoomsInThreeAndTwoRows();
void QmlInteractionTest::laysOutSinglePrimaryRoomAcrossTheCanvas();
void QmlInteractionTest::laysOutThreePrimaryRoomsWithTwoRightHandTiles();
void QmlInteractionTest::laysOutFivePrimaryRoomsWithTwoByTwoSecondaries();
void QmlInteractionTest::laysOutNinePrimaryRoomsWithTwoByFourSecondaries();
```

For each test, instantiate `WorkspaceGrid.qml` at `1280 x 720` using the existing `roomTileProperties()` helper. Find delegates under `layoutSurface` by `roomId`, then assert: every tile lies inside `layoutSurface`; no two tile rectangles intersect; automatic five has three unique top-row tiles and two unique bottom-row tiles; primary mode places the primary tile at `x == 0` spanning the full height; and the specified right-side row/column counts match the design.

- [x] **Step 2: Run the focused QML test and verify the new cases fail against the old equal-cell and `primary-two` logic.**

Run:

```powershell
$env:PATH='D:\Qt\6.8.3\msvc2022_64\bin;'+$env:PATH
$env:QT_PLUGIN_PATH='D:\Qt\6.8.3\msvc2022_64\plugins'
$env:QML2_IMPORT_PATH='D:\Qt\6.8.3\msvc2022_64\qml'
$env:QT_QPA_PLATFORM='offscreen'
$env:QT_QUICK_CONTROLS_STYLE='Basic'
& 'native\out\build\windows-x64-release\qml_interaction_test.exe' -v1
```

Expected: the new layout assertions fail before the QML rewrite.

- [x] **Step 3: Replace legacy layout resolution with two geometry specifications.**

Implement the following QML helpers in `WorkspaceGrid.qml`:

```qml
function autoRowCounts(count) {
    const rows = [[], [1], [2], [3], [2, 2], [3, 2], [3, 3], [4, 3], [4, 4], [3, 3, 3]]
    return rows[Math.max(0, Math.min(9, count))]
}

function primarySecondaryRowCounts(count) {
    const rows = [[], [], [1], [1, 1], [1, 1, 1], [2, 2], [3, 2], [3, 3], [4, 3], [2, 2, 2, 2]]
    return rows[Math.max(0, Math.min(9, count))]
}

function primaryWidthFor(count) {
    return count <= 1 ? 1 : (count <= 4 ? 2 / 3 : 1 / 2)
}
```

Use one `geometryFor(index, roomId)` helper that returns `{ x, y, width, height }`. For `auto`, use the selected row and each row's equal fractions of the surface width. For `primary`, resolve the primary room by `primaryRoomId`, fall back to index zero when absent, give it `primaryWidthFor(roomCount)` and full height, then place the remaining rooms in the right region according to `primarySecondaryRowCounts`. Subtract the existing eight-pixel gaps before proportional subdivision so the last tile ends at the surface edge. Bind every `RoomTile` geometry property to this returned object without altering the `Repeater` model, delegate key, or player attachment path.

Remove `resolvedLayoutId`, `recommendedLayout`, `gridColumns`, `primaryFocus`, `focusVertical`, `primaryRoomRatio`, and `PrimaryRoomDivider` usage from this component. Do not call `beginResetModel`, replace `roomModel`, or create a nested repeater.

- [x] **Step 4: Update old `primary-two` fixture values to `primary`, execute the focused QML suite, and inspect the geometry assertions.**

Run:

```powershell
$env:PATH='D:\Qt\6.8.3\msvc2022_64\bin;'+$env:PATH
$env:QT_PLUGIN_PATH='D:\Qt\6.8.3\msvc2022_64\plugins'
$env:QML2_IMPORT_PATH='D:\Qt\6.8.3\msvc2022_64\qml'
$env:QT_QPA_PLATFORM='offscreen'
$env:QT_QUICK_CONTROLS_STYLE='Basic'
& 'native\out\build\windows-x64-release\qml_interaction_test.exe' -v1
```

Expected: all tests exit with code `0`.

- [ ] **Step 5: Commit the geometry implementation and regression tests.**

```powershell
git add native/app/qml/components/WorkspaceGrid.qml native/tests/qml_interaction_test.cpp
git commit -m "feat: fill workspace with two layout modes"
```

### Task 3: Reduce the visible layout menu to two choices

**Files:**
- Modify: `native/app/qml/components/AppHeader.qml:260-323`
- Modify: `native/app/qml/Main.qml:34-36,236-243`
- Modify: `native/tests/qml_interaction_test.cpp:734-750`

- [x] **Step 1: Add failing interaction assertions for exactly two layout options.**

```cpp
void QmlInteractionTest::exposesOnlyAutomaticAndPrimaryLayoutOptions()
{
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("autoLayoutOption")) != nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("primaryLayoutOption")) != nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("singleLayoutOption")) == nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("grid2LayoutOption")) == nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("grid3LayoutOption")) == nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("grid3x3LayoutOption")) == nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("splitHorizontalLayoutOption")) == nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("splitVerticalLayoutOption")) == nullptr);
}
```

- [x] **Step 2: Run the target test and verify it fails while legacy menu items still exist.**

Run:

```powershell
$env:PATH='D:\Qt\6.8.3\msvc2022_64\bin;'+$env:PATH
$env:QT_PLUGIN_PATH='D:\Qt\6.8.3\msvc2022_64\plugins'
$env:QML2_IMPORT_PATH='D:\Qt\6.8.3\msvc2022_64\qml'
$env:QT_QPA_PLATFORM='offscreen'
$env:QT_QUICK_CONTROLS_STYLE='Basic'
& 'native\out\build\windows-x64-release\qml_interaction_test.exe' exposesOnlyAutomaticAndPrimaryLayoutOptions -v1
```

Expected: FAIL because legacy menu objects are still present.

- [x] **Step 3: Replace the menu items and simplify the grid bindings.**

```qml
MenuItem {
    objectName: "autoLayoutOption"
    text: "自动布局"
    checkable: true
    checked: root.workspaceModel ? root.workspaceModel.layoutMode === "auto" : true
    onTriggered: if (root.controller) root.controller.setLayout("auto")
}
MenuItem {
    objectName: "primaryLayoutOption"
    text: "主直播间 + 辅直播间"
    checkable: true
    checked: root.workspaceModel ? root.workspaceModel.layoutMode === "primary" : false
    onTriggered: if (root.controller) root.controller.setLayout("primary")
}
```

Remove the obsolete items. In `Main.qml`, stop passing `layoutId` and `primaryRoomRatio` to `WorkspaceGrid`; preserve `layoutMode` and `primaryRoomId`.

- [x] **Step 4: Run the complete QML interaction target.**

Run:

```powershell
$env:PATH='D:\Qt\6.8.3\msvc2022_64\bin;'+$env:PATH
$env:QT_PLUGIN_PATH='D:\Qt\6.8.3\msvc2022_64\plugins'
$env:QML2_IMPORT_PATH='D:\Qt\6.8.3\msvc2022_64\qml'
$env:QT_QPA_PLATFORM='offscreen'
$env:QT_QUICK_CONTROLS_STYLE='Basic'
& 'native\out\build\windows-x64-release\qml_interaction_test.exe' -v1
```

Expected: exit code `0` with both new menu and geometry tests green.

- [ ] **Step 5: Commit the menu reduction.**

```powershell
git add native/app/qml/components/AppHeader.qml native/app/qml/Main.qml native/tests/qml_interaction_test.cpp
git commit -m "feat: expose two workspace layouts"
```

### Task 4: Build and visually verify the new workspace

**Files:**
- Modify only if screenshot expectations need updating: `native/tests/qml_visual_smoke_test.cpp`
- Verify: `native/out/build/windows-x64-release/douyu_monitor_native.exe`

- [x] **Step 1: Build the Release app and affected test executables.**

Run:

```powershell
cmd /d /s /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build "D:\DouyuMonitor\.worktrees\codex\qt-libmpv-m0\native\out\build\windows-x64-release" --target douyu_monitor_native multi_room_coordinator_test app_controller_test qml_interaction_test qml_visual_smoke_test qml_close_regression_test --config Release -- -j1'
```

Expected: command exits with code `0`.

- [x] **Step 2: Run all affected regression targets with the Qt runtime environment.**

Run:

```powershell
$env:PATH='D:\Qt\6.8.3\msvc2022_64\bin;'+$env:PATH
$env:QT_PLUGIN_PATH='D:\Qt\6.8.3\msvc2022_64\plugins'
$env:QML2_IMPORT_PATH='D:\Qt\6.8.3\msvc2022_64\qml'
$env:QT_QPA_PLATFORM='offscreen'
$env:QT_QUICK_CONTROLS_STYLE='Basic'
$base='native\out\build\windows-x64-release'
& "$base\multi_room_coordinator_test.exe" -v1
& "$base\app_controller_test.exe" -v1
& "$base\qml_interaction_test.exe" -v1
& "$base\qml_visual_smoke_test.exe" -v1
& "$base\qml_close_regression_test.exe" removesAttachedPlayersWhileWindowRemainsOpen -v1
```

Expected: every executable exits with code `0`.

- [x] **Step 3: Verify the actual Release window.**

Launch the Release executable, add test rooms only as needed, and use ComputerUse or a current Playwright-capable visual harness to inspect 1, 3, 5, and 9 rooms in both modes. Confirm the specified 5-room automatic, 3/5/9-room primary arrangements, no visual gaps, no overlap, stable video after a layout switch, and no crash after removing a room. Request action-time confirmation before deleting any local room record through the Windows UI.

- [x] **Step 4: Run final whitespace/status checks and commit any required visual-test update.**

Implementation note: the actual Release window was checked with one existing room and both layout-menu entries. The deterministic 3/5/9 geometry checks ran in `qml_interaction_test` without adding or deleting records from the user's workspace.

Run:

```powershell
git diff --check
git status --short
```

Expected: no whitespace errors; report pre-existing unrelated modifications separately and never revert them.
