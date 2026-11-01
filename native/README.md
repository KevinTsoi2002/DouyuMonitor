# 原生 Qt 客户端

`native` 是 DouyuMonitor 当前唯一维护的桌面客户端：Qt Quick/QML 负责界面，C++20 负责应用逻辑，libmpv 负责播放，独立 `streamget_service.exe` 负责斗鱼资料与播放变体解析。

## CSTG狼团S1导航页

左侧导航提供 CSTG 主播入口。页面按内置公会主播目录顺序展示，支持全部、男主播、女主播、直播状态与搜索筛选。筛选保留现有行对象，减少切换时的创建与销毁开销。

导航页展示主播头像、开播状态、房间号及加入状态，详情通过显式操作打开。已确认房间号可直接加入当前工作区；无法自动确认的成员显示“待确认”，可在成员行手工输入房间号。快捷加入受当前构建的布局容量限制，并会显示控制器返回的重复房间或容量错误。队伍和活动角色仍在设置中管理，但不再决定导航列表的分组。

开播状态会写入工作区缓存，并记录检查时间。15 分钟内的重复打开导航页直接复用缓存，不再为全部主播重新排队发送状态请求；导航页可见期间仍每分钟刷新状态。空工作区且导航页隐藏时延迟启动名单检查。状态请求失败时显示未知状态，避免保留过时的在线信息。

## 目录职责

| 目录 | 职责 |
| --- | --- |
| `app` | Qt 入口与 QML 界面资源。 |
| `src/ui` | 控制器、QML 模型与 libmpv Quick Item。 |
| `src/workspace` | 多房协调、持久化、刷新、质量和通知规则。 |
| `src/media` | 播放源与播放控制。 |
| `src/service` | StreamGet 子进程协议与客户端。 |
| `src/danmaku` | 斗鱼弹幕协议、连接、治理和会话。 |
| `service` | StreamGet Python 服务和 Python 单元测试。 |
| `tests` | C++、QML 和运行时回归测试。 |
| `cmake` | Qt 部署、运行时依赖和发布载荷校验脚本。 |
| `scripts` | 依赖准备、服务构建和安装包生成脚本。 |

完整逐文件说明见根目录的 [文件职责索引](../docs/文件职责索引.md)。

## 前置条件

- Windows x64
- Visual Studio 2022（MSVC x64 工具链）
- CMake 3.24 或更高版本、Ninja
- Qt 6 MSVC 2022 x64（V1.0.0 使用 6.12.0 构建并验证）
- libmpv SDK：`include/mpv/client.h`、`libmpv.lib`、`libmpv-2.dll`

`vcpkg-configuration.json` 固定项目依赖基线。libmpv 不通过 vcpkg 分发，必须由 `MPV_ROOT` 显式指定。

## 配置和构建

在 `native` 目录内的 Visual Studio x64 Developer PowerShell 中设置环境变量：

```powershell
$env:QT_ROOT = 'D:\Qt\6.12.0\msvc2022_64'
$env:MPV_ROOT = (Resolve-Path .\sdk\mpv).Path
```

Debug：

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64-debug
ctest --preset windows-x64-debug
```

Release：

```powershell
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release
ctest --preset windows-x64-release
```

Release 可执行文件为：

```text
native/out/build/windows-x64-release/douyu_monitor_native.exe
```

部署步骤由 CMake 调用匹配的 `windeployqt` 完成。Release 目录必须包含 `platforms/qwindows.dll` 和按导入库实际名称部署的 `mpv.dll`；不要混用 Debug 与 Release 的 Qt DLL 或平台插件。SDK 源文件仍为 `libmpv-2.dll`，构建后会复制为应用加载名 `mpv.dll`。

## StreamGet 服务

准备 Python 构建环境并打包服务：

```powershell
.\scripts\bootstrap-streamget-service.ps1
.\scripts\build-streamget-service.ps1
```

StreamGet 依赖版本约束位于 `service/requirements-lock-win-py313.txt`，对应
Windows x64 / CPython 3.13.15。运行时顶层依赖在 `requirements.txt`，
PyInstaller 在 `requirements-build.txt`；准备环境时两者都使用相同的
`--constraint` 文件，打包前运行 `service/verify_dependency_lock.py` 检查
解释器、顶层版本及平台/extra 传递依赖闭包。更新版本时，在受控的干净
Windows / Python 3.13.15 环境解析两个顶层清单，从包元数据逐项复核实际
传递依赖、`http2` extra 和 Windows 环境标记，再更新约束并运行校验与
离线测试。当前锁定的是版本而非发行文件哈希；发布前仍需可信来源校验和
独立环境打包验证。现有 `.venv` 或 `out/service` 中的旧包不能作为最新源码
已发布的证据。

服务以私有 JSONL stdin/stdout 协议运行。主程序只在内存中持有当前播放会话的地址；不会持久化播放 URL、查询参数、Cookie、Token、签名、原始弹幕帧或原始服务诊断。

搜索结果的可选布尔字段 `statusKnown` 默认 `true`，兼容旧服务。名单提示在元数据查询失败时使用 `statusKnown: false`；此时 `online` 不是权威开播状态，界面显示“状态未知”，不触发离线通知。精确名单查询补充房间资料，模糊查询合并名单与远程结果并按房间号去重。

导航页可见时每分钟刷新开播状态，已验证的房间资料与头像使用缓存。工作区保存失败会保留内存中的更改并显示重试入口。活动角色映射在设置页编辑，存储于 `DouyuMonitor/eventMappingV1`，JSON 版本为 1；远端明确的队伍优先于本地队长回退配置，角色配置不代表远端角色自动同步。

## 后台托管

关闭按钮按设置执行询问、后台托管或完全退出；最小化会隐藏主窗口并进入 Windows 托盘。后台期间 StreamGet、房间状态检测、系统通知和 libmpv 音频继续运行；Qt Quick 视频渲染和弹幕展示暂停。通过托盘“显示窗口”恢复时，播放器重新建立渲染上下文并恢复弹幕展示；选择完全退出或托盘“退出程序”会停止服务并结束进程。

## 自测与安装包

运行程序自测：

```powershell
.\out\build\windows-x64-release\douyu_monitor_native.exe --self-test
```

构建安装包：

```powershell
.\scripts\build-windows-installer.ps1
```

输出文件：

```text
native/out/installer/DouyuMonitor-Setup-V1.0.0.exe
```

发布入口拒绝已经存在的输出目录，不会清空旧安装器或诊断文件。再次构建时，
使用一对尚不存在的 `-StageRoot` 和 `-InstallerOutputDir` 路径。
每次编译的 `build-result.json` 记录独立运行 ID、编译器和 Stage 门禁退出码、
安装器大小与 SHA-256。只有入口返回零且结果为 `state: validated`，
并且文件大小、哈希仍与记录一致时，才能将该文件认作本次通过检查的候选产物。
失败输出与 manifest 保留在本次目录中；缺失、损坏或非 `validated` 的结果
不能作为成功依据。这里验证的是编译、Stage 一致性及非空普通文件输出，
不代表安装器内部载荷一致或真实安装运行已通过验证。

可用 `scripts/verify-installer-build-result.ps1 -ResultPath <输出目录>\build-result.json`
只读复核结果记录、实际输出文件身份和记录所引用的 Stage manifest 比较。该检查用于
发现记录与文件不一致，不是数字签名或防篡改证明；它不验证 Inno Setup 安装器内部载荷，
也不代替真实安装、首次启动或卸载验证。

候选交付或复用前，可使用
`scripts/verify-installer-candidate.ps1 -ResultPath <输出目录>\build-result.json -EvidenceRoot <新证据目录>`
进行事后只读核验。该命令使用构建时保存的 Stage manifest 作为基准，在指定证据目录生成
独立的 `candidate-verification.json`，并返回 `verified`、`drift_detected`、
`invalid_evidence`、`insufficient_evidence` 或 `verification_error`。只有 `verified`
返回零；它仍不验证安装器内部载荷或真实安装运行，也不会更新构建基准。

安装器使用 Inno Setup 6。V1.0.0 稳定版安装包名为 `DouyuMonitor-Setup-V1.0.0.exe`，版本从 CMake 项目版本自动读取；24 路全解码测试版仍保留在历史 Release，不属于本次稳定版交付。安装时可选择目标目录；选择 `D:\` 等磁盘根目录时会自动归一化到 `D:\DouyuMonitor`。安装完成后创建开始菜单和可选的桌面快捷方式，并登记到 Windows 设置的“已安装的应用”。安装目录中的 `unins000.exe` 是标准卸载入口。运行时载荷校验会拒绝测试程序、构建残留以及 Electron、Chromium、Node、Qt WebEngine 文件。

安装器脚本回归检查：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\test-installer-script.ps1
```

重新生成应用图标：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\generate-app-icon.ps1
```

## 验收边界

自动化验证覆盖 C++、QML、服务协议、libmpv 依赖与自测入口。正式版布局最多支持 16 路并全部解码，24 路全解码测试版通过独立构建开关生成。发布前仍应在目标用户环境完成真实斗鱼 1/4/9/12/16/24 路长时间播放验收，重点观察高路数下的弹幕、CPU/GPU、内存和关闭稳定性。
