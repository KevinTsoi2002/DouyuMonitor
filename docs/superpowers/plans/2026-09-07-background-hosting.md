# Background Hosting Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add Windows tray-backed background hosting that keeps audio, status monitoring, notifications, and StreamGet alive while video and danmaku rendering are suspended.

**Architecture:** Add a Windows-only `WindowsTrayService` using `Shell_NotifyIcon` and a hidden message window. `AppController` owns the hosting state and idempotent lifecycle commands; QML handles close/minimize interception and visual visibility. `RoomSession`/`MpvQuickItem` gain a render-only suspend/resume path so libmpv audio is preserved while Qt Quick rendering is detached.

**Tech Stack:** Qt 6 Gui/Qml/Quick, C++20, QML, Windows Shell/User32 APIs, Qt Test, existing libmpv and StreamGet services.

---

### Task 1: Add tray service abstraction

**Files:**
- Create: `native/src/app/windows_tray_service.h`
- Create: `native/src/app/windows_tray_service.cpp`
- Modify: `native/CMakeLists.txt:763-820`
- Test: `native/tests/windows_tray_service_test.cpp`

- [ ] **Step 1: Write the failing test**

Add a Qt Test fixture that constructs the service, calls `start()`, invokes the test-only `triggerShowForTest()` and `triggerQuitForTest()` hooks, and verifies `showRequested` and `quitRequested` each fire once. On non-Windows, assert `start()` is harmless and returns false.

- [ ] **Step 2: Run the focused test and verify it fails**

Run: `ctest --test-dir native/out/build/windows-x64-release -R windows_tray_service_test --output-on-failure`

Expected: FAIL because the service and target do not exist.

- [ ] **Step 3: Implement the service**

Define `bool start(QWindow *window)`, `void stop()`, `bool isRunning() const`, `showRequested()`, `quitRequested()`, and Windows-only test hooks. On Windows create a message-only window class, call `Shell_NotifyIcon(NIM_ADD)`, use `WM_APP + 1` for tray callbacks, and display a popup menu with IDs 1 (`显示窗口`) and 2 (`退出程序`). Use the existing `app_icon.rc` icon resource. Provide a no-op non-Windows implementation guarded by `#ifdef Q_OS_WIN`.

- [ ] **Step 4: Add build and test wiring**

Add the source to `douyu_monitor_native`, `app_controller_test`, and `qml_close_regression_test`; link `Shell32` and `User32` on Windows. Register the new test with `add_test`.

- [ ] **Step 5: Run the focused test and commit**

Run the same `ctest` command; expected PASS. Commit with `feat: add windows tray service`.

### Task 2: Add AppController hosting lifecycle

**Files:**
- Modify: `native/src/ui/app_controller.h`
- Modify: `native/src/ui/app_controller.cpp`
- Test: `native/tests/app_controller_test.cpp`

- [ ] **Step 1: Write failing lifecycle tests**

Add tests for `backgroundHosted` initial false, `closeToTray()` setting true without stopping the service, `restoreFromBackground()` setting false, and repeated `requestQuit()` remaining safe and stopping the service exactly once.

- [ ] **Step 2: Run tests to verify failure**

Run: `ctest --test-dir native/out/build/windows-x64-release -R app_controller_test --output-on-failure`

Expected: compile/test failure because the property and methods do not exist.

- [ ] **Step 3: Implement state and tray wiring**

Add `Q_PROPERTY(bool backgroundHosted READ backgroundHosted NOTIFY backgroundHostedChanged)`, a `std::unique_ptr<WindowsTrayService>`, `backgroundHosted_`, and `quitRequested_`. Implement `minimizeToBackground()`, `closeToTray()`, `restoreFromBackground()`, and `requestQuit()`. `requestQuit()` calls the existing `shutdown()` once and closes the main window; `closeToTray()` only suspends visual layers and hides the window. Connect tray signals to restore/request quit.

- [ ] **Step 4: Add render lifecycle calls**

Add private `suspendVisualLayers()` and `resumeVisualLayers()` that iterate coordinator room IDs, call the new coordinator render methods, and disable/enable danmaku presentation without stopping sessions.

- [ ] **Step 5: Run focused tests and commit**

Run: `ctest --test-dir native/out/build/windows-x64-release -R "app_controller_test|windows_tray_service_test" --output-on-failure`. Expected PASS. Commit with `feat: add background hosting lifecycle`.

### Task 3: Preserve libmpv audio while suspending rendering

**Files:**
- Modify: `native/src/ui/mpv_quick_item.h`
- Modify: `native/src/ui/mpv_quick_item.cpp`
- Modify: `native/src/workspace/room_session.h`
- Modify: `native/src/workspace/room_session.cpp`
- Modify: `native/src/workspace/multi_room_coordinator.h`
- Modify: `native/src/workspace/multi_room_coordinator.cpp`
- Test: `native/tests/mpv_quick_item_test.cpp`
- Test: `native/tests/multi_room_coordinator_test.cpp`

- [ ] **Step 1: Add failing render-suspension tests**

Test that `suspendRendering()` does not change playback state, mute, or volume; `resumeRendering()` is idempotent; coordinator suspend/resume preserves room IDs and audio focus.

- [ ] **Step 2: Implement `MpvQuickItem` render-only suspension**

Add `bool renderingSuspended() const`, `void suspendRendering()`, and `void resumeRendering()`. The suspend method marks a flag, stops frame update requests, and releases only the scene-graph render target; it must not call `stop()`, clear the media source, or destroy the mpv core. Resume recreates the render target/update requests and emits `renderContextReady()` so the existing pending-source path can rebind.

- [ ] **Step 3: Implement session/coordinator forwarding**

Add `suspendRendering()`/`resumeRendering()` to `RoomSession` and coordinator methods that call the attached player's methods when present. Do not alter `RoomSession::stop()` or `release()` semantics.

- [ ] **Step 4: Run focused tests and commit**

Run: `ctest --test-dir native/out/build/windows-x64-release -R "mpv_quick_item_test|multi_room_coordinator_test" --output-on-failure`. Expected PASS. Commit with `feat: suspend video rendering without stopping audio`.

### Task 4: Suspend and restore danmaku presentation

**Files:**
- Modify: `native/src/danmaku/danmaku_controller.h`
- Modify: `native/src/danmaku/danmaku_controller.cpp`
- Modify: `native/app/qml/components/DanmakuOverlay.qml`
- Modify: `native/src/ui/app_controller.cpp`
- Test: `native/tests/danmaku_governance_test.cpp`

- [ ] **Step 1: Write failing tests**

Verify a presentation-suspended controller keeps session state and queued messages, while `takeNextMessage()` is suppressed only for presentation and resumes after restore.

- [ ] **Step 2: Implement suspension flag**

Add `presentationSuspended_` with `setPresentationSuspended(bool)` and `presentationSuspended()`. Keep socket/session synchronization running; only suppress presentation consumption and emit a settings/state update on resume.

- [ ] **Step 3: Bind QML overlay**

Add a `presentationSuspended` property to `DanmakuOverlay`; stop launch timers and clear active visual items while suspended, then restart the timer on resume. Keep the controller session connected.

- [ ] **Step 4: Run focused tests and commit**

Run: `ctest --test-dir native/out/build/windows-x64-release -R "danmaku_governance_test|qml_visual_smoke_test" --output-on-failure`. Expected PASS. Commit with `feat: suspend danmaku presentation in background`.

### Task 5: Intercept QML close/minimize and connect tray actions

**Files:**
- Modify: `native/app/qml/Main.qml:48-58`
- Modify: `native/app/qml/components/WindowControls.qml`
- Modify: `native/src/ui/app_controller.cpp`
- Modify: `native/tests/qml_close_regression_test.cpp`

- [ ] **Step 1: Add failing QML lifecycle assertions**

Extend the close regression test to call `window->close()`, verify the window becomes hidden, `backgroundHosted` is true, and `serviceProcessRunningForTest()` remains true; invoke `controller.requestQuit()` and verify the process stops.

- [ ] **Step 2: Update QML close behavior**

Change `onClosing` to set `close.accepted = false` and call `appController.closeToTray()`. Add a `Connections` block for `backgroundHostedChanged` to keep QML state synchronized. Route the existing explicit close control to `requestQuit()` only when the user selects “退出程序” from the tray; the window close button itself hides to tray per the approved behavior.

- [ ] **Step 3: Handle minimize transitions**

Add a `Connections` handler for `visibilityChanged`; when visibility is `Window.Minimized` and the app is not quitting, call `appController.minimizeToBackground()` and hide the window. Restore the prior maximized/fullscreen state through the existing controller state.

- [ ] **Step 4: Run QML regression tests and commit**

Run: `ctest --test-dir native/out/build/windows-x64-release -R qml_close_regression_test --output-on-failure`. Expected PASS. Commit with `feat: route window close and minimize to tray hosting`.

### Task 6: Build, runtime smoke test, and documentation

**Files:**
- Modify: `native/README.md`
- Modify: `README.md`
- Modify: `native/CMakeLists.txt` if deployment rules need tray resources
- Test: existing full native test suite

- [ ] **Step 1: Configure and build Release**

Run: `cmake --build native/out/build/windows-x64-release --config Release --parallel`.

Expected: `douyu_monitor_native.exe` and `streamget_service.exe` build successfully.

- [ ] **Step 2: Run the full test suite**

Run: `ctest --test-dir native/out/build/windows-x64-release -C Release --output-on-failure`.

Expected: all existing tests plus the new tray/lifecycle tests pass.

- [ ] **Step 3: Run Windows runtime checks**

Launch the Release executable, start 1/4/6/9 rooms, minimize and restore, close to tray and restore, then choose tray “退出程序”. Confirm audio continues in background, no video/danmaku is rendered while hidden, video/danmaku recover on restore, notifications continue, child process exits, and no tray icon remains.

- [ ] **Step 4: Document behavior and commit**

Document tray menu, background audio policy, rendering suspension, and true-exit behavior in both READMEs. Commit with `docs: document background hosting behavior`.

