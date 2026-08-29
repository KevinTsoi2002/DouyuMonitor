# Qt P1 Global Audio and Danmaku Controls Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (recommended) to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore the audited P1 global mute, single/multi-room audio mode, and global danmaku policy controls in the Qt Quick/QML runtime with workspace and preset persistence.

**Architecture:** Keep `AppController` as the only application orchestrator. `MultiRoomCoordinator` owns validated audio mode and mute application to attached libmpv players; `WorkspaceModel` exposes safe state to QML; `NativeWorkspaceStore` persists the new fields with a backward-compatible `single`/unmuted default. `DanmakuController` remains the sole QML boundary for the global danmaku master setting and continues to synchronize eligible sessions.

**Tech Stack:** Qt 6.8 Quick/QML Controls, C++17, libmpv, QtTest, CMake/Ninja.

---

### Task 1: Define the failing state and persistence contract

**Files:**
- Modify: `native/tests/workspace_model_test.cpp`
- Modify: `native/tests/native_workspace_store_test.cpp`
- Modify: `native/tests/multi_room_coordinator_test.cpp`
- Modify: `native/tests/app_controller_test.cpp`

- [ ] **Step 1: Add a failing workspace-model test.**

Assert that the model exposes `audioMode` with `single` as the default, emits `audioModeChanged` when changed, and exposes a writable global mute state through controller-backed commands rather than a QML-only property mutation.

- [ ] **Step 2: Add failing store migration and preset round-trip tests.**

Cover version 3 snapshots and presets containing `audioMode` and `globalMuted`, plus legacy snapshots without either field loading as `single` and `false`.

- [ ] **Step 3: Add failing coordinator policy tests.**

Create two fake rooms with attached fake players, assert single mode unmutes only the selected audio room, multi mode unmutes both rooms, and global mute mutes both without clearing the selected audio room.

- [ ] **Step 4: Add failing AppController command tests.**

Call `setAudioMode("multi")` and `setGlobalMuted(true)`, assert workspace roles update, the snapshot persists both values, and invalid audio modes are rejected without changing state.

- [ ] **Step 5: Run the focused tests and record the expected compile failures.**

Run `cmake --build --preset windows-x64-debug --target workspace_model_test native_workspace_store_test multi_room_coordinator_test app_controller_test`.

Expected failure: the new properties, setters, coordinator policy, and controller invokables do not yet exist.

### Task 2: Implement validated native audio state and persistence

**Files:**
- Modify: `native/src/workspace/native_workspace_types.h`
- Modify: `native/src/workspace/native_workspace_store.cpp`
- Modify: `native/src/workspace/multi_room_coordinator.h`
- Modify: `native/src/workspace/multi_room_coordinator.cpp`
- Modify: `native/src/ui/workspace_model.h`
- Modify: `native/src/ui/workspace_model.cpp`

- [ ] **Step 1: Add normalized fields.**

Add `QString audioMode = "single"` and `bool globalMuted = false` to `NativeWorkspaceSnapshot` and `NativeWorkspacePreset`. Normalize unknown modes to `single` and preserve the existing version-1/2 load behavior.

- [ ] **Step 2: Persist and restore the fields.**

Serialize both fields in snapshot and preset objects, reject unknown keys as before, and ensure `applyWorkspacePreset` restores them before publishing room snapshots.

- [ ] **Step 3: Add coordinator policy methods.**

Add `audioMode()`, `globalMuted()`, `setAudioMode(QString)`, and `setGlobalMuted(bool)`. `applyAudioFocus()` must compute audibility as `!globalMuted && (audioMode == "multi" || roomId == audioRoomId)`, while preserving the selected `audioRoomId` when global mute is toggled.

- [ ] **Step 4: Bind model signals.**

Expose `Q_PROPERTY(QString audioMode ...)` and `Q_PROPERTY(bool globalMuted ...)`, add change signals, and update the coordinator-state setter so QML sees the restored values without reopening panels.

- [ ] **Step 5: Run native focused tests.**

Run `ctest --preset windows-x64-debug -R "^(workspace_model_test|native_workspace_store_test|multi_room_coordinator_test|app_controller_test)$" --output-on-failure -j 1`.

### Task 3: Add AppController commands and QML controls

**Files:**
- Modify: `native/src/ui/app_controller.h`
- Modify: `native/src/ui/app_controller.cpp`
- Modify: `native/app/qml/components/AppHeader.qml`
- Modify: `native/app/qml/panels/DanmakuSettingsPanel.qml`
- Modify: `native/tests/qml_interaction_test.cpp`

- [ ] **Step 1: Add controller invokables.**

Implement `setGlobalMuted(bool)` and `setAudioMode(QString)` with validation, persistence, coordinator synchronization, and fixed command feedback. A mode change must never recreate a healthy player.

- [ ] **Step 2: Add compact header controls.**

Add icon-backed global mute and audio-mode controls beside the existing header actions. Use accessible names/tooltips, preserve the current selected audio room, and bind checked/pressed state to `WorkspaceModel`.

- [ ] **Step 3: Surface the global danmaku policy.**

Keep the existing `DanmakuController.globalEnabled` master toggle in the settings panel, but ensure the control updates immediately from `settingsChanged` and that its state is included in preset application.

- [ ] **Step 4: Extend QML interaction coverage.**

Assert that the header exposes the mute and audio-mode controls, toggling them calls the controller, switching modes changes the label from single to multi, and the danmaku master toggle remains available.

- [ ] **Step 5: Run QML focused tests and inspect screenshots.**

Run `ctest --preset windows-x64-debug -R "^(qml_interaction_test|qml_visual_smoke_test)$" --output-on-failure -j 1` and inspect the regenerated `native/out/verification/qml/shell-1280x720.png`.

### Task 4: Full verification, audit update, and Notion readback

**Files:**
- Modify: `docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md`
- Modify: `docs/superpowers/plans/2026-08-28-qt-p1-global-controls.md`
- Create: one sanitized Notion child page under the approved Qt migration design page

- [x] **Step 1: Run complete Debug and Release CTest suites.**

Run `ctest --preset windows-x64-debug --output-on-failure -j 1` and `ctest --preset windows-x64-release --output-on-failure -j 1` after rebuilding both configurations.

- [x] **Step 2: Run integrity checks.**

Run `git diff --check`, scan logs and test artifacts for playback/authentication material, verify the 1280x720, 1920x1080, and narrow-window screenshots, and confirm no native app or resolver service process remains.

- [x] **Step 3: Compare against the audit row.**

Record global mute precedence, single/multi mode, danmaku master synchronization, preset round-trip behavior, nine-room preservation, and the remaining real-network/manual validation boundary.

- [x] **Step 4: Write and fetch the Notion page.**

Create a redacted child page under `2026-08-26 Qt Quick/QML Electron UI 完整迁移设计`, fetch it back, and append its URL to the local progress log. Do not include source URLs, cookies, tokens, signatures, raw frames, or raw diagnostics.

### Task 4 verification record (2026-08-29)

- Debug complete CTest: 28/28 passed (175.08 s).
- Release rebuild and complete CTest: 28/28 passed (114.04 s).
- `git diff --check` completed without content errors; existing LF/CRLF warnings remain informational.
- Regenerated 1280x720, 1920x1080, and narrow 960x844 QML screenshots were inspected; no native application or resolver-service process remained.
- A sanitized Notion child page was created and fetched back: <https://app.notion.com/p/3cb0f4bdec4881eb8ab4f328ccf660ab?pvs=204>.
- Tasks 1-3 remain unchecked because their original pre-implementation red-run records were not retained in this plan. Their implemented behavior is covered by the complete Debug and Release suites above.
