# 收藏主播状态通知实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (recommended) or superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让所有收藏房间在未播放时也进行轻量状态检测，并对开播、下播和标题变化发送可识别主播身份的 Windows 通知。

**Architecture:** 新增独立 `FavoriteMonitor`，复用 `RoomStatusScheduler` 和现有 `StreamgetProcessClient::search()`，只保存收藏房间的元数据/直播状态，不创建 `RoomSession`、播放器或弹幕会话。`AppController` 同时协调播放快照和收藏监控事件，`NotificationPolicy` 负责统一基线、去重、限流与事件文案，`WindowsNotificationService` 增加标题变化偏好。

**Tech Stack:** C++20、Qt 6 QObject/QTimer/QTest、现有 JSON 行协议、Windows Shell notification sink、CMake/Ninja。

---

### Task 1: 扩展通知事件模型与策略

**Files:**
- Modify: `native/src/workspace/notification_policy.h`
- Modify: `native/src/workspace/notification_policy.cpp`
- Modify: `native/tests/notification_policy_test.cpp`

- [ ] **Step 1: 写失败测试**

在测试夹具中增加收藏快照辅助函数，并新增断言：收藏房间首次快照不通知；`Offline -> Online` 事件包含主播名；在线标题变化产生新事件，正文同时包含原标题和新标题；普通非收藏房间标题变化不产生事件。

```cpp
void titleChangesForFavoriteRoom();
```

断言示例：

```cpp
auto oldRoom = onlineRoom(QStringLiteral("63136"));
oldRoom.favorite = true;
oldRoom.metadata.title = QStringLiteral("旧标题");
auto newRoom = oldRoom;
newRoom.metadata.title = QStringLiteral("新标题");
NotificationPolicy policy([] { return qint64{1'000}; });
QCOMPARE(policy.update({oldRoom}).size(), 0);
const auto events = policy.update({newRoom});
QCOMPARE(events.size(), 1);
QCOMPARE(events.front().type, NotificationEventType::FavoriteTitleChanged);
QVERIFY(events.front().body.contains(QStringLiteral("旧标题")));
QVERIFY(events.front().body.contains(QStringLiteral("新标题")));
```

- [ ] **Step 2: 运行测试确认失败**

```powershell
ctest --test-dir native/out/build/windows-x64-release -R '^notification_policy_test$' --output-on-failure
```

预期：因 `FavoriteTitleChanged` 未定义或策略未生成事件而失败。

- [ ] **Step 3: 实现最小改动**

在 `NotificationEventType` 增加 `FavoriteTitleChanged`；`NotificationPolicy::update()` 仅在 `snapshot.favorite` 且前后标题非空、发生变化时生成该事件；事件 key 使用独立类型；`makeEvent()` 生成“主播名 的直播间标题已更新\n原标题：旧标题\n新标题：新标题”。开播/下播继续使用现有事件类型，确保已有偏好兼容。

- [ ] **Step 4: 运行测试确认通过**

```powershell
ctest --test-dir native/out/build/windows-x64-release -R '^notification_policy_test$' --output-on-failure
```

- [ ] **Step 5: 提交**

```powershell
git add native/src/workspace/notification_policy.h native/src/workspace/notification_policy.cpp native/tests/notification_policy_test.cpp
git commit -m "feat: add favorite title change notification policy"
```

### Task 2: 增加收藏监控器

**Files:**
- Create: `native/src/workspace/favorite_monitor.h`
- Create: `native/src/workspace/favorite_monitor.cpp`
- Create: `native/tests/favorite_monitor_test.cpp`
- Modify: `native/CMakeLists.txt`

- [ ] **Step 1: 写失败测试**

使用可注入的 `StartSearch`/`CancelRequest` 回调构造监控器，覆盖：同步收藏集合创建请求；首次成功响应只建立基线；后续在线、离线、标题变化分别生成事件；取消收藏取消请求并清理基线；失败响应保留旧基线。

```cpp
void emitsOnlineAndTitleChangesAfterBaseline();
void removesUnfavoritedRoomAndCancelsRequest();
```

- [ ] **Step 2: 运行测试确认失败**

```powershell
ctest --test-dir native/out/build/windows-x64-release -R '^favorite_monitor_test$' --output-on-failure
```

预期：目标不存在或测试无法链接。

- [ ] **Step 3: 实现最小监控器**

定义 `FavoriteRoomState { roomId, RoomMetadata metadata, RoomLiveStatus liveStatus }`、`FavoriteMonitorEvent` 和 `FavoriteMonitor::synchronize(const QVector<FavoriteRoomState>&)`、`onSearchResponse(const ServiceResponse&)`、`onRequestFailed(quint64)`、`stop()`。内部使用 `RoomStatusScheduler` 的相同时间参数，查询回调只调用 `StreamgetProcessClient::search(roomId)`；成功响应要求单条且 roomId 匹配。新房间首次成功只缓存基线，后续生成在线、离线、标题变化事件并发出 `eventsReady(QVector<NotificationEvent>)` 和 `roomUpdated(QString, RoomMetadata, RoomLiveStatus)`。

- [ ] **Step 4: 接入 CMake 并运行测试**

将源文件加入 `douyu_monitor_native` 与 `favorite_monitor_test`，注册 `add_test(NAME favorite_monitor_test ...)`，运行：

```powershell
cmake --build --preset windows-x64-release --target favorite_monitor_test -- -j1
ctest --test-dir native/out/build/windows-x64-release -R '^favorite_monitor_test$' --output-on-failure
```

- [ ] **Step 5: 提交**

```powershell
git add native/src/workspace/favorite_monitor.* native/tests/favorite_monitor_test.cpp native/CMakeLists.txt
git commit -m "feat: monitor favorite rooms without playback sessions"
```

### Task 3: 接入 AppController 与服务响应路由

**Files:**
- Modify: `native/src/ui/app_controller.h`
- Modify: `native/src/ui/app_controller.cpp`
- Modify: `native/src/workspace/native_workspace_types.h` only if a cached live-status field is required by the existing library model
- Modify: `native/tests/app_controller_test.cpp`

- [ ] **Step 1: 写失败测试**

增加测试：收藏房间未出现在 `activeRoomIds` 时，模拟收藏监控服务响应，验证收到开播事件、`libraryRooms()` 更新主播名/标题/online 状态；取消收藏后再次响应不更新也不通知。

- [ ] **Step 2: 运行测试确认失败**

```powershell
ctest --test-dir native/out/build/windows-x64-release -R '^app_controller_test$' --output-on-failure
```

- [ ] **Step 3: 实现接入**

在 `AppController` 增加 `std::unique_ptr<FavoriteMonitor> favoriteMonitor_`。构造时连接 `StreamgetProcessClient::responseReceived`/`requestFailed` 到收藏监控器，同时保留协调器连接；工作区恢复、`setFavorite()` 和每次快照变化后调用 `synchronizeFavoriteMonitor()`，传入所有收藏记录及其元数据。收藏监控事件送入 `notificationService_->deliver()`，但启动恢复阶段只建立基线。`roomUpdated` 更新对应 `NativeRoomRecord::metadata` 并触发 `libraryRoomsChanged()`；不要把收藏监控快照加入播放计数。

- [ ] **Step 4: 运行测试确认通过**

```powershell
cmake --build --preset windows-x64-release --target app_controller_test douyu_monitor_native -- -j1
ctest --test-dir native/out/build/windows-x64-release -R '^app_controller_test$' --output-on-failure
```

- [ ] **Step 5: 提交**

```powershell
git add native/src/ui/app_controller.h native/src/ui/app_controller.cpp native/tests/app_controller_test.cpp
git commit -m "feat: wire favorite monitoring into app controller"
```

### Task 4: Windows 通知偏好与设置界面

**Files:**
- Modify: `native/src/app/windows_notification_service.h`
- Modify: `native/src/app/windows_notification_service.cpp`
- Modify: `native/src/ui/app_controller.h`
- Modify: `native/src/ui/app_controller.cpp`
- Modify: `native/app/qml/dialogs/NotificationSettingsDialog.qml`
- Modify: `native/tests/windows_notification_service_test.cpp`
- Modify: `native/tests/qml_interaction_test.cpp`

- [ ] **Step 1: 写失败测试**

增加 `favoriteTitleChanged` 偏好默认开启、可持久化；关闭该偏好后 `WindowsNotificationService::deliver()` 不投递 `FavoriteTitleChanged`。QML 测试确认设置面板存在“收藏标题变化”开关。

- [ ] **Step 2: 运行测试确认失败**

```powershell
ctest --test-dir native/out/build/windows-x64-release -R "^(windows_notification_service_test|qml_interaction_test)$" --output-on-failure
```

- [ ] **Step 3: 实现最小改动**

在 `NotificationPreferences` 增加 `bool favoriteTitleChanged = true`，读写 `notifications/favoriteTitleChanged`；`key()` 和 `isEnabled()` 支持新事件；`AppController::notificationPreferences()`、`setNotificationPreferences()` 增加该字段并保持旧调用的默认值兼容；QML 新增复用现有复选框样式的“收藏标题变化”选项。

- [ ] **Step 4: 运行测试确认通过**

```powershell
cmake --build --preset windows-x64-release --target windows_notification_service_test qml_interaction_test -- -j1
ctest --test-dir native/out/build/windows-x64-release -R "^(windows_notification_service_test|qml_interaction_test)$" --output-on-failure
```

- [ ] **Step 5: 提交**

```powershell
git add native/src/app/windows_notification_service.* native/src/ui/app_controller.* native/app/qml/dialogs/NotificationSettingsDialog.qml native/tests/windows_notification_service_test.cpp native/tests/qml_interaction_test.cpp
git commit -m "feat: add favorite title notification preference"
```

### Task 5: 全量构建与回归验证

**Files:**
- Verify only; no source changes expected.

- [ ] **Step 1: 构建相关目标**

```powershell
$vs='C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat'
cmd /c "call `"$vs`" -arch=x64 && cmake --build --preset windows-x64-release --target douyu_monitor_native favorite_monitor_test notification_policy_test windows_notification_service_test app_controller_test qml_engine_smoke_test qml_visual_smoke_test qml_interaction_test -- -j1"
```

- [ ] **Step 2: 运行完整相关测试**

```powershell
ctest --test-dir native/out/build/windows-x64-release -R "^(favorite_monitor_test|notification_policy_test|windows_notification_service_test|app_controller_test|qml_engine_smoke_test|qml_visual_smoke_test|qml_interaction_test|douyu_monitor_native_self_test)$" --output-on-failure
```

预期：所有列出的测试通过。

- [ ] **Step 3: 检查差异和运行时资源**

```powershell
git diff --check
Get-Item native/out/build/windows-x64-release/douyu_monitor_native.exe
```

- [ ] **Step 4: 提交最终实现**

```powershell
git status --short
git log -5 --oneline
```

确认仅包含本功能相关提交后，再按项目发布流程决定是否构建安装包。
