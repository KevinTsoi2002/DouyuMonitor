# DouyuMonitor

DouyuMonitor 是一个基于 Electron 的斗鱼多直播间监看工具。它支持把多个房间放在同一窗口中查看，并在对应画面上叠加弹幕。

## 下载

Windows x64 安装包位于 [Releases](https://github.com/KevinTsoi2002/DouyuMonitor/releases/latest)。下载 `DouyuMonitor-Setup-0.1.1-x64.exe` 后运行安装程序即可。

当前发布包未进行 Windows 代码签名。原因是构建环境没有可用的、带 Code Signing 用途的受信任证书。Windows 可能显示“未知发布者”；请从本项目的 GitHub Release 下载，并在安装前核对 Release 中的 SHA-256 校验值。

## 功能

- 按直播间号或主播名字搜索并添加房间
- 多种画面布局：单画面、2x2、3x2、3x3、横向、纵向和主画面布局
- 主画面布局支持拖动分隔线调整画幅，切换主直播间时直接对调位置
- 每个房间独立显示滚动弹幕，可设置弹幕密度、字号、速度、透明度和颜色
- 房间列表支持历史记录、收藏和自定义分组
- 支持设置主直播间、独立音量、全局静音和默认音量 50%
- 房间资料和开播状态会定时刷新；未开播房间不会请求播放源
- 仅接受经过 CDN 白名单校验的独立 HTTP/HTTPS FLV 播放地址

## 使用

### 直接运行源码

环境要求：Node.js 20 或更高版本、Python 3.11 或更高版本。

```powershell
npm install
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements-streamget.txt
npm run build
npm start
```

Electron 主进程会自动查找 `.venv\Scripts\python.exe`。如果使用其他 Python，可设置 `STREAMGET_PYTHON` 指向解释器路径。

### 构建 Windows 安装包

打包前安装 PyInstaller：

```powershell
.\.venv\Scripts\python.exe -m pip install -r requirements-streamget-build.txt
npm run dist:win
```

安装包输出到 `release\DouyuMonitor-Setup-0.1.1-x64.exe`。`npm run dist:unpacked` 可生成 `release\win-unpacked\DouyuMonitor.exe` 用于本地验证。

## 播放源与合规边界

Python 桥接程序只调用 StreamGet 的 `DouyuLiveStream.fetch_app_stream_data()` 获取播放数据。应用不会调用网页解析器、生成斗鱼网页签名、保存带查询参数的播放 URL，也不会记录 Cookie、Token 或请求头。

应用只接受独立播放器可用且主机名通过 CDN 白名单校验的 HTTP/HTTPS FLV 地址。平台没有提供合规直连播放源时，房间仍会保留在列表中并显示平台阻塞状态。播放地址只在当前房间会话内存中使用，平台过期后需要重新获取。

当前生产模式通常只返回一个 `auto` 清晰度变体，因此清晰度选择器可能不可用；测试模式提供五种模拟清晰度用于验证界面。

## 数据与隐私

渲染进程会把房间顺序、布局、主房间、音频焦点、清晰度、弹幕偏好、音量和静音状态保存到本地浏览器存储。快照不包含播放 URL、查询参数、Token、Cookie、凭据或弹幕内容。应用启动后会重新检查播放可用性。

## 开发检查

```powershell
npm test
npm run typecheck
npm run build
```

性能基线需要先生成 `release\win-unpacked\DouyuMonitor.exe`，并通过环境变量提供当前在线的房间号。测试报告写入系统临时目录，不包含播放 URL 或凭据。
