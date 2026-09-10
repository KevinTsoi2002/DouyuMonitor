# 直播播放器进程与窗口生命周期 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 消除 StreamGet 残留进程，并实现可持久化的完全退出/后台托管关闭选择及独立的最小化降耗行为。

**Architecture:** StreamGet 改为 PyInstaller `--onedir`，Qt 直接管理唯一服务进程；Windows 下为服务进程绑定 Job Object，在超时或析构时通过 `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` 清理全部子进程。AppController 保留现有后台托盘路径，新增关闭偏好与最小化状态，QML 负责选择对话框，渲染/解码通过现有 coordinator/session/mpv 接口成对挂起和恢复。

**Tech Stack:** Qt Quick/QML, C++20, QProcess, Windows Job Object API, libmpv, PyInstaller, QtTest.

---

### Task 1: StreamGet 单进程打包与安装复制

**Files:**
- Modify: `native/scripts/build-streamget-service.ps1`
- Modify: `native/cmake/copy_streamget_service.cmake`
- Modify: `native/CMakeLists.txt`
- Test: `native/tests/streamget_process_client_test.cpp`

- [ ] 将 PyInstaller 从 `--onefile` 改为 `--onedir`，输出 `out/service/streamget_service/streamget_service.exe`。
- [ ] 让 CMake 复制整个服务目录，并保留开发环境下旧 exe 路径兼容。
- [ ] 增加测试辅助函数，验证关闭后服务路径下不存在残留服务进程（Windows 使用 `tasklist`/PID 记录，非 Windows 跳过）。
- [ ] 先运行测试确认新增断言在旧实现上失败。

### Task 2: Windows Job Object 生命周期

**Files:**
- Modify: `native/src/service/streamget_process_client.h`
- Modify: `native/src/service/streamget_process_client.cpp`
- Modify: `native/tests/streamget_process_client_test.cpp`

- [ ] 增加 Windows-only `HANDLE job_` 和 `attachProcessToJob()`/`terminateJob()` 私有方法。
- [ ] 构造 Job Object，设置 `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`，进程启动后将 `process_->processId()` 对应句柄加入 Job。
- [ ] `shutdown()` 等待超时后先终止 Job，再终止 QProcess；析构释放 Job handle。
- [ ] 非 Windows 保持现有行为。
- [ ] 运行关闭、重启、异常退出测试，确认无服务残留。

### Task 3: 关闭偏好与关闭选择对话框

**Files:**
- Modify: `native/src/ui/app_controller.h`
- Modify: `native/src/ui/app_controller.cpp`
- Modify: `native/app/qml/Main.qml`
- Create: `native/app/qml/dialogs/CloseBehaviorDialog.qml`
- Modify: `native/tests/app_controller_test.cpp`

- [ ] 增加 `closeBehavior` 属性/枚举：`ask`、`quit`、`background`，使用 QSettings 持久化。
- [ ] 暴露 `requestClose()`、`setCloseBehavior()`、`clearCloseBehavior()`，保留 `requestQuit()` 与 `closeToTray()` 的既有语义。
- [ ] QML 关闭按钮调用 `requestClose()`；`ask` 时弹出“完全退出/进入后台/记住我的选择”，否则直接执行记忆动作。
- [ ] 完全退出路径调用 `shutdown()`，等待 libmpv、hwdec、服务 Job 清理后允许窗口关闭。
- [ ] 增加 QSettings round-trip 与三种动作测试。

### Task 4: 最小化独立降耗

**Files:**
- Modify: `native/src/ui/app_controller.h`
- Modify: `native/src/ui/app_controller.cpp`
- Modify: `native/src/workspace/multi_room_coordinator.h`
- Modify: `native/src/workspace/multi_room_coordinator.cpp`
- Modify: `native/src/workspace/room_session.h`
- Modify: `native/src/workspace/room_session.cpp`
- Modify: `native/src/ui/mpv_quick_item.h`
- Modify: `native/src/ui/mpv_quick_item.cpp`
- Modify: `native/app/qml/Main.qml`
- Modify: `native/tests/app_controller_test.cpp`

- [ ] 增加 `windowMinimized` 状态；最小化只隐藏窗口到任务栏，不设置 `backgroundHosted`。
- [ ] 最小化时暂停 QML presentation，降低视频渲染/刷新频率；保持音频与状态检测。
- [ ] 恢复时按相反顺序恢复 QML presentation 与 mpv rendering。
- [ ] 后台托管和最小化状态互斥，恢复窗口时清理最小化状态。
- [ ] 增加测试：最小化不进入托盘，后台托管仍暂停 presentation，恢复对称。

### Task 5: 构建与验收

**Files:**
- Verify: `native/CMakePresets.json`, `native/scripts/build-streamget-service.ps1`, installer staging files

- [ ] 构建 StreamGet onedir 与 Release native executable。
- [ ] 运行 QtTest：服务客户端、AppController、托盘、QML 关闭回归。
- [ ] 启动 Release 检查进程树只有一个正式 `streamget_service.exe` 工作进程。
- [ ] 验收完全退出、进入后台、记住选择、最小化/恢复及重复关闭行为。
- [ ] 记录当前环境无法执行的真实斗鱼长时间播放项，不将其误报为已验证。
