# DouyuMonitor

DouyuMonitor 是基于 Qt Quick/QML、C++ 和 libmpv 的 Windows x64 斗鱼多直播间监看工具。当前 `main` 只维护原生 Qt 实现，最多支持 9 路直播；旧 Electron/TypeScript 实现已从 `main` 移除，并保存在 `legacy-framework` 分支供历史追溯。

## 当前版本

- 发布版本：`V0.2.3`
- GitHub Release：[DouyuMonitor V0.2.3](https://github.com/KevinTsoi2002/DouyuMonitor/releases/tag/V0.2.3)
- Windows 安装包：Release 中的 `DouyuMonitor-Setup-V0.2.3.exe`
- 未提供代码签名；下载后请以 Release 页面中的 SHA-256 值校验安装包

## 功能

- 按房间号或主播名搜索并添加斗鱼直播间
- 单画面、2x2、3x2、3x3、横向、纵向和主画面布局
- 最多 9 路同时播放，主画面支持拖动分隔线
- 每个房间独立弹幕、弹幕过滤、重复抑制和峰值治理
- 房间资料、主播头像、标题、观众数和开播状态定时刷新
- 历史记录、收藏、自定义分组和工作区预设
- 声音总控、单声道/多声道、独立音量和默认音频焦点
- StreamGet 动态清晰度列表、播放源重试和状态 Toast
- 应用级全屏，支持 F11 切换和 Escape 退出
- Windows 系统通知、快捷键和关闭生命周期保护

## 技术边界

- UI：Qt Quick/QML
- 应用逻辑：C++20
- 播放：libmpv + OpenGL
- 斗鱼解析：独立 `streamget_service.exe` 子进程
- 弹幕：Qt WebSockets 原生客户端
- 安装器：Inno Setup 6 Windows 安装器
- 不使用 Electron、Chromium、Node.js、Qt WebEngine 或网页播放器

## 快速开始

### 环境要求

- Windows x64
- Visual Studio 2022，含 MSVC x64 工具链
- CMake 3.24 或更高版本、Ninja
- Qt `6.8.3` MSVC 2022 x64
- 已准备好的 libmpv SDK，包含头文件、`libmpv.lib` 和 `libmpv-2.dll`

### 配置和构建

在项目根目录的 Visual Studio x64 Developer PowerShell 中设置依赖路径：

```powershell
$env:QT_ROOT = 'D:\Qt\6.8.3\msvc2022_64'
$env:MPV_ROOT = (Resolve-Path .\native\sdk\mpv).Path
Set-Location .\native
```

后续命令均在同一终端的 `native` 目录内执行：

```powershell
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release --target douyu_monitor_native
ctest --preset windows-x64-release
```

启动 Release 程序：

```powershell
.\out\build\windows-x64-release\douyu_monitor_native.exe
```

### 构建 Windows 安装包

先构建 Release，再执行：

```powershell
.\scripts\build-windows-installer.ps1
```

安装包输出到 `native/out/installer/DouyuMonitor-Setup-V0.2.3.exe`。后续版本会自动将版本号加入安装包文件名。运行安装包时可选择安装目录；如果选择 `D:\` 这样的磁盘根目录，安装器会自动使用 `D:\DouyuMonitor`，不会把程序文件直接写入根目录。安装完成后会创建开始菜单和可选的桌面快捷方式，并可直接启动程序。卸载入口由 Inno Setup 生成的 `unins000.exe` 提供，同时登记到 Windows 设置的“已安装的应用”。

## StreamGet 服务

服务源码位于 `native/service`。首次准备环境：

```powershell
.\scripts\bootstrap-streamget-service.ps1
.\scripts\build-streamget-service.ps1
```

服务通过私有 JSONL stdin/stdout 协议与 Qt 主程序通信。播放地址只在当前房间会话内存中使用，不写入日志、配置或诊断文件；Cookie、Token、签名和原始弹幕帧同样不会持久化。

## 目录和文件职责

完整的逐文件职责说明见：[文件职责索引](docs/文件职责索引.md)。主要边界如下：

| 目录 | 职责 |
| --- | --- |
| `native/app` | Qt 应用入口、QML 组件、面板、对话框和图标资源 |
| `native/src/ui` | QML 暴露的控制器、模型和 libmpv Quick Item |
| `native/src/workspace` | 房间会话、工作区持久化、布局、质量、通知和状态调度 |
| `native/src/media` | 播放源模型和 StreamGet 播放控制 |
| `native/src/service` | Qt 与 StreamGet 子进程之间的协议和进程管理 |
| `native/src/danmaku` | 斗鱼弹幕协议、Socket、治理和会话管理 |
| `native/service` | 独立的 StreamGet Python 服务及单元测试 |
| `native/tests` | C++、QML 和服务回归测试 |
| `native/cmake` | 运行时依赖校验、Qt 部署和服务复制脚本 |
| `native/scripts` | 依赖准备、服务构建和 Windows 安装包脚本 |
| `docs` | 当前 Qt 设计、计划、文件职责索引和中文开发日志；保留的 2026-08-24 迁移日志维持原始审计记录 |

## 验证

发布前至少执行 Release 全量 CTest、安装包 stage 的 `--self-test` 和载荷扫描。真实斗鱼 1/4/6/9 路长时间播放、弹幕稳定性及用户机器上的 CPU/GPU/内存验收仍需在目标环境完成。
