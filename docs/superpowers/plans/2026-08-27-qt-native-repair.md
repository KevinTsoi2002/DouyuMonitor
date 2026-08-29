# Qt Native Repair Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore Qt client correctness, nine-room behavior, resource efficiency, and Electron-reference UI parity without changing the Qt-only runtime boundary.

**Architecture:** Keep coordinator and playback ownership in C++. Add a safe library projection for QML history/favorites. Use the libmpv wakeup callback to wake each QML item only when events exist, and use the libmpv render callback for frame scheduling. Package all visuals as QML module assets.

**Tech Stack:** C++20, Qt 6 QML/Quick/Test, CMake/Ninja, libmpv, Windows GUI subsystem.

---

### Task 1: Lock the room-status contract

**Files:**
- Modify: `native/tests/room_list_model_test.cpp`
- Modify: `native/app/qml/components/RoomTile.qml`
- Modify: `native/app/qml/components/RoomSidebar.qml`

- [ ] **Step 1: Add failing model and QML contract tests**

```cpp
QCOMPARE(model.data(model.index(0, 0), RoomListModel::LiveStateRole).toString(),
         QStringLiteral("online"));
QVERIFY(qmlTextUsesExactState("online"));
```

- [ ] **Step 2: Run the focused tests and observe the QML contract failure**

Run: `ctest --preset windows-x64-debug -R "room_list_model_test|qml_interaction_test" --output-on-failure -j 1`

- [ ] **Step 3: Change QML status checks from `live` to `online`**

```qml
readonly property bool isOnline: liveState === "online"
text: isOnline ? "直播中" : "未开播"
```

- [ ] **Step 4: Re-run the focused tests**

Run: `ctest --preset windows-x64-debug -R "room_list_model_test|qml_interaction_test" --output-on-failure -j 1`

### Task 2: Preserve partial valid metadata

**Files:**
- Modify: `native/tests/room_session_test.cpp`
- Modify: `native/src/workspace/room_session.cpp`
- Modify: `native/src/ui/app_controller.cpp`

- [ ] **Step 1: Add a failing test for valid identity with optional empty fields**

```cpp
RoomSearchResult result{.roomId = QStringLiteral("63136"), .anchorName = QStringLiteral("主播")};
result.online = true;
session.applyMetadata(result);
QCOMPARE(session.liveStatus(), RoomLiveStatus::Online);
QCOMPARE(session.metadata().anchorName, QStringLiteral("主播"));
```

- [ ] **Step 2: Run the focused test and observe rejection**

Run: `ctest --preset windows-x64-debug -R room_session_test --output-on-failure -j 1`

- [ ] **Step 3: Accept safe partial metadata and merge nonempty fields**

```cpp
if (result.roomId != roomId_ || result.anchorName.isEmpty()) return;
metadata_.anchorName = result.anchorName;
if (!result.title.isEmpty()) metadata_.title = result.title;
setLiveStatus(result.online ? RoomLiveStatus::Online : RoomLiveStatus::Offline);
```

- [ ] **Step 4: Re-run room-session and coordinator tests**

Run: `ctest --preset windows-x64-debug -R "room_session_test|multi_room_coordinator_test" --output-on-failure -j 1`

### Task 3: Add durable history projection

**Files:**
- Create: `native/src/ui/room_library_model.h`
- Create: `native/src/ui/room_library_model.cpp`
- Modify: `native/src/ui/app_controller.h`
- Modify: `native/src/ui/app_controller.cpp`
- Modify: `native/tests/app_controller_test.cpp`
- Modify: `native/CMakeLists.txt`

- [ ] **Step 1: Add failing controller test**

```cpp
QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
QTRY_COMPARE(controller.libraryRooms()->rowCount(), 1);
QVERIFY(controller.libraryRooms()->data(controller.libraryRooms()->index(0, 0),
        RoomLibraryModel::LastOpenedAtRole).toLongLong() > 0);
```

- [ ] **Step 2: Run the controller test and observe missing library API**

Run: `ctest --preset windows-x64-debug -R app_controller_test --output-on-failure -j 1`

- [ ] **Step 3: Implement a safe `RoomLibraryModel` and touch history after accepted add**

```cpp
Q_PROPERTY(RoomLibraryModel *libraryRooms READ libraryRooms CONSTANT)
record->lastOpenedAtMs = QDateTime::currentMSecsSinceEpoch();
libraryRooms_->apply(snapshot_.library, coordinator_->roomIds());
```

- [ ] **Step 4: Re-run controller, workspace-store, and model tests**

Run: `ctest --preset windows-x64-debug -R "app_controller_test|native_workspace_store_test|room_library_model_test" --output-on-failure -j 1`

### Task 4: Restore Electron-parity sidebar and title-bar behavior

**Files:**
- Modify: `native/app/qml/Main.qml`
- Modify: `native/app/qml/components/AppHeader.qml`
- Modify: `native/app/qml/components/RoomSidebar.qml`
- Create: `native/app/qml/components/RoomLibraryView.qml`
- Modify: `native/tests/qml_interaction_test.cpp`
- Modify: `native/tests/qml_visual_smoke_test.cpp`
- Modify: `native/CMakeLists.txt`

- [ ] **Step 1: Add a failing interaction test for header order and history mode**

```cpp
QVERIFY(toggle->property("x").toDouble() < brandMark->property("x").toDouble());
click(window->findChild<QObject *>(QStringLiteral("historyTab")));
QTRY_VERIFY(window->findChild<QObject *>(QStringLiteral("roomLibraryView"))->property("visible").toBool());
```

- [ ] **Step 2: Run QML interaction test and observe missing history surface/order**

Run: `ctest --preset windows-x64-debug -R qml_interaction_test --output-on-failure -j 1`

- [ ] **Step 3: Implement exact three-mode sidebar with a left-leading collapse button**

```qml
Row {
    ToolButton { objectName: "sidebarToggleButton"; onClicked: root.toggleSidebar() }
    Item { objectName: "brandMark"; width: 28; height: 28 }
}
property string viewMode: "current"
ToolButton { objectName: "historyTab"; onClicked: root.viewMode = "history" }
RoomLibraryView { visible: root.viewMode === "history" || root.viewMode === "favorites" }
```

- [ ] **Step 4: Re-run interaction and visual smoke tests**

Run: `ctest --preset windows-x64-debug -R "qml_interaction_test|qml_visual_smoke_test" --output-on-failure -j 1`

### Task 5: Replace placeholder icons with QML module assets

**Files:**
- Create: `native/app/qml/assets/icons/*.svg`
- Modify: `native/CMakeLists.txt`
- Modify: `native/app/qml/components/AppHeader.qml`
- Modify: `native/app/qml/components/RoomSidebar.qml`
- Modify: `native/app/qml/components/RoomTile.qml`
- Modify: `native/app/qml/components/WindowControls.qml`
- Modify: `native/tests/qml_visual_smoke_test.cpp`

- [ ] **Step 1: Add a failing QML smoke assertion that references asset-backed image objects**

```cpp
QVERIFY(window->findChild<QObject *>(QStringLiteral("headerSidebarIcon")) != nullptr);
QVERIFY(window->findChild<QObject *>(QStringLiteral("roomFavoriteIcon")) != nullptr);
```

- [ ] **Step 2: Run the QML smoke test and observe missing objects**

Run: `ctest --preset windows-x64-debug -R qml_visual_smoke_test --output-on-failure -j 1`

- [ ] **Step 3: Package SVG icon assets in `qt_add_qml_module` and render them via `Image`**

```cmake
RESOURCES app/qml/assets/icons/menu.svg app/qml/assets/icons/star.svg
```

```qml
Image { objectName: "headerSidebarIcon"; source: "assets/icons/menu.svg" }
```

- [ ] **Step 4: Re-run visual smoke test**

Run: `ctest --preset windows-x64-debug -R qml_visual_smoke_test --output-on-failure -j 1`

### Task 6: Remove idle libmpv polling and continuous render scheduling

**Files:**
- Modify: `native/tests/mpv_quick_item_test.cpp`
- Modify: `native/src/ui/mpv_quick_item.h`
- Modify: `native/src/ui/mpv_quick_item.cpp`

- [ ] **Step 1: Add a failing test for coalesced event draining and idle render requests**

```cpp
QVERIFY(item.usesWakeupCallbackForTest());
QCOMPARE(item.pendingEventDrainCountForTest(), 0);
QVERIFY(item.renderRequestsForTest() <= expectedFrameRequests);
```

- [ ] **Step 2: Run the focused test and observe timer-driven behavior**

Run: `ctest --preset windows-x64-debug -R mpv_quick_item_test --output-on-failure -j 1`

- [ ] **Step 3: Use libmpv's wakeup callback and remove unconditional renderer updates**

```cpp
mpv_set_wakeup_callback(mpv_, &MpvQuickItem::onMpvWakeup, renderState_.get());
if (!state_->eventDrainQueued.exchange(true)) {
    QMetaObject::invokeMethod(item, &MpvQuickItem::pollMpvEvents, Qt::QueuedConnection);
}
// no update() at the end of MpvRenderer::render()
```

- [ ] **Step 4: Re-run mpv and close-lifecycle tests**

Run: `ctest --preset windows-x64-debug -R "mpv_quick_item_test|qml_close_regression_test" --output-on-failure -j 1`

### Task 7: Prove nine-player lifecycle and protect ninth-room diagnosis

**Files:**
- Modify: `native/tests/qml_close_regression_test.cpp`
- Modify: `native/tests/multi_room_coordinator_test.cpp`
- Modify: `native/src/workspace/multi_room_coordinator.cpp`
- Modify: `native/src/ui/app_controller.cpp`

- [ ] **Step 1: Add failing nine-item render-context and safe-error tests**

```cpp
QTRY_COMPARE(readyRenderContexts(), 9);
QCOMPARE(coordinator.addRoomDetailed(QStringLiteral("999999")), RoomCommandResult::RoomLimitReached);
QVERIFY(!controller.fixedPlaybackMessage(QStringLiteral("SAFE_CODE")).contains("://"));
```

- [ ] **Step 2: Run the targeted tests and capture the failing lifecycle assertion if present**

Run: `ctest --preset windows-x64-debug -R "qml_close_regression_test|multi_room_coordinator_test" --output-on-failure -j 1`

- [ ] **Step 3: Repair only the discovered ninth-item lifecycle failure and retain the nine-room limit**

```cpp
if (order_.size() >= kMaxRooms) return RoomCommandResult::RoomLimitReached;
publishSnapshots();
```

- [ ] **Step 4: Re-run the targeted tests five times serially**

Run: `ctest --preset windows-x64-debug -R "qml_close_regression_test|multi_room_coordinator_test" --output-on-failure -j 1 --repeat until-pass:5`

### Task 8: Ship a frameless Windows GUI executable

**Files:**
- Modify: `native/CMakeLists.txt`
- Modify: `native/app/qml/Main.qml`
- Modify: `native/tests/qml_interaction_test.cpp`
- Modify: `native/tests/native_self_test_test.cpp`

- [ ] **Step 1: Add failing source/package assertions for GUI subsystem and frameless flag**

```cpp
QVERIFY(mainQml.contains("Qt.FramelessWindowHint"));
QVERIFY(peSubsystem(executablePath) == QStringLiteral("Windows GUI"));
```

- [ ] **Step 2: Run focused tests and inspect the current console subsystem**

Run: `ctest --preset windows-x64-debug -R "qml_interaction_test|native_self_test_test" --output-on-failure -j 1`

- [ ] **Step 3: Set the target GUI property and QML window interactions**

```cmake
set_target_properties(douyu_monitor_native PROPERTIES WIN32_EXECUTABLE TRUE)
```

```qml
flags: Qt.Window | Qt.FramelessWindowHint
TapHandler { onDoubleTapped: root.appController.toggleMaximizedWindow() }
DragHandler { onActiveChanged: if (active) root.startSystemMove() }
```

- [ ] **Step 4: Rebuild and re-run focused tests**

Run: `cmake --build --preset windows-x64-debug --target douyu_monitor_native qml_interaction_test native_self_test_test; ctest --preset windows-x64-debug -R "qml_interaction_test|native_self_test_test" --output-on-failure -j 1`

### Task 9: Final regression and release evidence

**Files:**
- Modify: `docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md`

- [ ] **Step 1: Run all Debug tests**

Run: `cmake --build --preset windows-x64-debug; ctest --preset windows-x64-debug -j 1 --output-on-failure`

- [ ] **Step 2: Run all Release tests and package validation**

Run: `cmake --build --preset windows-x64-release; ctest --preset windows-x64-release -j 1 --output-on-failure`

- [ ] **Step 3: Verify screenshots and the release executable**

Run: `ctest --preset windows-x64-release -R "qml_visual_smoke_test|native_self_test_media" --output-on-failure -j 1`

- [ ] **Step 4: Scan for prohibited runtimes and sensitive output**

Run: `rg -n "QWidget|QOpenGLWidget|Qt6::Widgets|QtWebEngine|Electron|Chromium|node\.exe|playbackUrl|cookie|token|signature" native docs/superpowers -g "!out/**"`

- [ ] **Step 5: Append a progress record, create a Notion child page, fetch it back, and compare it to this design and plan**

Record test counts, UI viewport evidence, ninth-player result, package result, and limits only. Do not record media source URLs, authentication material, or raw service diagnostics.
