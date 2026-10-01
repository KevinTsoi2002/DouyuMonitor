# DouyuMonitor Agent Guide

本文件定义在本仓库中工作的 AI、自动化和代码代理的默认规则。它适用于整个仓库；如果未来某个子目录新增更具体的 `AGENTS.md`，以距离目标文件更近的规则为准。

本文件不是项目功能文档，也不替代源码、测试、构建脚本和现有文档。发生冲突时，以当前分支的实际代码、配置和测试行为为准。

## 1. 项目边界

- 当前 `main` 只维护原生 Windows x64 桌面客户端。
- UI 使用 Qt Quick/QML，应用逻辑使用 C++20。
- 视频播放使用 libmpv 和 OpenGL，发布运行时加载 `mpv.dll`。
- 斗鱼资料和播放源解析由独立的 `streamget_service.exe` 子进程完成。
- 弹幕使用 Qt WebSockets 原生客户端。
- Windows 安装包使用 Inno Setup 6。
- 不使用 Electron、Chromium、Node.js、Qt WebEngine 或网页播放器。
- 旧 Electron/TypeScript 实现保存在 `legacy-framework` 分支；不要从该分支恢复代码到 `main`，除非用户明确要求。
- 版本号以 `native/CMakeLists.txt` 中的 `project(DouyuMonitorNative VERSION ...)` 为准，不要只根据 README 或安装包文件名判断。

主要入口和边界：

- 程序入口：`native/app/main.cpp`。
- QML 根界面：`native/app/qml/Main.qml`，通过 `DouyuMonitor/Main` 模块加载。
- QML 与领域层的公共适配面：`native/src/ui/app_controller.*`。
- 多房生命周期和布局协调：`native/src/workspace/multi_room_coordinator.*`。
- 单房状态和播放生命周期：`native/src/workspace/room_session.*`。
- StreamGet 子进程协议和调用：`native/src/service/streamget_process_client.*`、`native/src/service/stream_service_protocol.*`。
- StreamGet Python 服务：`native/service/streamget_service.py`、`native/service/douyu_backend.py`、`native/service/protocol.py`。
- libmpv Qt Quick 渲染项：`native/src/ui/mpv_quick_item.*`。
- 弹幕协议、连接和治理：`native/src/danmaku/`。
- 工作区持久化：`native/src/workspace/native_workspace_store.*`。

## 2. 事实优先级

分析和修改前按以下顺序确认事实：

1. 当前分支的源码、配置、构建脚本和测试。
2. `native/CMakeLists.txt`、`native/CMakePresets.json`、`native/vcpkg*.json` 和实际 PowerShell 脚本。
3. `README.md`、`native/README.md`、`docs/文件职责索引.md`。
4. `docs/superpowers/specs/`、`docs/superpowers/plans/` 和日志。

规则：

- 不要根据文件名、注释、旧文档或命名习惯直接推断行为。
- 代码与注释、README、设计文档冲突时，记录冲突，并以当前代码行为为主要依据。
- 无法从代码、配置或测试确认的结论必须标记为“未确认”“推测”或“需要进一步验证”。
- 修改行为时同步检查对应测试和文档；不要为了文档完整而编造实现细节。
- 不要把 `out/`、SDK、虚拟环境、日志和根目录诊断文件当作源码事实来源。

## 3. 架构和修改边界

### UI 与领域层

- QML 负责界面呈现、交互状态和动画，不应复制领域状态或直接实现业务规则。
- QML 通过 `AppController` 的命令和只读属性访问领域层。
- 不要绕过 `AppController`、`MultiRoomCoordinator` 或 `RoomSession` 的公开边界直接修改内部状态。
- 修改房间、播放器绑定、质量、音频、弹幕或持久化状态时，保持现有信号和状态同步关系。

### 播放链

当前主要调用链为：

`RoomSession` → `RemotePlaybackController` → `StreamgetProcessClient` → `streamget_service.exe` → `MediaSource` → `MpvQuickItem`。

- 播放 URL、查询参数、Cookie、Token、签名和原始服务诊断只允许在当前会话内存中使用。
- 不要新增日志、配置、测试快照或诊断文件来保存上述敏感数据。
- libmpv 的初始化和释放必须遵守 Qt Quick 场景图生命周期；不要持有已销毁 QML 对象的裸指针。

### StreamGet 服务

- Qt 与 Python 服务之间使用私有 JSONL stdin/stdout 协议。
- 请求必须有正整数 `requestId`，并按 `ping`、`resolve`、`search`、`cancel`、`shutdown` 等操作处理。
- 错误响应使用固定错误码和 `retryable` 字段，不应泄露 Python 异常、堆栈、URL 或其他诊断信息。
- 修改协议时必须同时修改 C++ 协议实现、Python 协议实现、双方测试和文档。
- 保持服务的 UTF-8 stdin/stdout 行为和 Windows 子进程兼容性。

### 弹幕

- 弹幕连接、心跳、重连、协议编解码和治理策略属于 `native/src/danmaku/`。
- 不要持久化原始弹幕帧、认证数据或连接敏感信息。
- 修改弹幕会话数量、队列、限流或重连规则时，检查 UI 资格同步、治理统计和关闭生命周期。

### 持久化

- 应用使用 `QSettings` 的 INI 格式，工作区键为 `DouyuMonitor/nativeWorkspaceV1`。
- 当前持久化内容需要保持向后兼容；新增字段或提升 schema 时，必须处理旧版本数据、默认值和规范化。
- 不要向设置中写入播放地址、Cookie、Token、签名或原始弹幕数据。
- 不要随意改变已有键名、字段名或 schema 语义；这会影响用户工作区恢复和迁移测试。

### 应用生命周期

- 关闭窗口或最小化可能进入 Windows 托盘后台托管，具体行为以 `AppController`、托盘服务和关闭回归测试为准。
- 后台托管期间音频、状态检测和通知可能继续运行，视频渲染和弹幕展示可能暂停。
- 修改退出、托盘、窗口最小化、全屏或后台恢复逻辑时，必须检查资源释放、服务停止和播放器回调。

## 4. 构建、运行和测试

所有原生命令默认在 `native` 目录中执行，并使用 Visual Studio 2022 x64 Developer PowerShell。

环境变量示例：

```powershell
$env:QT_ROOT = 'D:\Qt\6.8.3\msvc2022_64'
$env:MPV_ROOT = (Resolve-Path .\sdk\mpv).Path
Set-Location .\native
```

Release 构建和测试：

```powershell
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release --target douyu_monitor_native
ctest --preset windows-x64-release
```

Debug 构建和测试：

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64-debug
ctest --preset windows-x64-debug
```

启动 Release 程序：

```powershell
.\out\build\windows-x64-release\douyu_monitor_native.exe
```

程序自测：

```powershell
.\out\build\windows-x64-release\douyu_monitor_native.exe --self-test
```

StreamGet 服务准备和打包：

```powershell
.\scripts\bootstrap-streamget-service.ps1
.\scripts\build-streamget-service.ps1
```

安装包构建：

```powershell
.\scripts\build-windows-installer.ps1
```

安装器脚本回归检查：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\test-installer-script.ps1
```

测试和验证规则：

- 修改 C++ 领域逻辑时，至少构建相关目标并运行对应 CTest。
- 修改 QML 时，运行 `qml_engine_smoke_test`、`qml_interaction_test`，涉及视觉布局时运行 `qml_visual_smoke_test`。
- 修改关闭、托盘或播放器生命周期时，运行 `qml_close_regression_test` 和相关生命周期测试。
- 修改 StreamGet 协议或 Python 服务时，运行 C++ 协议测试和 Python 服务测试。
- 修改持久化时，运行 `native_workspace_store_test` 和 `app_controller_test`。
- 修改安装或发布载荷时，运行安装器脚本回归检查，并在可行时运行 Release 自测和载荷校验。
- 不要用“构建成功”代替测试；不要把某个局部测试通过表述为全量验证通过。

已知测试环境差异：

- `native/docs/visual-validation.md` 记录了 `qml_close_regression_test` 在 offscreen 后端下无法创建多路渲染上下文的情况。
- 该测试应优先使用 Windows 平台后端执行；不要为了消除环境差异而删除渲染或关闭断言。

## 5. 隐私、安全和仓库卫生

- 不记录、提交或输出密钥、Token、Cookie、签名、证书、私钥、播放地址、查询参数或原始弹幕帧。
- 不把 `.env`、`.pfx`、`.p12`、`.pem`、`.key`、`.cer`、`.crt` 或本地私有配置加入版本库。
- 不提交 `native/sdk/`、`native/out/`、`native/.venv/`、`native/vcpkg_installed/`、日志、构建产物和诊断文件。
- 不覆盖、回退或删除用户已有的未提交修改和诊断文件。
- 不使用破坏性 Git 命令清理工作区；除非用户明确要求，不执行 `git reset --hard`、`git checkout --`、强制删除或强制推送。
- 不把远程接口失败、异常堆栈或服务原始诊断直接暴露给用户界面。
- 修改外部请求、解析器或协议边界时，保持现有错误码、超时、取消和重试语义，除非用户明确要求改变行为。

## 6. 已知约束和高风险区域

以下内容是当前代码中需要特别检查的事实，不应在没有产品决策和测试的情况下自行“统一”：

- 房间容量存在多处不同约束：`MultiRoomCoordinator::kMaxRooms` 为 10；普通添加和分组业务路径使用 9；`primary-two` 布局可能达到 10；`NativeWorkspaceStore::kMaxGroupRooms` 为 10；弹幕会话管理器有独立的 9 路上限。
- README、UI 文案和实际代码对 9 路与 10 路的表述不完全一致。修改容量前先确认目标布局、弹幕开关、持久化规范和测试预期。
- `native/app/qml/dialogs/GroupManagerDialog.qml` 存在于源码树，但当前没有出现在 `native/CMakeLists.txt` 的 QML 模块文件清单中。不要默认该对话框已在当前运行时注册；涉及分组 UI 时先验证加载路径。
- `native/scripts/bootstrap-dependencies.ps1` 依赖 `native/dependencies.lock.json`。当前仓库该文件被忽略且未随源码提供；使用该脚本前先确认锁文件已恢复或改用现有的 `QT_ROOT`、`MPV_ROOT`。
- 当前未发现正式的 CI/CD 配置文件。不要假设远端会自动构建、测试或生成安装包；本地验证是默认要求。
- 真实斗鱼环境的长时间多路播放、弹幕稳定性、CPU/GPU 和内存验收属于目标环境验证，不等同于本地单元测试或 QML 烟测。

## 7. 代理工作流

处理任务时按以下顺序执行：

1. 先读取与任务相关的最小源码、配置、测试和文档。
2. 明确入口、数据流、状态变化和失败路径。
3. 优先做最小、可逆、与现有模式一致的修改。
4. 修改行为时同步补充或更新测试。
5. 运行与改动风险匹配的构建、测试和运行验证。
6. 检查 Git diff，确认没有覆盖用户改动、没有加入敏感数据或生成物。
7. 汇报变更内容、验证命令、实际结果、未验证部分和残余风险。

除非用户明确要求，否则不要创建提交、标签、Release、分支推送或远程变更。创建分支时默认使用 `codex/` 前缀。

## 8. 完成标准

只有在以下条件满足时才能声称任务完成：

- 需求对应的代码或文档已经实际修改。
- 相关构建和测试已经运行，并能引用实际输出。
- 没有把未验证的真实斗鱼环境、安装包或性能验收描述为已通过。
- 已知限制、冲突和需要进一步验证的事项已经明确报告。
