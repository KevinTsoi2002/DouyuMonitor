# 移除分组与拖拽 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 移除可操作的分组与拖拽排序功能，为工作区预设增加删除操作，并保留旧工作区分组数据兼容性。

**Architecture:** QML 只移除分组和拖拽表面，不删除 `AppController` 的历史分组 API 或 `NativeWorkspaceSnapshot` 的分组字段。预设删除通过 AppController 删除快照条目、刷新投影并写回存储；QML 委托使用独立按钮，避免与应用预设点击区域重叠。

**Tech Stack:** Qt 6 QML、C++20、Qt Test、CMake、Windows MSVC。

---

### Task 1: 预设删除控制器 API

**Files:**
- Modify: `native/src/ui/app_controller.h`
- Modify: `native/src/ui/app_controller.cpp`
- Test: `native/tests/app_controller_test.cpp`

- [x] **Step 1: 写入失败测试**

```cpp
void AppControllerTest::deletesWorkspacePresetAndPersistsRemoval()
{
    const QString presetId = controller.saveWorkspacePreset(QStringLiteral("可删除"));
    QVERIFY(controller.deleteWorkspacePreset(presetId).isEmpty());
    QVERIFY(controller.workspace()->presets().isEmpty());
}
```

- [x] **Step 2: 运行测试确认失败**

Run: `ctest --test-dir native/out/build/windows-x64 -R ^app_controller_test$ --output-on-failure`
Expected: 编译失败，提示 `deleteWorkspacePreset` 未定义。

- [x] **Step 3: 实现最小 API**

```cpp
Q_INVOKABLE QString deleteWorkspacePreset(const QString &presetId);

QString AppController::deleteWorkspacePreset(const QString &presetId)
{
    const auto it = std::find_if(snapshot_.presets.begin(), snapshot_.presets.end(),
                                 [&presetId](const NativeWorkspacePreset &preset) {
                                     return preset.id == presetId;
                                 });
    if (it == snapshot_.presets.end()) return commandMessage(RoomCommandResult::RoomNotFound);
    snapshot_.presets.erase(it);
    refreshPresentation();
    persistWorkspace();
    return {};
}
```

- [x] **Step 4: 运行测试确认通过**

Run: `ctest --test-dir native/out/build/windows-x64 -R ^app_controller_test$ --output-on-failure`
Expected: `app_controller_test` 通过。

### Task 2: 移除分组与拖拽 QML 表面

**Files:**
- Modify: `native/app/qml/components/RoomSidebar.qml`
- Modify: `native/app/qml/components/RoomLibraryView.qml`
- Modify: `native/app/qml/panels/WorkspacePresetsPanel.qml`
- Modify: `native/app/qml/Main.qml`
- Modify: `native/CMakeLists.txt`
- Test: `native/tests/qml_interaction_test.cpp`

- [x] **Step 1: 写入失败交互测试**

```cpp
QVERIFY(row->findChild<QObject *>(QStringLiteral("roomDragHandle")) == nullptr);
QVERIFY(row->findChild<QObject *>(QStringLiteral("roomDropArea")) == nullptr);
QVERIFY(sidebar->findChild<QObject *>(QStringLiteral("groupTabs")) == nullptr);
```

- [x] **Step 2: 运行测试确认失败**

Run: `ctest --test-dir native/out/build/windows-x64 -R ^qml_interaction_test$ --output-on-failure`
Expected: 断言失败，当前 UI 仍有拖拽与分组对象。

- [x] **Step 3: 移除用户可达入口并添加预设删除按钮**

```qml
ToolButton {
    objectName: "deleteWorkspacePresetButton"
    onClicked: root.controller.deleteWorkspacePreset(modelData.id)
}
```

删除侧栏的组标签、组管理信号和拖拽代理；删除收藏拖拽代理；删除主界面分组对话框实例及 CMake QML 资源引用。保留分组数据与 C++ API。

- [x] **Step 4: 运行 QML 交互测试确认通过**

Run: `ctest --test-dir native/out/build/windows-x64 -R ^qml_interaction_test$ --output-on-failure`
Expected: `qml_interaction_test` 通过，且上下排序按钮测试继续通过。

### Task 3: 全量验证与 Release 构建

**Files:**
- Verify: `native/out/build/windows-x64-release/douyu_monitor_native.exe`

- [x] **Step 1: 构建全部测试目标并执行测试**

Run: `ctest --test-dir native/out/build/windows-x64 --output-on-failure`
Expected: 所有测试通过。

- [x] **Step 2: 构建 Windows Release 程序**

Run: `cmake --build --preset windows-x64-release --target douyu_monitor_native douyu_monitor_uninstaller -- -j1`
Expected: 两个目标构建成功。

- [x] **Step 3: 运行 Release 自检**

Run: `douyu_monitor_native.exe --self-test`
Expected: 退出码 `0`。
