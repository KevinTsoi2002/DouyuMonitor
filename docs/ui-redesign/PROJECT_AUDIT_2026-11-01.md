# 全项目审查（2026-11-01）

## 修复收尾更新（2026-11-01 07:21 Asia/Shanghai）

本轮已实现并验证审计中确认的安全、状态、持久化、通知、键盘交互、头像回退、响应体边界和弹幕建连期限修复。新增的赛程详情键盘回归先复现了 `ConsoleButton` 获得焦点但 Return 不打开详情的问题，随后在 `native/app/qml/pages/SeasonArchivePanel.qml` 为现有详情动作增加 Return、Enter、Space 显式处理，并在 `native/tests/qml_interaction_test.cpp` 覆盖 Return 与 Space；定向用例和完整交互目标均通过。

验证结果：Windows x64 Release 目标构建退出0；Python 服务测试 62/62 通过；完整串行 CTest 两次均为 39/39 通过（最新一次总耗时 273.72 秒，包含 `streamget_process_client_test`）；生产程序 `D:/DouyuMonitor/native/out/verification/fix-release-utf8/douyu_monitor_native.exe --self-test` 退出0，输出 `native self-test passed: Qt Quick renderer`。`git diff --check` 退出0，仅报告工作区既有的换行转换提示。

先前 `streamget_process_client_test` 的 CTest 间歇性空输出失败仍保留为未完全定位的可靠性风险；本轮最新两次完整运行通过不能证明其根因已消除。真实斗鱼长时播放/弹幕、网络黑洞、DPI/GPU矩阵、真实安装卸载和全量依赖CVE扫描仍未执行。未提交、未推送，也未清理工作区既有修改。

时区：Asia/Shanghai。范围为 `D:/DouyuMonitor` 当前 `main` 工作区；审查为只读，没有修改生产源代码。

## 结论

发现一项安全配置缺陷和八项用户可见的功能/可用性问题。全套 CTest 两次均有同一子进程测试失败；隔离运行多次通过，但也有一次失败，失败输出为空，当前证据只能确认该测试有间歇性失败，无法定位根因。真实长时间直播、弹幕连接挂起、DPI/驱动和真实安装部署没有本轮验收。

审查开始时 `main` HEAD 为 `968557b7ea78359ddb88aa673e0a3d1567b008b3`，工作区已有182条未提交/未跟踪状态，均予保留。当前 `native/CMakeLists.txt` 版本为0.2.20，Qt6.12.0，默认房间容量16、可选24；不沿用旧文档中的9/10路结论。

## 已确认发现

### P1：斗鱼解析请求关闭 TLS 证书校验

`native/service/douyu_backend.py:381` 使用默认 `DouyuLiveStream()`；仓库锁定 `streamget==4.0.10`（`native/service/requirements-lock-win-py313.txt:30`），其 HTTP 客户端默认 `verify=False`。使用隔离模拟响应执行真实库方法，观测到传给 `httpx.AsyncClient` 的选项确为 `verify:false`；无网络请求、无凭据或播放地址输出。中间人可以伪造 HTTPS 响应，影响房间/播放源解析。证据：`native/out/verification/project-audit-20261101/streamget-tls-proof.json`。修复需为生产解析路径启用证书校验，验证库兼容性并增加断言测试。

### P2：过期的榜单会话不会自动重新认证

`native/src/app/maozi_rank_client.cpp:279` 在缓存有令牌时一直用原令牌检查版本；`handleVersionReply()` 将非2xx（包括401/403）转成错误，但不清空令牌或触发一次新会话。真实 `MaoziRankClient` 配合本地 HTTP 夹具复现：初次加载4条数据后，连续两次401检查结果为认证请求1次、版本请求2次、数据保留4条；自动同步会持续失败，直到手动刷新。证据：`native/out/verification/project-audit-20261101/rank-expiry/result.txt`。

### P2：状态接口返回错误时仍保留“在线”缓存

`native/src/workspace/guild_room_resolver.cpp:365` 对合法的服务错误响应只安排下一次请求；它没有像传输失败分支那样将状态改为 `unknown`。已有状态的新鲜期为15分钟。使用生产 resolver 和 `SERVICE_FAILED` 响应的隔离夹具复现：期望 `unknown`，实际仍为 `online`。结果：`native/out/verification/project-audit-20261101/guild-status-error/result.txt`。用户会看到过期的在线状态。

### P2：已支持的单主布局无法持久化

`native/app/qml/panels/WatchToolsPanel.qml` 提供 `primary` 布局，但 `native/src/workspace/native_workspace_store.cpp:24` 的有效值列表遗漏 `primary`，保存归一化会在第211行将其改成 `auto`。隔离原生 Qt 探针验证保存 `primary` 后重新加载为 `auto`。证据：`native/out/verification/project-audit-20261101/domain-probe-result.txt`。

### P2：应用“无声音焦点”预设会恢复房间声音

`native/src/ui/app_controller.cpp:1433` 只在预设 `audioRoomId` 非空时恢复焦点；此前 `replaceRooms()` 已默认选择活动房间。工作区重启恢复在同文件1806行有明确的空焦点清理，预设路径缺少对应处理。隔离原生 Qt 探针复现：静音预设的焦点为空，切换状态后重新应用，实际焦点变成 `63136`，且全局未静音。证据同上。

### P2：真实播放恢复不会触发“播放已恢复”通知

`native/src/workspace/notification_policy.cpp:76` 只识别 `Error → Playing`，并在每次更新时覆盖前一状态。实际播放恢复会经过 `Error → Pending → Playing`，中间的 `Pending` 抹掉错误状态。隔离原生 Qt 探针得到错误通知1条、恢复通知0条。现有策略测试只覆盖直接跳转，没覆盖真实状态序列。证据同上。

### P2：榜单详情仅支持鼠标点击

`native/app/qml/pages/MaoziRankPage.qml:623` 和1197处的排名/上场记录行依赖 `TapHandler`，没有可 Tab 聚焦或 Enter/Space 激活的键盘入口及对应辅助操作。键盘和辅助技术用户不能打开主播详情/雷达图。现有交互测试只用鼠标触发详情。

### P2：主播导航没有尝试控制器提供的备用头像

`native/src/ui/app_controller.cpp` 提供经过身份校验的 `fallbackAvatarUrl`，但 `native/app/qml/components/GuildNavigationPanel.qml:84` 重建行时只保留 `avatarUrl`，行组件只有单一 `Image.source`。当在线头像请求失败时，后备头像不会进入候选序列，导航仍可能显示缺图。

### P3：历史/收藏列表的头像请求失败会遮住占位

`native/app/qml/components/RoomLibraryView.qml:101` 只根据 URL 非空显示图片；URL 存在但图片加载失败时，文字占位仍被隐藏。影响历史和收藏复用此组件的条目。

## 待核实风险

- 弹幕10秒握手计时器在 WebSocket `connected` 后才启动（`native/src/danmaku/douyu_danmaku_client.cpp:88`）。若 TCP/TLS/WebSocket 建连卡住且 Qt 尚未发出错误，可能长期停在 Connecting。本机 Qt6.12 公共 QWebSocket API 未见可设置的连接期限；本轮未做网络黑洞运行验证，列为可靠性调查项。
- 榜单/更新请求对响应体大小没有硬上限，且 `posterUrl` 可从上游数据形成任意 HTTP(S) 图片地址。本轮未证明远端可控性、利用路径或实际资源耗尽，不列为已确认漏洞。
- 旧的 `native/app/uninstaller_main.cpp` 通过 `cmd.exe /c` 安排删除目录；但当前 `native/installer/DouyuMonitor.iss` 使用 Inno Setup 自带的 `{uninstallexe}`，且安装暂存脚本明确拒绝 `douyu_monitor_uninstaller.exe`。本次隔离暂存验证通过，因此旧原生卸载器不属于当前安装包的已确认执行路径；真实安装/卸载仍未验收。
- 静态扫描提示需语义判断：115个C++文件有407条候选、43个QML文件有603条候选（含测试/风格规则）；没有将这些扫描数或风格项当作缺陷数量。

## 验证与覆盖边界

- 全目标 Release 构建：通过，Windows x64，Qt6.12.0 / MSVC14.51。
- Python StreamGet 单元测试：61/61通过。
- 安装器脚本静态回归通过；隔离 Stage 完整性测试通过；安装结果证据契约55/55通过。后者使用模拟编译器，不能代表真实安装包构建/安装/卸载验收。
- 全套 CTest 两轮：均为38/39通过；唯一失败均为 `streamget_process_client_test`，测试输出为空。后续隔离 CTest 运行也出现通过与失败：`--repeat until-fail:3` 前两次通过（7.31秒、7.24秒），第三次失败（8.21秒）；另一次 `ctest -V` 隔离运行失败（8.21秒）。同一测试在 CTest 工作目录下直接运行并把 QtTest 明细写入文件，连续4次均为28/28通过（7.07–7.40秒）。证据确认这是间歇性测试失败，但没有定位到具体断言或根因，不能归因于 CreateFile 权限。日志位于 `native/out/verification/project-audit-20261101/ctest-all.txt`、`ctest-all-retry.txt`、`ctest-stream-process-retry.txt` 和 `streamget-process-client-repeat-1.txt` 至 `streamget-process-client-repeat-3.txt`；CTest 最近运行日志在重跑后只保留最近结果。
- 三项纯领域问题在隔离 Qt 原生探针中按预期稳定复现3/3。榜单过期认证和公会状态错误分别用生产类与本地夹具复现。
- 未执行真实斗鱼长播/弹幕、网络挂起、设备DPI/GPU矩阵、真实安装包安装卸载或全量依赖CVE扫描。

## 范围与工具

读取仓库 `AGENTS.md`、全局规则、Qt C++/QML审查流程、CMake、原生服务/数据/播放/弹幕/持久化、43个QML、测试和安装打包脚本。Serena和Context7本轮没有暴露在可用工具中，使用源码、构建、隔离本机夹具和本地 Qt API 头文件验证。Notion检查点与本地报告关联见项目页记录100–103。
