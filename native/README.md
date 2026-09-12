# 原生 Qt 客户端

`native` 是 DouyuMonitor 当前唯一维护的桌面客户端：Qt Quick/QML 负责界面，C++20 负责应用逻辑，libmpv 负责播放，独立 `streamget_service.exe` 负责斗鱼资料与播放变体解析。

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
- Qt 6.8.3 MSVC 2022 x64
- libmpv SDK：`include/mpv/client.h`、`libmpv.lib`、`libmpv-2.dll`

`vcpkg-configuration.json` 固定项目依赖基线。libmpv 不通过 vcpkg 分发，必须由 `MPV_ROOT` 显式指定。

## 配置和构建

在 `native` 目录内的 Visual Studio x64 Developer PowerShell 中设置环境变量：

```powershell
$env:QT_ROOT = 'D:\Qt\6.8.3\msvc2022_64'
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
cmake --build --preset windows-x64-release --target douyu_monitor_native
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

服务以私有 JSONL stdin/stdout 协议运行。主程序只在内存中持有当前播放会话的地址；不会持久化播放 URL、查询参数、Cookie、Token、签名、原始弹幕帧或原始服务诊断。

## 后台托管

关闭窗口和最小化都会隐藏主窗口并进入 Windows 托盘。后台期间 StreamGet、房间状态检测、系统通知和 libmpv 音频继续运行；Qt Quick 视频渲染和弹幕展示会暂停，以降低后台资源占用。通过托盘“显示窗口”恢复时，播放器重新建立渲染上下文并恢复弹幕展示。托盘“退出程序”才会停止服务并结束应用进程。

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
native/out/installer/DouyuMonitor-Setup-V0.2.4.exe
```

安装器使用 Inno Setup 6。安装包文件名会自动包含项目版本，例如 `DouyuMonitor-Setup-V0.2.4.exe`。安装时可选择目标目录；选择 `D:\` 等磁盘根目录时会自动归一化到 `D:\DouyuMonitor`。安装完成后创建开始菜单和可选的桌面快捷方式，并登记到 Windows 设置的“已安装的应用”。安装目录中的 `unins000.exe` 是标准卸载入口。运行时载荷校验会拒绝测试程序、构建残留以及 Electron、Chromium、Node、Qt WebEngine 文件。

安装器脚本回归检查：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\test-installer-script.ps1
```

重新生成应用图标：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\generate-app-icon.ps1
```

## 验收边界

自动化验证覆盖 C++、QML、服务协议、libmpv 依赖与自测入口。发布前仍应在目标用户环境完成真实斗鱼 1/4/6/9/10 路长时间播放验收，重点观察第十路、弹幕、CPU/GPU、内存和关闭稳定性。
