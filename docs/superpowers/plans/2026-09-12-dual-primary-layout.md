# 双主直播间布局 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 4–10 路活动直播间范围内提供可切换、可持久化、可分别指定两个主直播间的双主布局。

**Architecture:** 在现有 `MultiRoomCoordinator` 单主状态上增加第二主 ID，并让质量策略、快照和预设统一读取两个主 ID。QML `WorkspaceGrid` 负责双主几何，`AppHeader` 负责数量门槛，`RoomTile` 负责两个主画面选择；存储层以新增 JSON 字段兼容旧快照。

**Tech Stack:** Qt 6.8, C++17, Qt Quick/QML, Qt Test, CMake/CTest。

---

### Task 1: 扩展领域状态与失败测试

**Files:**
- Modify: `native/src/workspace/multi_room_coordinator.h/.cpp`
- Modify: `native/src/workspace/native_workspace_types.h`
- Modify: `native/src/ui/workspace_model.h/.cpp`
- Test: `native/tests/multi_room_coordinator_test.cpp`
- Test: `native/tests/workspace_model_test.cpp`

- [ ] **Step 1: Write failing coordinator tests**

添加测试：4 路房间时 `setSecondaryPrimaryRoomDetailed` 可选第二主；两个主房间均获得 `StreamQuality::Original`；删除第二主后自动补位；3 路时双主布局自动归一为 `auto`。

- [ ] **Step 2: Run coordinator tests to verify failure**

Run: `ctest --test-dir native/out -R "multi_room_coordinator_test|workspace_model_test" --output-on-failure`

Expected: 编译或测试失败，原因是第二主 API/状态尚不存在。

- [ ] **Step 3: Add second-primary state and API**

在协调器增加 `secondaryPrimaryRoomId_`、getter、`setSecondaryPrimaryRoomDetailed`，快照中扩展 `RoomSnapshot.primary` 之外的主标记传递；质量计算改为 `roomId == primaryRoomId_ || roomId == secondaryPrimaryRoomId_`。新增房间/删除房间/替换房间时保持两个主 ID 有效且不重复，双主模式低于 4 路时切回 `auto`。

- [ ] **Step 4: Add workspace model property and presentation plumbing**

增加 `secondaryPrimaryRoomId` Q_PROPERTY、通知信号及 `setCoordinatorState` 参数，更新 `AppController::refreshPresentation` 调用链。

- [ ] **Step 5: Run focused tests**

Run: `ctest --test-dir native/out -R "multi_room_coordinator_test|workspace_model_test" --output-on-failure`

Expected: 新增测试与原有测试通过。

- [ ] **Step 6: Commit**

```powershell
git add native/src/workspace native/src/ui native/tests/multi_room_coordinator_test.cpp native/tests/workspace_model_test.cpp
git commit -m "feat: add dual primary room state"
```

### Task 2: 持久化、预设与控制器门槛

**Files:**
- Modify: `native/src/workspace/native_workspace_store.cpp`
- Modify: `native/src/ui/app_controller.cpp/.h`
- Modify: `native/src/workspace/native_workspace_types.h`
- Test: `native/tests/native_workspace_store_test.cpp`
- Test: `native/tests/app_controller_test.cpp`

- [ ] **Step 1: Write failing persistence/controller tests**

覆盖 `secondaryPrimaryRoomId` JSON 保存/加载、旧 JSON 缺失字段兼容、预设恢复两个主 ID、3 路时 `setLayout("primary-two")` 返回失败并保持自动布局。

- [ ] **Step 2: Run tests to verify failure**

Run: `ctest --test-dir native/out -R "native_workspace_store_test|app_controller_test" --output-on-failure`

Expected: 新字段断言失败或接口缺失。

- [ ] **Step 3: Implement persistence and preset fields**

在 `NativeWorkspacePreset`/`NativeWorkspaceSnapshot` 增加字段；`toJson`、解析和 normalize 校验字段必须属于对应房间列表且不能与第一主重复。旧版本字段缺失时置空。

- [ ] **Step 4: Implement controller APIs and fallback message**

增加 `AppController::setSecondaryPrimaryRoom`，`setLayout` 对 `primary-two` 做 4 路门槛校验；删除或快照变化后低于 4 路自动调用 `setLayout("auto")` 并设置状态消息。保存/应用预设同步第二主 ID。

- [ ] **Step 5: Run focused tests**

Run: `ctest --test-dir native/out -R "native_workspace_store_test|app_controller_test" --output-on-failure`

Expected: 相关测试通过。

- [ ] **Step 6: Commit**

```powershell
git add native/src/workspace native/src/ui native/tests/native_workspace_store_test.cpp native/tests/app_controller_test.cpp
git commit -m "feat: persist dual primary layout state"
```

### Task 3: 工作区上限与双主几何

**Files:**
- Modify: `native/src/workspace/multi_room_coordinator.h`
- Modify: `native/src/workspace/native_workspace_store.cpp`
- Modify: `native/app/qml/components/WorkspaceGrid.qml`
- Modify: `native/app/qml/Main.qml`
- Test: `native/tests/qml_interaction_test.cpp`

- [ ] **Step 1: Write failing QML geometry tests**

创建 4–10 路模型并设置 `layoutMode: "primary-two"`，断言主画面 1/2 位于顶部、宽度相等、其余房间位于底部且最多 10 路；断言 3 路不进入双主布局。

- [ ] **Step 2: Run QML test to verify failure**

Run: `ctest --test-dir native/out -R qml_interaction_test --output-on-failure`

Expected: 当前网格将 `primary-two` 当作自动布局或只显示单主，几何断言失败。

- [ ] **Step 3: Raise active-room limits**

将协调器 `kMaxRooms`、存储层活动/分组/预设上限统一改为 10，并更新空状态文案为“最多容纳 10 路”。

- [ ] **Step 4: Implement dual-primary geometry**

在 `WorkspaceGrid.qml` 增加 `secondaryPrimaryRoomId` 属性和双主索引；对 4–10 路生成顶部两块大区域及底部剩余网格，保持稳定间距和边界尺寸。主房间缺失时按模型顺序补位。

- [ ] **Step 5: Wire Main.qml**

把工作区模型的第二主 ID传给 `WorkspaceGrid`。

- [ ] **Step 6: Run QML tests**

Run: `ctest --test-dir native/out -R qml_interaction_test --output-on-failure`

Expected: 双主几何及既有 QML 测试通过。

- [ ] **Step 7: Commit**

```powershell
git add native/src/workspace native/app/qml native/tests/qml_interaction_test.cpp
git commit -m "feat: render dual primary workspace layout"
```

### Task 4: 布局菜单与房间操作

**Files:**
- Modify: `native/app/qml/components/AppHeader.qml`
- Modify: `native/app/qml/components/RoomTile.qml`
- Modify: `native/app/qml/Main.qml`
- Test: `native/tests/qml_interaction_test.cpp`

- [ ] **Step 1: Write failing menu/action tests**

断言菜单出现“双主直播间”、4 路以下禁用、4 路以上可选；双主模式房间菜单出现“设为主画面 1/2”，当前两个房间分别显示对应状态。

- [ ] **Step 2: Run test to verify failure**

Run: `ctest --test-dir native/out -R qml_interaction_test --output-on-failure`

Expected: 菜单项和操作对象不存在。

- [ ] **Step 3: Add menu option and disabled state**

在 `AppHeader.qml` 增加 `dualPrimaryLayoutOption`，根据 `rooms.rowCount >= 4` 设置 `enabled`，触发 `setLayout("primary-two")`，并为禁用状态提供工具提示。

- [ ] **Step 4: Add two primary actions**

在 `RoomTile.qml` 保留单主按钮；双主模式显示两个明确按钮，分别调用 `setPrimaryRoom`/`setSecondaryPrimaryRoom`，当前状态显示“当前主画面 1/2”。

- [ ] **Step 5: Run QML tests**

Run: `ctest --test-dir native/out -R qml_interaction_test --output-on-failure`

Expected: 菜单和房间操作测试通过。

- [ ] **Step 6: Commit**

```powershell
git add native/app/qml native/tests/qml_interaction_test.cpp
git commit -m "feat: add dual primary layout controls"
```

### Task 5: 全量验证与文档

**Files:**
- Modify: `README.md` or `native/README.md` only if layout list is documented
- Test: all CTest targets

- [ ] **Step 1: Configure/build**

Run: `cmake --build native/out --config Debug --parallel 4`

Expected: exit code 0。

- [ ] **Step 2: Run full test suite**

Run: `ctest --test-dir native/out -C Debug --output-on-failure`

Expected: all tests pass, 0 failures。

- [ ] **Step 3: Run QML smoke test**

Run: `ctest --test-dir native/out -C Debug -R "qml_(engine|visual|interaction)_" --output-on-failure`

Expected: UI 资源加载和双主菜单均通过。

- [ ] **Step 4: Inspect diff and status**

Run: `git diff --check; git status --short`

Expected: 无空白错误；仅包含本功能改动和已有用户未跟踪诊断文件。

- [ ] **Step 5: Commit verification/doc updates**

```powershell
git add README.md native/README.md
git commit -m "docs: describe dual primary layout"
```
