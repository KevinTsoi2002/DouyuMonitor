# Qt Native Bugfixes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复收藏加入、声音总控定位和工作区预设重复应用三个回归问题。

**Architecture:** 收藏记录作为持久化库的事实来源，加入活动区时把收藏、音量和元数据作为初始会话状态传入；声音总控弹层以按钮为锚点定位；工作区预设应用保留可复用的现有会话与播放器绑定，仅增删差异房间。

**Tech Stack:** Qt 6, QML, C++, Qt Test, CMake/Ninja.

---

### Task 1: 收藏状态在重新加入时保持

**Files:**
- Modify: `native/src/workspace/multi_room_coordinator.h`
- Modify: `native/src/workspace/multi_room_coordinator.cpp`
- Modify: `native/src/ui/app_controller.cpp`
- Test: `native/tests/app_controller_test.cpp`

- [x] 为“收藏后移除再从库中加入仍是收藏”写失败测试。
- [x] 让 `addRoomDetailed` 接收并应用已有记录的收藏与音量初值，再发布快照。
- [x] 运行单测并确认通过。

### Task 2: 声音总控弹层锚定按钮

**Files:**
- Modify: `native/app/qml/components/AppHeader.qml`
- Test: `native/tests/qml_interaction_test.cpp`

- [x] 增加弹层相对按钮位置的失败断言。
- [x] 使用同一父级 `actionButtons.x + soundMasterButton.x` 计算锚点，避免 Release/离屏环境的跨层坐标异常；弹层出现在按钮正下方并保持屏幕边界。
- [x] 增加再次点击按钮关闭弹层的回归断言。
- [x] 运行 Debug 与 Release QML 交互测试。

### Task 3: 工作区预设批量、可重复应用

**Files:**
- Modify: `native/src/workspace/multi_room_coordinator.cpp`
- Modify: `native/src/ui/app_controller.cpp`
- Test: `native/tests/app_controller_test.cpp`

- [x] 增加多房间预设连续应用两次的失败测试。
- [x] 复用相同 roomId 的会话，安全解绑/清理被移除房间，并在批量操作期间抑制中间快照重入。
- [x] 保留预设中合法但暂不在 library 的房间号，应用时创建 fallback library 记录，避免旧预设只恢复第一路。
- [x] 增加应用五路预设后删除主房间的主画面与列表回归断言。
- [x] 运行全部 C++/QML 测试和 Release 构建。
