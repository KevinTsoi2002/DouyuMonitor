# Update Checker Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在设置页实现基于 GitHub Releases API 的正式版本检查，并覆盖网络错误、解析错误、超时和 QML 交互。

**Architecture:** 新增可注入 API 地址的 `UpdateChecker`，负责 Qt Network 请求、超时、JSON 和 SemVer；`AppController` 转发状态并打开 Release URL；`SettingsPage.qml` 只展示状态和交互。当前版本由 CMake 项目版本定义注入。

**Tech Stack:** C++20, Qt 6 Core/Network/Test, Qt Quick/QML, CMake, QtTest, QTcpServer。

---

### Task 1: 建立更新检查测试夹具和版本行为测试

**Files:**
- Create: `native/tests/update_checker_test.cpp`
- Modify: `native/CMakeLists.txt`

- [ ] **Step 1: Write the failing tests**
  - 覆盖 `V/v/无前缀` 标签解析、三段数字比较、非法标签拒绝。
  - 用本地 `QTcpServer` 夹具覆盖成功响应、HTTP 500、无效 JSON、超时。
  - 先引用尚不存在的 `UpdateChecker` API，确保测试表达期望行为。
- [ ] **Step 2: Run the focused test and verify it fails**
  - 配置现有 Qt 构建目录后运行 `ctest -R update_checker_test --output-on-failure`。
  - 预期因目标/类尚不存在而失败，而非测试自身语法错误。
- [ ] **Step 3: Add the test target wiring**
  - 在 CMake 中加入 `Qt6::Network` 组件和 `update_checker_test` 目标；先保留生产源缺失导致的失败。

### Task 2: 实现 UpdateChecker

**Files:**
- Create: `native/src/app/update_checker.h`
- Create: `native/src/app/update_checker.cpp`
- Modify: `native/CMakeLists.txt`

- [ ] **Step 1: Implement pure version helpers**
  - 提供标签规范化和三段数字比较，拒绝额外 prerelease/build 后缀。
- [ ] **Step 2: Implement request lifecycle**
  - 使用 `QNetworkAccessManager` GET 默认 GitHub endpoint，设置 `Accept`、`User-Agent`。
  - 设置有限超时；新请求取消旧请求；检查 HTTP 状态和网络错误。
- [ ] **Step 3: Parse official Release payload**
  - 读取 `tag_name`、`html_url`、`draft`、`prerelease`，过滤非正式 Release。
  - 发出 checking/success/error 信号，携带当前版本、最新版本和 URL。
- [ ] **Step 4: Run focused tests and make them green**
  - 运行 `ctest -R update_checker_test --output-on-failure`，确认新增测试通过。

### Task 3: 接入 AppController

**Files:**
- Modify: `native/src/ui/app_controller.h`
- Modify: `native/src/ui/app_controller.cpp`
- Modify: `native/CMakeLists.txt`

- [ ] **Step 1: Add QML-facing properties and invokables**
  - 添加 update state/message/current/latest/release URL 属性及 `checkForUpdates()`、`openLatestRelease()`。
- [ ] **Step 2: Wire UpdateChecker signals**
  - 构造时注入 `QCoreApplication::applicationVersion()` 或 CMake 定义的版本字符串。
  - 将 checker 结果转成稳定的中文状态消息并发出单一状态信号。
- [ ] **Step 3: Add controller tests first and run them**
  - 在 `app_controller_test.cpp` 增加状态转发测试；先确认失败，再补齐接线。
  - 运行 `ctest -R app_controller_test --output-on-failure`。

### Task 4: 更新设置页交互

**Files:**
- Modify: `native/app/qml/pages/SettingsPage.qml`
- Modify: `native/tests/qml_interaction_test.cpp` 或新增专用 QML 测试

- [ ] **Step 1: Replace placeholder click handler**
  - 点击按钮调用 `controller.checkForUpdates()`。
  - 绑定 checking 状态、结果消息、最新版本和 Release 打开按钮。
- [ ] **Step 2: Keep layout stable and accessible**
  - 请求期间禁用检查按钮；状态文本不覆盖现有设置内容；打开链接按钮仅在有合法 URL 时显示。
- [ ] **Step 3: Run QML interaction test**
  - 使用已有 QML 测试夹具验证点击、checking 状态和成功/失败消息显示。

### Task 5: 全量验证

**Files:**
- No additional files unless verification reveals a defect.

- [ ] **Step 1: Configure/build the native project**
  - 复用 `native/out/build/windows-x64-release` 的 Qt/MSVC 配置，必要时在 Visual Studio Developer PowerShell 中运行 CMake。
- [ ] **Step 2: Run focused and regression tests**
  - `ctest -R "update_checker_test|app_controller_test|qml_interaction_test" --output-on-failure`
  - 再运行完整 CTest 集合。
- [ ] **Step 3: Run source and diff checks**
  - `git diff --check`
  - 检查未跟踪诊断文件未被删除或修改。
- [ ] **Step 4: Verify UI runtime**
  - 用 Playwright/现有 QML smoke 流程打开设置页，点击检查更新并确认状态变化；记录构建环境限制（如有）。
