# Qt Native Group Switching Implementation Plan

> **For agentic workers:** Execute this plan task-by-task with focused tests and review checkpoints.

**Goal:** Make native group activation replace the active room set, expose group tabs in the sidebar, and complete group member management without leaving stale playback or danmaku sessions.

**Architecture:** `MultiRoomCoordinator` receives a validated ordered room specification and performs one batched replacement, releasing removed sessions before creating the target sessions. `AppController` resolves group membership from the persisted room library, delegates the replacement, and keeps active-group state, persistence, presentation, and danmaku synchronization aligned. QML consumes `WorkspaceModel.groups` for compact tabs and uses explicit controller commands for member removal and reordering.

**Tech Stack:** Qt 6.8, Qt Quick/QML, C++17, QtTest, CMake/Ninja.

---

### Task 1: Batched coordinator room replacement

**Files:**
- Modify: `native/src/workspace/multi_room_coordinator.h`
- Modify: `native/src/workspace/multi_room_coordinator.cpp`
- Modify: `native/tests/multi_room_coordinator_test.cpp`

- [ ] **Step 1: Add a failing replacement test.**

Add a test that creates three fake rooms, calls the new ordered replacement API with two different rooms, and asserts the old sessions are released, the new order is preserved, and the coordinator publishes only the target room IDs.

- [ ] **Step 2: Run the focused test and confirm it fails to compile.**

Run `cmake --build --preset windows-x64-debug --target multi_room_coordinator_test` and expect the replacement API to be absent.

- [ ] **Step 3: Implement the minimal batched API.**

Add an ordered `CoordinatorRoomSpec` value and `replaceRooms(const QVector<CoordinatorRoomSpec>&)`; validate IDs, duplicates, capacity, and client availability, release all existing sessions, create the requested sessions in order, restore favorite/volume values, recompute layout, schedule status requests, and publish one final snapshot.

- [ ] **Step 4: Run the focused test green.**

Run `ctest --preset windows-x64-debug -R '^multi_room_coordinator_test$' --output-on-failure`.

- [ ] **Step 5: Check the scoped diff.**

Run `git diff --check -- native/src/workspace/multi_room_coordinator.h native/src/workspace/multi_room_coordinator.cpp native/tests/multi_room_coordinator_test.cpp`.

### Task 2: AppController group activation and member commands

**Files:**
- Modify: `native/src/ui/app_controller.h`
- Modify: `native/src/ui/app_controller.cpp`
- Modify: `native/src/workspace/multi_room_coordinator.h`
- Modify: `native/tests/app_controller_test.cpp`

- [ ] **Step 1: Add failing controller tests.**

Cover group activation replacing the active room set and preserving the stored group order. Add member removal and move-up/move-down assertions against the persisted group data.

- [ ] **Step 2: Implement controller commands.**

Add `removeRoomFromGroup` and `moveRoomInGroup` invokables. Update `setActiveGroup` to build ordered specs from library records, call `replaceRooms`, set audio focus to the first target room, update `activeGroupId`, and persist. Restore an active group from its room IDs during startup when present.

- [ ] **Step 3: Run controller and coordinator tests.**

Run `ctest --preset windows-x64-debug -R '^(app_controller_test|multi_room_coordinator_test)$' --output-on-failure`.

- [ ] **Step 4: Check the scoped diff.**

Run `git diff --check -- native/src/ui/app_controller.h native/src/ui/app_controller.cpp native/tests/app_controller_test.cpp`.

### Task 3: Sidebar group tabs and complete member management UI

**Files:**
- Modify: `native/app/qml/Main.qml`
- Modify: `native/app/qml/components/RoomSidebar.qml`
- Modify: `native/app/qml/dialogs/GroupManagerDialog.qml`
- Modify: `native/tests/qml_interaction_test.cpp`

- [ ] **Step 1: Add failing QML interaction assertions.**

Assert that group tabs render from the workspace model, activating a tab calls `setActiveGroup`, and the manager exposes member rows with remove, up, and down actions.

- [ ] **Step 2: Implement compact group tabs.**

Pass `workspaceModel` into `RoomSidebar`; render up to three named tabs plus an overflow menu and keep the current/history/favorites views separate. Clicking a group switches to current view and calls the controller.

- [ ] **Step 3: Implement member controls.**

In `GroupManagerDialog`, show the selected group's `roomIds` as rows, provide remove/up/down buttons, and call the new controller invokables. Keep the existing add-room assignment flow.

- [ ] **Step 4: Run focused QML tests and inspect screenshots.**

Run `ctest --preset windows-x64-debug -R '^(qml_interaction_test|qml_visual_smoke_test)$' --output-on-failure` and inspect the generated shell screenshot.

### Task 4: Full verification and records

**Files:**
- Modify: `docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md`
- Create: one sanitized Notion progress page

- [ ] **Step 1: Run complete Debug and Release CTest suites.**

Run `ctest --preset windows-x64-debug --output-on-failure` and `ctest --preset windows-x64-release --output-on-failure`.

- [ ] **Step 2: Compare implementation against the audit row.**

Verify active group replacement, group tabs, member removal/reordering, persistence, and release of removed playback/danmaku sessions. Record any live-network limitation separately.

- [ ] **Step 3: Append a sanitized local log and create/fetch the Notion page.**

Record test totals, screenshots, changed files, plan alignment, and the next audit priority. Do not include URLs, tokens, cookies, playback sources, raw chat, or diagnostics.

- [ ] **Step 4: Run final integrity checks.**

Run `git diff --check` and `git status --short`; do not reset or remove unrelated worktree changes.
