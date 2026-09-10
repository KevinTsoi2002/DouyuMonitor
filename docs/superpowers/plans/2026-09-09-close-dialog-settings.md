# 关闭弹窗与设置页 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复关闭弹窗布局和取消交互，并增加持久化关闭行为设置页入口。

**Architecture:** 保持 `AppController` 为关闭策略唯一状态源。QML 仅负责布局和视图切换；设置页嵌入主内容区，左侧房间列表保持可见。关闭弹窗使用显式 indicator/text 行布局，避免 Qt Controls 默认 indicator 与自定义 contentItem 重叠。

**Tech Stack:** Qt Quick/QML, Qt Quick Controls 2, C++20, QSettings, QtTest.

---

### Task 1: 关闭弹窗回归测试

**Files:**
- Modify: `native/tests/qml_close_regression_test.cpp`
- Modify: `native/tests/app_controller_test.cpp`

- [ ] 增加测试：加载关闭弹窗后找到取消按钮和记忆复选框。
- [ ] 增加测试：点击取消后弹窗不可见，控制器未进入退出或后台状态。
- [ ] 增加测试：复选框 indicator 与文本 item 的几何区域不相交。
- [ ] 运行目标测试，确认新增断言在旧实现上失败。

### Task 2: 修复关闭弹窗布局与取消按钮

**Files:**
- Modify: `native/app/qml/dialogs/CloseBehaviorDialog.qml`

- [ ] 用 `RowLayout` 替换 `CheckBox` 的自定义 `contentItem` 叠加方式，显式放置固定尺寸 indicator 和文本。
- [ ] 保留现有“进入后台”和“完全退出”逻辑。
- [ ] 增加次要“取消”按钮，点击只调用 `root.close()`。
- [ ] 运行 QML 关闭回归测试，确认通过。

### Task 3: 设置入口与设置页骨架

**Files:**
- Modify: `native/app/qml/components/RoomSidebar.qml`
- Modify: `native/app/qml/Main.qml`
- Create: `native/app/qml/pages/SettingsPage.qml`
- Modify: `native/CMakeLists.txt`

- [ ] 在房间列表底部增加设置按钮和 `settingsRequested` 信号。
- [ ] 在 `Main.qml` 增加 `currentView` 状态，通过设置入口切换主内容区。
- [ ] 新建 `SettingsPage.qml`，实现返回按钮、关闭行为单选/复选和检查更新占位项。
- [ ] 视图切换时不销毁播放器网格、会话或弹幕对象。
- [ ] 将新 QML 文件加入所有相关 QML 资源目标。

### Task 4: 设置持久化与测试

**Files:**
- Modify: `native/app/qml/pages/SettingsPage.qml`
- Modify: `native/src/ui/app_controller.cpp` only if an adapter is required
- Modify: `native/tests/app_controller_test.cpp`

- [ ] 绑定设置页到 `closeBehavior` 属性，并调用 `setCloseBehavior`/`clearCloseBehavior`。
- [ ] 验证每次询问和未记住选项会移除 `window/closeBehavior`。
- [ ] 点击检查更新只更新本地占位状态文本。
- [ ] 增加 QSettings round-trip 测试及占位行为测试。

### Task 5: 构建与验收

**Files:**
- Verify: `native/CMakeLists.txt`, generated QML resources, Release build output

- [ ] 使用 MSVC 环境构建 `douyu_monitor_native`、`app_controller_test`、`qml_close_regression_test`。
- [ ] 运行相关 QtTest，确认 0 failed。
- [ ] 运行 QML 加载/视觉 smoke test，确认设置页和关闭弹窗可实例化。
- [ ] 使用 Playwright/浏览器截图或 Qt Quick offscreen 检查 checkbox 间距、设置入口和返回路径。
- [ ] 运行 `git diff --check`，确认没有空白错误。
