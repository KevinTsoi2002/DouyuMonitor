# 房间列表、收藏排序与多分组 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** 让房间列表具备可见的拖拽插入提示与稳定对齐，收藏支持按收藏时间与手动排序，多分组支持完整的成员选择和持久化管理。

**Architecture:** C++ 的 NativeWorkspaceSnapshot 是收藏顺序和分组成员关系的唯一持久来源；AppController 负责校验、排序及更新工作区。QML 仅维护瞬时拖拽预览并调用控制器，收到模型更新后复位到模型定义的位置。

**Tech Stack:** C++20、Qt 6 Core/Quick/QML/Qt Test、CMake/CTest、QML。

---

## 文件职责

- native/src/workspace/native_workspace_types.h：保存收藏时间和排序键。
- native/src/workspace/native_workspace_store.cpp：工作区 JSON V4 保存、读取和 V1-V3 兼容迁移。
- native/src/ui/app_controller.{h,cpp}：收藏状态、手动重排、多分组和库投影。
- native/src/ui/room_list_model.{h,cpp}：向展示层提供多个分组归属。
- native/app/qml/components/RoomSidebar.qml：当前房间列表拖拽预览、插入线与拖拽复位。
- native/app/qml/components/RoomLibraryView.qml：收藏视图的派生排序和拖拽排序。
- native/app/qml/components/RoomTile.qml：顶部信息区与操作区的布局隔离。
- native/app/qml/dialogs/GroupManagerDialog.qml：分组成员库选择、容量和操作反馈。
- native/tests/native_workspace_store_test.cpp：迁移与收藏字段持久化。
- native/tests/app_controller_test.cpp：收藏排序、手动重排、多分组与容量行为。
- native/tests/qml_interaction_test.cpp：拖拽、布局、分组管理的 QML 合约。

## Task 1: 收藏字段与安全迁移

**Files:**
- Modify: native/src/workspace/native_workspace_types.h:35-45
- Modify: native/src/workspace/native_workspace_store.cpp:16-18,205-282,297-307,583-613,674-733
- Modify: native/tests/native_workspace_store_test.cpp:55-68

- [ ] **Step 1: 写出失败的工作区迁移与轮转测试**

在 NativeWorkspaceStoreTest 声明并实现：

~~~cpp
void roundTripsFavoriteAddedTimeAndManualOrder();
void migratesVersionThreeFavoritesWithoutLosingMembership();
~~~

第一个测试保存并读回 favoriteAddedAtMs = 1'700'000'000'010 与 favoriteSortOrder = 20。第二个测试写入 V3 JSON，其中 favorite 为 true，验证加载后仍为收藏、时间回退为 lastOpenedAtMs、排序键非零且重存后版本为 4。

- [ ] **Step 2: 运行测试，确认当前实现失败**

Run: cmake --build --preset windows-x64-debug --target native_workspace_store_test; ctest --preset windows-x64-debug -R "^native_workspace_store_test$" --output-on-failure

Expected: 编译失败，提示 NativeRoomRecord 没有 favoriteAddedAtMs 或 favoriteSortOrder。

- [ ] **Step 3: 最小化实现 V4 字段与迁移**

在记录类型增加：

~~~cpp
qint64 favoriteAddedAtMs = 0;
qint64 favoriteSortOrder = 0;
~~~

将 kCurrentVersion 改为 4。JSON 保存写入两字段；V4 读取要求数字。V1-V3 读取时：收藏记录使用 qMax<qint64>(1, lastOpenedAtMs) 初始化收藏时间和排序键，未收藏记录保持零。归一化清空未收藏记录字段；收藏记录将非正值修正为稳定正值，且不移除一个房间在多个分组中的成员关系。

- [ ] **Step 4: 运行迁移测试，确认通过**

Run: cmake --build --preset windows-x64-debug --target native_workspace_store_test; ctest --preset windows-x64-debug -R "^native_workspace_store_test$" --output-on-failure

Expected: native_workspace_store_test 通过，旧版本测试仍通过。

- [ ] **Step 5: 提交该原子变更**

~~~powershell
git add native/src/workspace/native_workspace_types.h native/src/workspace/native_workspace_store.cpp native/tests/native_workspace_store_test.cpp
git commit -m "feat: persist favorite ordering metadata"
~~~

## Task 2: 控制器收藏排序与多分组语义

**Files:**
- Modify: native/src/ui/app_controller.h:52-83
- Modify: native/src/ui/app_controller.cpp:151-181,352-367,442-546,959-990,1040-1046
- Modify: native/src/ui/room_list_model.{h,cpp}
- Modify: native/tests/app_controller_test.cpp:185-210,445-549

- [ ] **Step 1: 写出失败的控制器测试**

在 AppControllerTest 增加：

~~~cpp
void ordersFavoritesByAddedTimeAndPersistsManualOrder();
void keepsRoomInMultipleGroupsAndHonorsGroupCapacity();
~~~

第一个测试依次收藏两个已在库中的房间，验证 libraryRooms() 中收藏项按最新收藏时间优先；调用 moveFavoriteRoom("63136", 0) 后验证排序变化，重新创建控制器后顺序不变。第二个测试将 63136 加入 A 和 B，验证两个组都保留该 ID；再尝试向 A 加入第十个成员，验证返回“分组最多包含 9 个房间”，成员顺序未变。

- [ ] **Step 2: 运行测试，确认当前实现失败**

Run: cmake --build --preset windows-x64-debug --target app_controller_test; ctest --preset windows-x64-debug -R "^app_controller_test$" --output-on-failure

Expected: 编译失败，提示 moveFavoriteRoom 不存在；多分组断言失败，因为 assignRoomToGroup 会移除其他组成员。

- [ ] **Step 3: 实现控制器接口与投影规则**

在头文件公开：

~~~cpp
Q_INVOKABLE QString moveFavoriteRoom(const QString &roomId, int targetIndex);
~~~

实现要求：

- setFavorite(roomId, true) 只在状态变化时写入 QDateTime::currentMSecsSinceEpoch()，并分配大于现有收藏排序键的键值。
- setFavorite(roomId, false) 清空两个收藏字段，发射 libraryRoomsChanged() 并持久化。
- moveFavoriteRoom 仅接受已收藏房间，将“收藏排序键降序、收藏时间降序、roomId 升序”得到的列表移动到 targetIndex，再将排序键重写为 count - index；操作后持久化并发射 libraryRoomsChanged()。
- libraryRooms() 投影附带 favoriteAddedAtMs 与 favoriteSortOrder；历史页保持 lastOpenedAtMs 排序。
- assignRoomToGroup 删除全局 removeAll(roomId) 循环；若目标组已有房间返回成功但不追加；若已有九个成员返回“分组最多包含 9 个房间”。
- 用 QStringList groupIdsForRoom(const QString &roomId) const 取代单一分组查询，并让 RoomPresentationSettings 与 RoomListModel 使用 QStringList 角色。

- [ ] **Step 4: 运行控制器与模型测试，确认通过**

Run: cmake --build --preset windows-x64-debug --target app_controller_test room_list_model_test; ctest --preset windows-x64-debug -R "^(app_controller_test|room_list_model_test)$" --output-on-failure

Expected: 两个测试通过，且无敏感播放字段进入库投影或持久化数据。

- [ ] **Step 5: 提交该原子变更**

~~~powershell
git add native/src/ui/app_controller.h native/src/ui/app_controller.cpp native/src/ui/room_list_model.h native/src/ui/room_list_model.cpp native/tests/app_controller_test.cpp
git commit -m "feat: support favorite reordering and multi-group rooms"
~~~

## Task 3: 房间列表与收藏列表拖拽反馈

**Files:**
- Modify: native/app/qml/components/RoomSidebar.qml:25-47,247-416
- Modify: native/app/qml/components/RoomLibraryView.qml:16-141
- Modify: native/tests/qml_interaction_test.cpp:238-273,956-1101

- [ ] **Step 1: 写出失败的 QML 合约测试**

扩展 FakeRoomController：

~~~cpp
Q_INVOKABLE QString moveFavoriteRoom(const QString &roomId, int targetIndex) {
    movedFavoriteRoom = roomId;
    movedFavoriteIndex = targetIndex;
    return {};
}
~~~

新增测试槽：

~~~cpp
void exposesRoomDropInsertionIndicatorAndResetsDraggedRow();
void exposesFavoriteReorderSurface();
~~~

第一个测试建立两行 RoomListModel，检查每行有 roomDropInsertionIndicator，将侧栏的 draggedRoomId 与 dropInsertionIndex 写入测试值后验证插入线可见，并调用委托复位函数后验证 x == 0、y == 0、状态清空。第二个测试创建收藏视图，验证仅收藏项存在拖动手柄与落点区域，且落下调用 moveFavoriteRoom。

- [ ] **Step 2: 运行测试，确认当前实现失败**

Run: cmake --build --preset windows-x64-debug --target qml_interaction_test; ctest --preset windows-x64-debug -R "^qml_interaction_test$" --output-on-failure

Expected: roomDropInsertionIndicator 和收藏拖拽对象不存在。

- [ ] **Step 3: 实现统一拖拽会话与收藏拖拽**

在 RoomSidebar.qml 增加：

~~~qml
property string draggedRoomId: ""
property int dropInsertionIndex: -1

function resetRoomDrag(row) {
    row.x = 0
    row.y = 0
    draggedRoomId = ""
    dropInsertionIndex = -1
}
~~~

每个房间委托显示名为 roomDropInsertionIndicator 的 2px Rectangle。DropArea 根据 drop.y < height / 2 计算插入索引，调用 controller.moveRoom(source.roomId, finalIndex - source.index)，并在 drop/release/cancel 路径调用 resetRoomDrag。源行降低 opacity，但不改变模型。

在 RoomLibraryView.qml 过滤收藏数组，并按 favoriteSortOrder 降序、favoriteAddedAtMs 降序、roomId 升序排序。添加同样的局部拖拽状态和插入线，落下调用 controller.moveFavoriteRoom(source.roomId, finalIndex)。历史模式不提供排序手柄。

- [ ] **Step 4: 运行 QML 交互测试，确认通过**

Run: cmake --build --preset windows-x64-debug --target qml_interaction_test; ctest --preset windows-x64-debug -R "^qml_interaction_test$" --output-on-failure

Expected: qml_interaction_test 通过；既有上下移动按钮测试继续通过。

- [ ] **Step 5: 提交该原子变更**

~~~powershell
git add native/app/qml/components/RoomSidebar.qml native/app/qml/components/RoomLibraryView.qml native/tests/qml_interaction_test.cpp
git commit -m "feat: add room and favorite drag ordering feedback"
~~~

## Task 4: 紧凑房间卡片的文本与操作区隔离

**Files:**
- Modify: native/app/qml/components/RoomTile.qml:144-184,237-345
- Modify: native/tests/qml_interaction_test.cpp:261-266,832-850

- [ ] **Step 1: 写出失败的窄宽布局测试**

新增：

~~~cpp
void reservesTopBarSpaceForActionsAtNarrowWidths();
~~~

创建宽度 220 的 RoomTile，填入 180 个中文字符的分类和观看人数。断言 roomTopMetadata 的右边不超过 roomTopActions 左边减去 6px，且次级文本对象的 elide 为 Text.ElideRight，或在宽度不足时不可见。

- [ ] **Step 2: 运行测试，确认当前实现失败**

Run: cmake --build --preset windows-x64-debug --target qml_interaction_test; ctest --preset windows-x64-debug -R "^qml_interaction_test$" --output-on-failure

Expected: roomTopMetadata 和 roomTopActions 对象不存在，或几何断言失败。

- [ ] **Step 3: 最小化重构顶部栏锚点**

将顶部栏显式拆为：

~~~qml
Row {
    id: roomTopActions
    anchors.right: parent.right
    anchors.rightMargin: 7
    anchors.verticalCenter: parent.verticalCenter
    width: 25
}
Row {
    id: roomTopMetadata
    anchors.left: parent.left
    anchors.right: roomTopActions.left
    anchors.rightMargin: 6
    anchors.verticalCenter: parent.verticalCenter
    clip: true
}
~~~

状态标签保留固定隐式宽度；分类和观看人数使用剩余宽度与 Text.ElideRight。空间不足时先隐藏观看人数、再隐藏分类，菜单按钮始终可点。保留底栏、播放器、弹幕与菜单 z 层级。

- [ ] **Step 4: 运行 QML 交互测试，确认通过**

Run: cmake --build --preset windows-x64-debug --target qml_interaction_test; ctest --preset windows-x64-debug -R "^qml_interaction_test$" --output-on-failure

Expected: 新窄宽测试与既有长标题测试均通过。

- [ ] **Step 5: 提交该原子变更**

~~~powershell
git add native/app/qml/components/RoomTile.qml native/tests/qml_interaction_test.cpp
git commit -m "fix: prevent room metadata from overlapping controls"
~~~

## Task 5: 完整多分组管理界面

**Files:**
- Modify: native/app/qml/dialogs/GroupManagerDialog.qml:21-253
- Modify: native/tests/qml_interaction_test.cpp:17-39,254-256,518-591

- [ ] **Step 1: 写出失败的分组管理 QML 测试**

扩展 FakeGroupController：

~~~cpp
Q_INVOKABLE QString assignRoomToGroup(const QString &roomId, const QString &groupId) {
    assignedMembers.push_back(groupId + ":" + roomId);
    return {};
}
~~~

新增：

~~~cpp
void selectsLibraryRoomsForGroupMembership();
void showsGroupCapacityAndCommandFeedback();
~~~

第一个测试注入两个库房间与一个有一名成员的分组，确认 groupLibraryList 显示两个候选、已有成员显示“已加入”、未加入项点击后调用 assignRoomToGroup。第二个测试确认 groupCapacityLabel 显示 1 / 9，假控制器返回错误时 groupManagerStatus 显示错误文字。

- [ ] **Step 2: 运行测试，确认当前实现失败**

Run: cmake --build --preset windows-x64-debug --target qml_interaction_test; ctest --preset windows-x64-debug -R "^qml_interaction_test$" --output-on-failure

Expected: groupLibraryList、groupCapacityLabel 与 groupManagerStatus 对象不存在。

- [ ] **Step 3: 实现分组库选择、容量与反馈**

增加：

~~~qml
property string statusMessage: ""
property bool statusIsError: false

function runCommand(result) {
    statusMessage = result || "已保存"
    statusIsError = result.length > 0
}
~~~

分组列表显示 modelData.name + "  " + modelData.roomIds.length + " / 9"。成员区保留上移、下移和移除。新增 groupLibraryList，以 libraryRooms 为模型，行内计算 selectedGroup().roomIds.includes(entry.roomId)；已加入项展示“已加入”，未加入项提供“加入”按钮。调用 assignRoomToGroup 后将返回值交给 runCommand。容量达到 9 时禁用未加入项的加入按钮，并显示“分组最多包含 9 个房间”。保留输入框，但改为筛选库项目，不再是唯一添加方式。

- [ ] **Step 4: 运行分组交互和控制器回归测试，确认通过**

Run: cmake --build --preset windows-x64-debug --target qml_interaction_test app_controller_test; ctest --preset windows-x64-debug -R "^(qml_interaction_test|app_controller_test)$" --output-on-failure

Expected: 分组成员选择、容量反馈、多分组持久化和既有分组切换测试全部通过。

- [ ] **Step 5: 提交该原子变更**

~~~powershell
git add native/app/qml/dialogs/GroupManagerDialog.qml native/tests/qml_interaction_test.cpp
git commit -m "feat: complete multi-group management workflow"
~~~

## Task 6: 集成验证与视觉核查

**Files:**
- Modify only if verification reveals a demonstrated defect in one of the files above.

- [ ] **Step 1: 构建并运行目标回归集**

Run:

~~~powershell
cmake --build --preset windows-x64-debug
ctest --preset windows-x64-debug -R "^(native_workspace_store_test|room_list_model_test|app_controller_test|qml_interaction_test|qml_close_regression_test)$" --output-on-failure
~~~

Expected: 所有目标通过。

- [ ] **Step 2: 构建 Release 并运行相同回归集**

Run:

~~~powershell
cmake --build --preset windows-x64-release
ctest --preset windows-x64-release -R "^(native_workspace_store_test|room_list_model_test|app_controller_test|qml_interaction_test|qml_close_regression_test)$" --output-on-failure
~~~

Expected: Release 构建成功且目标测试通过。

- [ ] **Step 3: 进行视觉核查**

启动或复用本地视觉页后，用 Playwright 截图确认：房间列表拖拽有唯一插入线、收藏页拖拽指示正确、220px 宽卡片顶部文字没有进入操作区、分组管理窗口的成员库和 n / 9 容量可见。若本地视觉页不可用，记录原因，并以 qml_interaction_test 的对象与几何断言作为原生界面验证依据。

- [ ] **Step 4: 检查工作区并提交验证后的修复**

Run:

~~~powershell
git diff --check
git status --short
~~~

Expected: 无空白错误；除本计划的受控文件外不纳入 $xml 或已有未跟踪文档。

若前面任务后仅有已提交更改，此步骤不创建空提交；若验证暴露并修复了问题，仅提交相关文件：

~~~powershell
git add <verified-files>
git commit -m "fix: verify room library interactions"
~~~
