# 更新检查功能设计

## 目标

在设置页提供“检查更新”能力，查询 `KevinTsoi2002/DouyuMonitor` 的 GitHub Releases 最新正式版本，与当前构建版本比较，并将结果清晰反馈给用户。功能只负责检查和打开 Release 页面，不自动下载或安装。

## 用户体验

- 点击“检查更新”后显示检查中状态，并防止重复请求。
- 最新正式版本不高于当前版本时显示“已是最新版本”。
- 发现更高版本时显示版本号，并提供打开 GitHub Release 页面的小按钮/链接。
- 网络错误、HTTP 错误、超时、JSON 无效或 Release 缺少必要字段时显示可理解的失败信息。
- 支持 Release 标签 `V0.2.3`、`v0.2.3` 和 `0.2.3`；比较三段数字版本，忽略前导 `v/V`。

## 架构

新增 `UpdateChecker` C++ 类，封装 `QNetworkAccessManager`、GitHub API 请求、超时、HTTP 状态检查、JSON 解析和版本比较。请求地址默认为：

`https://api.github.com/repos/KevinTsoi2002/DouyuMonitor/releases/latest`

类支持注入 API 基础地址，测试使用本地 `QTcpServer`，避免依赖真实 GitHub 网络。请求设置明确的 `User-Agent`，仅接受正式 Release（`draft=false` 且 `prerelease=false`）。

`AppController` 持有 `UpdateChecker`，暴露 QML 属性、状态信号和 `checkForUpdates()`/`openLatestRelease()` 调用。当前版本从 CMake 项目版本注入到 C++，不在 QML 中硬编码。QML 只负责展示状态和调用控制器，不直接处理平台网络或 URL 打开逻辑。

## 数据流

1. QML 点击按钮。
2. `AppController::checkForUpdates()` 将状态设为 checking，并委托 `UpdateChecker`。
3. `UpdateChecker` 发起 GET 请求并启动超时计时器。
4. 成功响应解析 `tag_name` 和 `html_url`，完成版本比较并发出结果。
5. `AppController` 转发状态、消息、最新版本和 Release URL。
6. QML 按状态更新文本；用户点击打开按钮时由 `AppController` 使用 `QDesktopServices::openUrl()`。

## 错误处理

- 非 2xx HTTP 状态码统一为检查失败，不展示服务端原始响应。
- 网络错误和超时统一结束为 error 状态，保留适合用户阅读的中文消息。
- JSON 不是对象、缺少字符串 `tag_name`、缺少合法 `html_url`，或版本标签无法解析时视为无效响应。
- 新请求开始前取消上一次未完成请求；析构时由 Qt 父子关系清理请求。

## 测试

- 版本标签解析与三段数字比较。
- 本地 HTTP 服务返回成功、HTTP 错误、无效 JSON、缺少字段和超时。
- `AppController` 的属性/信号转发及打开 Release URL 调用可测试。
- QML 交互测试点击按钮后出现 checking 和最终结果，按钮在请求期间不可重复触发。

## 非目标

- 不下载、校验或安装更新。
- 不检查预发布版本、草稿 Release。
- 不增加后台定时检查或启动时自动检查。
