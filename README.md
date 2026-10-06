# DouyuMonitor

DouyuMonitor 是基于 Qt Quick/QML、C++ 和 libmpv 的 Windows x64 斗鱼多直播间监看工具。当前 `main` 只维护原生 Qt 实现，正式版布局最多支持 16 路并全部解码，24 路全解码测试版通过独立构建开关生成；旧 Electron/TypeScript 实现已从 `main` 移除，并保存在 `legacy-framework` 分支供历史追溯。

## 界面预览

图一 单直播间监看，顶部信息栏显示热度，右侧房间列表可展开收起：

![单直播间监控](docs/images/screenshot-single-room.png)

图二 多直播间同时监看，每路独立弹幕、音量、清晰度和开播状态：

![多直播间监控](docs/images/screenshot-multi-room.png)

图三、图四 房间列表支持历史与收藏，并显示主播头像和当前开播状态：

![房间列表与历史收藏一](docs/images/screenshot-room-sidebar.png)

![房间列表与历史收藏二](docs/images/screenshot-room-sidebar-2.png)

郎团S1野榜支持综合排名、定级赛榜和游乐值榜：

![郎团S1野榜综合排名](docs/images/screenshot-maozi-rank.png)

![郎团S1定级赛榜](docs/images/screenshot-maozi-placement.png)

![郎团S1游乐值榜](docs/images/screenshot-maozi-play-value.png)

24 路全解码测试版在 1920x1080 下的自动布局：

![24路自动布局](docs/images/screenshot-24-rooms.png)

## 当前版本

- 正式版：`V0.2.16`
- Windows 正式版安装包：`DouyuMonitor-Setup-V0.2.16.exe`
- 24 路全解码测试版：`V0.2.16-beta.24`
- Windows 24 路测试版安装包：`DouyuMonitor-Setup-V0.2.16-beta.24.exe`
- 正式版 Release：[DouyuMonitor V0.2.16](https://github.com/KevinTsoi2002/DouyuMonitor/releases/tag/V0.2.16)
- 24 路测试版 Release：[DouyuMonitor V0.2.16-beta.24](https://github.com/KevinTsoi2002/DouyuMonitor/releases/tag/V0.2.16-beta.24)
- 未提供代码签名；下载后请以 Release 页面中的 SHA-256 值校验安装包

## 功能

- 按房间号或主播名搜索并添加斗鱼直播间
- 自动布局、主直播间布局和双主直播间布局
- 正式版自动布局、主直播间布局和双主直播间布局最多 16 路并全部解码；双主布局要求至少 4 路，两个主画面均享有高画质优先级
- 24 路全解码测试版支持三种布局最多 24 路，并同时创建 24 路播放器
- 每个房间独立弹幕、弹幕过滤、重复抑制和峰值治理
- 房间资料、主播头像、标题、热度和开播状态定时刷新
- 历史记录、收藏、自定义分组和工作区预设
- 顶栏房间列表按钮右侧提供CSTG狼团S1导航页入口，按独立队伍展示内置公会名单；未分配成员进入“未分队”，队伍管理位于“设置 -> 队伍管理”
- CSTG狼团S1导航页按“团长、队伍、队长、队员、其他”分层展示；角色通过设置页的本地活动配置维护，主播列表按昵称拼音首字母排序
- 鼠标悬停导航页主播可查看雷达图、定级赛总评和游乐值，详细数据位于“郎团S1野榜”页面
- 导航页可见期间按版本号增量同步野榜数据，版本未变化时不重复拉取完整快照
- 主播开播状态写入本地缓存，短时间重复打开导航页不会再次排队检查全部主播
- 声音总控、单声道/多声道、独立音量和默认音频焦点
- StreamGet 动态清晰度列表、播放源重试和状态 Toast
- 应用级全屏，支持 F11 切换和 Escape 退出
- Windows 系统通知、快捷键和关闭生命周期保护
- Windows 托盘后台托管：隐藏窗口后继续保留音频、状态检测和通知，暂停视频与弹幕渲染；从托盘恢复时自动重新渲染

## 技术边界

- UI：Qt Quick/QML
- 应用逻辑：C++20
- 播放：libmpv + OpenGL（发布目录加载名为 `mpv.dll`）
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

关闭窗口或最小化会进入 Windows 托盘后台托管，不会停止直播音频、StreamGet 或状态通知。需要真正退出时，在托盘图标右键选择“退出程序”；托盘双击或选择“显示窗口”可恢复画面和弹幕。

### 构建 Windows 安装包

先构建 Release，再执行：

```powershell
.\scripts\build-windows-installer.ps1
```

安装包输出到 `native/out/installer/DouyuMonitor-Setup-V0.2.16.exe`。后续版本会自动将版本号加入安装包文件名。构建 24 路全解码测试版时执行 `.\scripts\build-windows-installer.ps1 -ReleaseDir .\out\build\windows-x64-beta24 -Version 0.2.16-beta.24`。运行安装包时可选择安装目录；如果选择 `D:\` 这样的磁盘根目录，安装器会自动使用 `D:\DouyuMonitor`，不会把程序文件直接写入根目录。安装完成后会创建开始菜单和可选的桌面快捷方式，并可直接启动程序。卸载入口由 Inno Setup 生成的 `unins000.exe` 提供，同时登记到 Windows 设置的“已安装的应用”。

CSTG狼团S1导航页展示应用内置的公会名单、主播头像和开播状态，不根据直播资料动态扩张名单。已确认房间号可以直接加入当前工作区；无法自动确认的成员需要在行内手工输入房间号。快捷加入受当前构建的布局容量限制。隐藏导航页的空工作区启动时不会为名单检查启动服务。

### CSTG狼团S1导航与郎团S1野榜

导航页按“团长 -> 队伍 -> 队长 -> 队员 -> 其他”排列主播。设置页的“活动角色（本地配置）”支持维护角色和队长回退队伍；远端明确队伍优先于回退配置，角色配置不代表远端角色自动同步。导航页中存在但野榜中没有记录的成员归入“其他”。每个小分类内按主播昵称拼音首字母排序。队伍管理支持预览后从野榜一键导入，身份匹配有歧义的主播不会被移动。

鼠标悬停在主播上会显示共享卡片，包含雷达图、定级赛总评和游乐值；详细数据位于郎团S1野榜页面。导航页可见时每 60 秒做一次轻量版本检查并刷新开播状态，野榜版本变化后才重新拉取完整快照；同步失败时保留上一次成功数据并提示数据可能已过期。重复打开导航页会复用 15 分钟内的开播缓存；可见期间的定时状态刷新不受该缓存期限限制。状态更新保留现有列表行，收起导航页、切换页面或窗口失去激活时会关闭悬浮卡片。

郎团S1野榜页面展示排名、主播、房间号、队伍、评级、综合评分、参与人数和开播状态：

![郎团S1野榜](docs/images/screenshot-rank-page.jpg)

队伍管理位于“设置 -> 队伍管理”，队伍是导航页的主分组，可创建空队伍并调整成员归属：

![队伍管理](docs/images/screenshot-team-manager.jpg)

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

发布前至少执行 Release 全量 CTest、安装包 stage 的 `--self-test` 和载荷扫描。真实斗鱼 1/4/9/12/16/24 路长时间播放、弹幕稳定性及用户机器上的 CPU/GPU/内存验收仍需在目标环境完成。
