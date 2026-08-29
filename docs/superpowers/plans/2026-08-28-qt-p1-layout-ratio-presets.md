# Qt 原生布局、比例与预设恢复实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 将旧 Electron 工作区的布局选择、主画面比例和预设恢复行为迁移到唯一的 Qt Quick/QML + C++ 运行时。

**Architecture:** `MultiRoomCoordinator` 负责布局模式和房间顺序，`WorkspaceModel` 暴露布局/比例给 QML，`AppController` 通过安全 invokable 修改并在预设应用时恢复。QML `WorkspaceGrid` 根据布局模式渲染普通网格、主画面分栏和两路分屏；敏感播放源仍只留在 C++。

**Tech Stack:** Qt 6 Quick/QML Controls、C++17、Qt Test、CMake。

---

### Task 1: 原生布局状态与预设恢复测试

**Files:** `native/tests/multi_room_coordinator_test.cpp`, `native/tests/app_controller_test.cpp`, `native/tests/qml_interaction_test.cpp`

- [x] Write failing tests for manual layout persistence, preset layout/ratio restoration, and QML layout controls.
- [ ] Run focused CTest and capture the expected failure.

### Task 2: C++ 布局与比例 API

**Files:** `native/src/workspace/multi_room_coordinator.*`, `native/src/workspace/native_workspace_types.h`, `native/src/ui/workspace_model.*`, `native/src/ui/app_controller.*`

- [x] Add a validated layout setter and primary ratio state; automatic mode alone recomputes recommended layouts when room count changes.
- [x] Save and restore layout, ratio, sidebar visibility, and room order in workspace presets.
- [x] Run focused coordinator/controller tests.

### Task 3: QML 布局菜单与主画面分栏

**Files:** `native/app/qml/Main.qml`, `native/app/qml/components/AppHeader.qml`, `native/app/qml/components/WorkspaceGrid.qml`, `native/app/qml/components/PrimaryRoomDivider.qml`

- [x] Add a selectable layout menu with accessible labels and controller-backed commands.
- [x] Render primary-focus and split layouts, including ratio stepping and pointer dragging.
- [x] Run QML interaction and visual smoke tests.

### Task 4: 全量验证、日志和计划对照

- [x] Build and run Debug/Release CTest.
- [x] Validate 1280x720 and 390x844 with Playwright/QML screenshots.
- [x] Run `git diff --check` and sensitive-output scans.
- [x] Append a redacted progress log and record the next audit row.

## Next audit item

- [ ] P1: expose complete global audio and danmaku controls in the QML header/settings surface, including global mute, single/multi-room audio mode, and global danmaku policy while preserving the nine-room and sensitive-data boundaries.
