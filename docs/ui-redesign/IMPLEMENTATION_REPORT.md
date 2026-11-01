# 原生 UI 实施报告

## 2026-11-01 四项反馈：当前本地结果

本轮已取消鼠标悬停提示，保留键盘焦点与辅助名称；补齐12位历史成员公开头像，并为排行榜/定级赛/详情提供在线→解析器→可信公开资源失败回退。相同URL的状态更新保持已成功候选，避免反复请求坏图。导航保持62个目录行，过滤只切换可见和禁用状态，九次同步筛选从6–147ms降至5–10ms，创建/销毁186/186降为0/0。外部解析完成时关闭旧手动输入框。

构建、控件13/0、引擎5/0、完整交互115/0、视觉烟测35/0、领域CTest5/5及最终EXE渲染自测通过。46张真实头像网络解码通过；生产页面排行46/46、定级赛36/36及美伢详情Ready。范围、日志及限制见[四项修复](FOUR_ISSUE_REPAIR.md)和[播放链接核查](PLAYBACK_LINK_LIFETIME.md)。本轮不修改登录或播放协议；未完成真实长播/设备DPI/安装包及整体Figma。以下九项记录为上一轮历史。

更新：2026-10-31，Asia/Shanghai。状态：九项截图反馈的本地实现、相关回归与审图已完成；114/0完整交互、独占CTest1/1、四QML目标4/4、视觉烟测35/0、领域5/5和EXE自测exit0。详细日志及限制见TEST_VALIDATION_LOG.md最新节。上一轮六项目标 CTest 6/6 与交互 109/0 保留为历史证据。Notion 连接已恢复并已实际写入、回读，当前记录见 [NOTION_REMOTE_SYNC.md](NOTION_REMOTE_SYNC.md)；原整体重构及完整 Figma 交付仍未完成。

本报告对应用户已确认的 R3：C1 夜间/C3 白天、紧凑表格、3px 圆角、36px 标题栏、210×24px 图片字标、80px 导航轨、40px 品牌图标/26px 线性图标、按需覆盖工具和沉浸观看。规格见 [DESIGN_SYSTEM.md](DESIGN_SYSTEM.md)、[SCREEN_SPECIFICATIONS.md](SCREEN_SPECIFICATIONS.md) 和 [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md)。本报告不重新设置视觉确认关卡。

## 上一轮九项截图反馈：历史源码实施与验证边界

本节对应最新九张截图，范围与阶段记录见 [NINE_ISSUE_REPAIR.md](NINE_ISSUE_REPAIR.md)。以下内容描述实际源码修改；最新生产构建与相关回归结果见TEST_VALIDATION_LOG.md，分域夹具证据单独保留。

| 截图问题 | 当前实际实现 |
| --- | --- |
| 图1：榜单头像未加载 | `pages/MaoziRankPage.qml` 为 46 位排行与 36 位定级赛参与者关联线上/导航头像，使用稳定 ID、房间号及经核验的历史房间别名。`AppController` 的 `portraitIdentityVerified` 阻止手动更换房间后把无关头像传播到历史身份；保留缺图回退，不修改历史分数。 |
| 图2–4：赛事详情简陋 | `pages/ArchiveDetailModel.js`、`SeasonMatchDetail.qml`、`FootballMatchDetail.qml` 与 `SeasonArchivePanel.qml` 将常规/决赛整理为成绩、轮次和附加记录，将足球整理为比分、首发、事件、MVP；完整原文可展开核对，未知表格不猜列、不重算结果。修复足球积分 `overview → stand` 和无结构原文默认展开后不能收起的问题。 |
| 图5：导航悬浮数据窗 | `Main.qml` 删除悬浮卡、Timer、属性与信号链；`GuildMemberRow.qml` 仅保留轻量整行悬停反馈，资料由明确点击名称打开。设置内队伍管理入口保留。 |
| 图6：公会目录及添加闪烁 | `guild_directory.json` 保存本次官网公开目录 62 人（25 男、37 女）及来源顺序；名册合并保留旧 59 身份并增加 16 身份。`GuildNavigationPanel.qml` 按目录平铺，提供全部/男主播/女主播、搜索、直播中/已添加组合筛选，按 ID 移动和逐属性更新行；筛选排除已选主播时关闭其资料。 |
| 图7：九维雷达缺失 | `components/RadarChart.qml` 与榜单详情增加九维雷达及精确数值，兼容归档 `value` 与线上 `average`；缺失维度保持缺失，不补造评分。 |
| 图8：白天画内颜色 | `RoomTile.qml` 的画内按钮、音量、占位身份容器使用独立深色语义；`ControlBackground.qml`、`ConsoleToolButton.qml`、`ConsoleComboBox.qml` 通过默认关闭的 `videoSurface` 按需启用。未改 libmpv 视频色彩参数；占位画面检查不能证明真实视频色彩正常。 |
| 图9：清晰度文字位置 | 清晰度闭合标签与弹出选项居中，当前项显式高亮，移除多余指示器占位；保留已有清晰度命令和键盘选择。 |

当前分域证据：导航源 QML 5/0、头像/雷达 5/0、视频控件 6/0、档案详情 6/0；档案模型覆盖全部 45 场，保留 35 场常规/决赛原文和 10 场足球原始内容。四个真实公开头像由 Qt 原生网络加载到 `Image.Ready`，独立测试 3/0、未见 SSL/网络警告。目录名册 12/0、此前五项目标领域 CTest 5/5 属对应时点结果；之后身份别名安全、旧缓存与恢复、交互和资源登记修改仍须以最后重建及复跑为准。

四尺寸双主题的档案/导航源截图和原生详情矩阵已有实际运行证据，但集成中也出现需修复的测试失败，不能只引用源夹具结果。最新整合结果统一见 [TEST_VALIDATION_LOG.md](TEST_VALIDATION_LOG.md) 与 [VISUAL_QA_REPORT.md](VISUAL_QA_REPORT.md)。`layout-*`、房间菜单和清晰度 16 对旧新图片已独立检查：差异集中于画内控件颜色与清晰度对齐。后续18张限定基线已备份/更新且最终回归通过，完整记录见VISUAL_QA_REPORT.md最新节。

Notion 已恢复：历史 001–070 补录、071–078 后续步骤及分域证据已有远端写入与回读，具体链接和后续状态以 [NOTION_REMOTE_SYNC.md](NOTION_REMOTE_SYNC.md) 为准。历史实时同步缺口如实保留，未来步骤不自动视为同步。Figma 配额限制和空 Screens 等未完成项仍在下文单独记录。

## 上一轮六项修复的历史结论

六项修复前，后续构建与选择性 CTest 5/5 已更新更早的交互失败及 AutoMoc/QProcess 权限阻塞，质量弹窗定向测试为 3/0。这些记录保留为历史，不作为九项最新结果。上一轮六项使用下列 `six-issue-*` 日志；局部修复通过不等于原总体交付完成。

2026-10-31 的文档复审最初只记录问题，随后根据用户六张截图实际修改了生产 QML。修复前发现与现状已在 [PLAN_REVIEW.md](PLAN_REVIEW.md) 分开记录；六项范围见 [SIX_ISSUE_REPAIR.md](SIX_ISSUE_REPAIR.md)。

## 六项截图问题修复与当时证据（历史）

| 问题 | 实际改动及文件 |
| --- | --- |
| 标题栏主图标 | `components/AppHeader.qml` 增加独立 26×26 应用图标，保留 210×24 日夜图片字标；加载失败有文字回退。系统 ICO 与标题栏图标仍是不同资源检查项。 |
| 名称悬停和详情卡 | `GuildMemberRow.qml` 取消名称局部底色与重复 Tooltip；`GuildNavigationPanel.qml` 使用整行轻量反馈；`GuildRankHoverCard.qml` 按实际内容收缩，保留详情入口和键盘焦点。 |
| 筛选框对比度 | `controls/ConsoleComboBox.qml` 显式配对选项文字与背景，保留原生模型和键盘提交；原选中项约 1.365:1，修复后日夜均约 11.55:1。 |
| 重复添加入口 | `components/RoomSidebar.qml` 与 `WatchToolsPanel.qml` 整理为房间面板一个明确添加按钮，统一打开 `AddRoomDialog`；独立空工作区的添加操作保留。 |
| 观看工具交互 | `WatchToolsPanel.qml` 将声音/布局放入同一面板的子页；`Main.qml` 协调互斥辅助面板、返回、分级 Esc、外击关闭和焦点恢复。修复 Tooltip 阻断外击关闭的路径；完整交互 109/0，最终六目标 CTest 6/6。 |
| 白天文字及禁用状态 | 七类 `controls/Console*.qml` 改为语义角色绑定；`Main.qml`、四个有效 Dialog 与三个管理 Panel 的调色板逐角色绑定，重叠角色按 active/inactive/disabled 分组。生成代码确认旧 unused `Theme.mode` 读取被 AOT 省略；Qt 赋值只拷贝 Palette，且未分组角色在首次求值时会覆盖禁用色。新的生产模块测试直接覆盖这些失败路径。 |

上一轮六项修复没有改变播放器、服务协议、持久化或榜单统计规则。当时榜单只做最小窗口、列对齐和截图复核，不把它写为完成了全新榜单设计；最新九项实施见报告首节。

| 六项修复当时验证 | 实际结果与边界 |
| --- | --- |
| 生产程序 | `six-issue-build-verified.txt` 对应构建成功；`douyu_monitor_native.exe` 修改时间为 2026-10-31 20:50:07（Asia/Shanghai）。`six-issue-app-self-test.txt` 记录 `--self-test` 退出 0，Qt Quick renderer 自测通过。 |
| 编译模块主题 | `six-issue-engine-verified.txt`：5 passed / 0 failed。`loadFromModule` 检查 night/day/night/day/night，覆盖根窗口、七个弹窗、顶部/空态按钮与真实禁用 Switch；没有用源码解释执行夹具替代 AOT。 |
| 共享控件 | `six-issue-controls-final.txt`：13 passed / 0 failed；覆盖文本/图标、启用/禁用、选中状态、主题切换、筛选键盘选择及真实对比度。 |
| 视觉烟测 | `six-issue-visual-smoke-final.txt`：35 passed / 0 failed；包含尺寸与双主题截图。日志仍有引擎销毁时异步 item 警告，不写作零警告。 |
| 视觉回归 | `six-issue-visual-regression-final.txt`：7 passed / 0 failed。三张预期变更基线逐张审查，旧图保存至 `native/out/verification/six-issue-baseline-backup/` 后局部更新；最终复测未启用基线更新变量。 |
| 六项完整交互（历史） | `six-issue-interaction-final.txt`：109 passed / 0 failed，42.413 秒；当时二进制包含 tooltip 外击与布局选择回归。 |
| 最终六项目标 CTest | **6/6 通过，136.84 秒**：控件、引擎、交互、视觉烟测、视觉回归、关闭回归；`six-issue-ctest-final.txt`。不是完整仓库 CTest。 |
| 普通环境启动自测 | `six-issue-app-default-env.txt`：无额外 Qt 环境设置，`--self-test` 退出 0。 |

上述日志均位于 `native/out/verification/`。编译模块无标注截图位于构建目录 `verification/theme-controls/compiled-day.png`、`compiled-night.png`，已人工查看标题栏图标/字标、顶部按钮与空态文字。离线矩阵的汇总图和最小窗口、悬停等指定细节也已审查；这些证据不覆盖真实直播内容、所有设备/DPI或全部用户流程。完整仓库 CTest与原整体产品验收继续单独跟踪。

## 工作区与实现边界

- 实施开始前，仓库已有大量未提交修改。本次沿用该工作区并保留原有改动，不能将整个 Git diff 都归入 UI 重设计。
- 原生界面仍使用 Qt Quick/QML，领域命令经 `AppController` 调用既有工作区、播放器、StreamGet 和弹幕服务。网页仅用于视觉评审，没有引入网页播放器或 WebEngine。
- 主代理已保存 65 个 QML/入口/controller 文件快照：`native/out/ui-redesign-snapshots/before-confirmed-ui-20261030-015720.zip`。业务文件另有副本：`native/out/verification/ui-business-before-20261030/`。
- 当前源码默认容量为 Phase16；这是现有工作区事实。本次没有按旧文档的 9/10 路说明修改容量。服务、持久化及容量约束见 [DATA_CONSTRAINTS.md](DATA_CONSTRAINTS.md)。

## 已落盘的界面范围

下表描述源码中实际存在的实现，不代表该区域已通过全部运行和视觉验收。

| 区域 | 主要文件 | 实现内容与核验边界 |
| --- | --- | --- |
| 主题与基础控件 | `Theme.qml`、`controls/ThemeIcon.qml`、公共控件 | 日夜语义色、3px 圆角、32px 基本控件高度；视频用独立颜色。六项修复补充 AOT 安全角色绑定与选项对比，对应证据见六项历史节，未宣称全应用所有状态均已人工验收。 |
| 外壳与导航 | `Main.qml`、`AppHeader.qml`、`WindowControls.qml`、新增 `AppNavigationRail.qml` | 36px 标题栏、80px 导航轨；独立主图标与日夜图片字标现已显示。编译模块昼夜窗口与顶部/空态文字已复核，原缺图标结论属于修复前状态。 |
| 观看工具与沉浸 | `WatchToolsPanel.qml`、`ImmersiveEdge.qml`、`WorkspaceGrid.qml`、`RoomTile.qml`、`RoomSidebar.qml` | 同面板声音/布局子页、互斥辅助工具、T/Esc 与焦点恢复；保持观看网格实例。手动打开工具仍会临时覆盖视频，几何稳定不等同于永远零遮挡；最终交互与关闭回归已通过六目标 CTest。 |
| 主播导航 | `GuildNavigationPanel.qml`、`GuildMemberRow.qml`、`Main.qml` | 当前按官网目录平铺、性别/状态/搜索组合筛选及显式资料；悬浮数据卡运行链已移除，保留真实成员 ID 与控制器命令。源运行通过，九项最终整合状态见首节。 |
| 野榜与定级赛 | `pages/MaoziRankPage.qml`、`components/RadarChart.qml` | 标题、查询、五标签和来源状态条；56px 排行行、48px 定级行、横向滚动；当前补充头像关联、九维雷达及精确数值。源顺序和归档评分保留，未新增表头排序；最终集成仍进行。 |
| 赛季档案 | `pages/SeasonArchivePanel.qml`、`ArchiveDetailModel.js`、`SeasonMatchDetail.qml`、`FootballMatchDetail.qml` | 日期筛选、每页 10 场、可换行比赛行；当前增加结构化赛果/回合/足球首发/事件/MVP和完整原文。45场模型和源GUI通过，不代替最终生产回归。 |
| 管理面板 | `MonitoringStatusPanel.qml`、`WorkspacePresetsPanel.qml`、`DanmakuSettingsPanel.qml` | 窗口内约束、标题和关闭入口、可滚动内容、空状态；保留真实监控/弹幕统计与命令。预设返回空 ID 时保留输入，成功返回 ID 后清空；应用动作继续延后到 `Qt.callLater`。 |
| 有效弹窗 | `AddRoomDialog.qml`、`NotificationSettingsDialog.qml`、`CloseBehaviorDialog.qml`、`TeamManagerDialog.qml` | 统一表面、边界和标题，调整候选/选项/队伍行密度；保留搜索、即时通知保存、关闭选择、记住选择、取消默认焦点及队伍导入确认。 |
| 房间库与设置 | `RoomLibraryView.qml`、`SettingsPage.qml` | 适配共同视觉和外观选择；继续使用控制器的房间及应用操作。 |
| 未注册文件 | `GroupManagerDialog.qml` | 同步了源码外观，但它仍未列入当前 CMake QML 模块；不计作已交付的运行页面或入口。 |

五项品牌图片位于 `native/app/qml/assets/brand/`。CMake 已登记品牌资产及新增 shell 组件。曾发生的 raw QML/qmlcache 资源链接缺失经重新配置后恢复生成对象；当前证据未要求修改生产资源链接语义。

## 主题偏好接口

`AppController` 新增 `themeMode` 只读属性、`themeModeChanged` 信号和 `setThemeMode(QString)` 命令，只接受 `night`、`day`。默认夜间；构造时仅将有效保存值 `day` 恢复为白天。

偏好独立保存在 `ui/themeModeV1`，没有提升或改写 `DouyuMonitor/nativeWorkspaceV1` 的 schema。保存失败时保持原模式，并恢复同一设置文件的共享缓存候选值，避免后续同步误存失败选择。`Main.qml` 绑定主题，设置页调用同一命令。

三项聚焦测试覆盖独立保存/恢复、非法值和保存失败。`theme-green.txt` 记录 QtTest 5 passed / 0 failed（包含 init/cleanup），55ms。之后独立运行的 `app_controller_test` 为 79 passed / 0 failed / 1 个只读 live-preview skip，`native_workspace_store_test` 为 25/0；详见 `TEST_VALIDATION_LOG.md`。这两项不代表完整 CTest。

## 业务数据核对

只读解析实际 `native/app/resources/cstg_s1_archive.json`，并核对 `native/src/app/s1_archive.cpp` 与页面绑定。`S1Archive::parse` 检查元数据、必要区块、唯一 ID 和定级参与者引用后原样返回档案，不重新计算成绩或映射字段。

| 数据 | 当前档案事实 | 页面处理 |
| --- | --- | --- |
| 排行参与者 | 46 人，每人 9 个 `dimensions`，字段为 `name/value` | 详情展示力量、体力、智力、技巧、工投、财力、心态、颜值、直播能力；成绩取源字段。 |
| 排行成绩 | 源 `score` 含字符串，例如 `"15.51"` | 表格统一按一位小数展示；先验证类型和空白，再转为数值。空、无效、负哨兵显示 `—`，有效 0 显示 `0.0`。 |
| 定级参与者 | 36 人，每人 6 个 `stats`；其中 34 人含 `team`，2 人未提供该字段 | 队伍按源值显示，缺失使用占位；第一列标为“序号”，不把源数组顺序表述为重新计算的排名。 |
| 定级指标 | `label/session/value`，六场依次为 28午、28晚、29午、29晚、30午、30晚；敏捷/技巧/体能/智力/力量/稳定 | 表格按场次列展示，详情保留场次和指标名；总评来自 `overall`。现有固定 S1 场次键保持不变。 |
| 比赛 | 常规赛 31 场、决赛 4 场、足球 10 场 | 原顺序、现有筛选与 10 场分页；详情保留源文字。 |
| 足球专题 | `matches/stand/tree`；goals 10 条、assists 4 条、mvp 7 条 | 保留专题切换及三类球员榜单。 |

归档存在时，榜单及定级赛优先使用归档；缺少对应归档区块时才显示线上客户端数据。来源条区分 S1 档案与线上加载/失败状态；归档状态不冒充实时开播状态。监控说明保留未知状态不计在线/离线、错误数可能与在线数重叠的含义。

本轮没有重算评分、补造人数/队伍或添加无实现的排序行为。历史原文中若含旧交互说明，应作为来源文字审查，不能据此声称客户端提供该操作。

## 已发现并处理的业务运行问题

1. 首轮归档测试出现 `toFixed` 类型异常：档案分数是字符串。`MaoziRankPage.scoreText()` 现在接受数值或非空数字字符串，对无效/缺失值保留 `—`，用于排行、定级场次和总评。
2. 预设保存按钮原先无条件清空输入。现在仅在控制器返回非空预设 ID 后清空。独立夹具覆盖拒绝保存后原输入不丢失、成功保存后清空。该修复不改变控制器已创建内存预设但磁盘未保存时的 `workspaceUnsaved` 语义。
3. 预设删除/应用测试曾在列表委托尚未生成时读取行。交互测试负责人补了条件等待；当前没有证据要求修改生产列表的生成机制。
4. 定级赛返回总览后，测试在布局刷新前获取鼠标坐标。独立夹具记录首行 Y 从 256 变为 292，差值正好为 36px 表头；等待渲染后悬停和点击通过。生产 `HoverHandler` 保留，集成测试须等待表头/列表几何成立后采样。
5. 通知弹窗页脚采用 `Theme.managementSurface`，夜间为 `#161c1b`。旧测试期望控件色 `#1e2624` 与当前规范不符，测试负责人已按表面角色调整断言。

上述独立 QML 夹具直接加载本地生产 QML，不编译原生目标。证据位于 `native/out/verification/business-runtime/`：`red.txt` 为 2 passed / 3 failed，分别覆盖保存输入、字符串成绩和过早悬停；`hover-geometry.txt` 记录布局对照；`green.txt` 为 5 passed / 0 failed，503ms（含 init/cleanup），没有 QWARN。该夹具属于局部诊断，持久回归由主代理合入 `qml_interaction_test.cpp`。

## 服务载荷与构建前置条件

环境与完整命令见 [BASELINE_NATIVE.md](BASELINE_NATIVE.md)。本轮使用 Qt 6.12.0、MSVC、CMake/CTest 4.4.3、Release NMake，隔离构建目录为 `native/out/build/ui-redesign-baseline-qt612-nmake`。

旧服务包的 11 项 provenance 输入中仅名册哈希与当前工作区不一致。执行人员先逐文件备份 114 个文件，再通过现有正式脚本在隔离目录重打包，使用正式复制脚本推广至标准目录。新包源/目标 provenance、本地 ping/shutdown、安装器脚本回归和 12 项 provenance/依赖锁测试均有通过记录；没有修改服务源码、重写旧 manifest 或跳过校验。

旧包保留在 `native/out/service/streamget_service.pre-ui-redesign-20261030-001`，独立备份在 `native/out/service-backups/streamget_service-ui-redesign-20261030-001`。这些是忽略的本地产物，不加入源码提交。服务包检查不等于最新完整应用、安装器或真实直播验收。

## 六项修复前的验证记录（历史快照）

本节保留早期构建与测试原始结果。表中的待验描述是当时状态；之后六项结果见历史结论节，最新九项源码及未完成边界以本报告首节为准。

| 阶段 | 实际证据 | 能证明的范围 |
| --- | --- | --- |
| 改造前 Qt 基线 | 引擎 1/1、3.67s；交互 1/1、27.19s；视觉 1/1、26.71s；关闭 1/1、24.03s | 当前工具链可运行既有测试。全部属于改造前工作区，不能作为当前 UI 通过证据。 |
| 关闭基线权限对照 | 同 exe/Qt/Windows/16 路，沙箱服务启动失败；批准执行后通过 | 支持当时失败来自 QProcess 子进程权限。未删除渲染或关闭断言；日志仍有引擎销毁时 delegate 警告。 |
| 主题命令 | `theme-green.txt`：5 passed / 0 failed，55ms | 三个主题保存行为；完整 controller/persistence 回归待执行。 |
| 首批 shell 聚焦 | `shell-first-green.txt`：5 passed / 1 failed | 抽屉不改网格几何、匿名输入快捷键保护通过；键盘画内控件当时失败，不能将整批记为通过。 |
| 业务源码静态检查 | 10 个文件 `qmllint` 诊断 0 error、207 warning；两页 qmlformat 退出 0；既有 objectName 无丢失；Theme token 存在 | 只证明该次静态检查。unqualified 和部分父属性类型推断警告仍在，不能称为零警告 lint。 |
| 首轮完整交互 | `interaction-first-full.txt`：81 passed / 18 failed，132.488s | 暴露入口迁移、旧几何断言、异步列表、画内控件和业务页问题；部分失败已修复，当前完整结果待重跑。 |
| 业务聚焦修复 | `business-runtime/green.txt`：5 passed / 0 failed，503ms | 数字字符串/缺失、预设输入保留、布局就绪后定级悬停与点击。不是完整 S1、全部弹窗或全量 CTest。 |
| 修复后静态复核 | 两文件 fresh `qmllint` 在沙箱及批准环境均退出 `-1`，未生成诊断；`git diff --check` 退出 0 | 本次 lint 结果不确定，不以先前静态记录替代；直接加载生产 QML 的局部运行检查已有上述通过证据。 |
| 集成构建 | `build-final-pass2.txt`：原生应用、控件、交互、视觉烟测及相关目标构建成功 | 证明该次源码快照可构建；其后测试源码有修改，不能代表最新快照已构建。 |
| 原生控件 | `controls-final-pass2.txt`：12 passed / 0 failed | 共享控件的该次运行通过；不等于全部 QML 回归。 |
| 原生视觉烟测 | `visual-smoke-final-20261031.txt`：33 passed / 0 failed，54.943s | 包括 960×600 等视口约束和双主题三尺寸截图矩阵；有 QML 引擎销毁时尚在创建的 item 警告。 |
| 原生交互全量（历史） | `interaction-final-pass3.txt`：103 passed / 1 failed | 旧二进制在 `keepsControlsVisibleDuringQualityPopupInteraction()` 的 `controlsVisible` 断言失败。该失败由后续构建和选择性回归更新，不再作为当前状态。 |
| 核心 CTest（历史快照） | `integrated-core-ctest.txt`：关闭、controller、workspace store 通过，引擎烟测失败；4 项中 3 项通过 | 这是较早构建快照，不代表当前完整 CTest 结果。独立 controller/store 测试的后续运行见 `TEST_VALIDATION_LOG.md`。 |
| 后续重建（历史） | `build-interaction-final3.txt`：`moc` 的 libuv spawn 报 `operation not permitted`；审批通道当时返回 429 | 后续已经成功构建，并完成选择性回归；详见下方当前验证。审批失败仅是当时的环境记录，不是当前阻塞。 |
| 六项修复前选择性 QML 回归 | `TEST_VALIDATION_LOG.md`：指定引擎烟测、视觉烟测、视觉回归、交互及关闭回归 5/5 通过；质量弹窗定向测试 3/0 | 证明该次构建快照中所选测试通过；不替代本轮六项修复后的运行或完整仓库 CTest。 |
| 独立 controller / workspace store 回归 | `TEST_VALIDATION_LOG.md`：`app_controller_test` 79/0/1 skipped；`native_workspace_store_test` 25/0 | unrestricted runner 下现有 Windows 测试可执行文件的结果；后续受限运行遇到文件/子进程权限失败。不是完整 CTest。 |

早期原生截图矩阵为两主题 × 三尺寸（1280×800、1366×768、1920×1080）× 五页，共 30 张 PNG，当时仅核对数量与尺寸，尚未人工审查。之后的矩阵、最小窗口和编译模块截图检查见首节及 [VISUAL_QA_REPORT.md](VISUAL_QA_REPORT.md)，不再沿用此处的历史待查状态。网页 R3 证据仍不能替代 Qt 验收。

## Figma 与 Notion 状态

Figma 已建立真实可编辑文件，标识与节点清单见 [FIGMA_STATE.json](FIGMA_STATE.json)，实施和恢复步骤见 [FIGMA_DESIGN_REPORT.md](FIGMA_DESIGN_REPORT.md)。当前有三页、四个变量集合、92 个变量和五个文字样式；Primary Button 10 个变体、Secondary Button 10 个变体、TextField 8 个变体。Secondary 的禁用列截图存在空白疑点，尚未复核。

Figma Starter MCP 返回 `mcp_rate_limit_paywall`，后续调用已停止。StatusBadge 及其余组件尚未创建，Screens 仍为空，五项品牌资产尚未导入。完整 Figma 设计系统和页面交付受配额阻塞，不能标为完成，也没有通过其他通道绕过额度。

Notion 连接器已于 2026-10-31 22:46（Asia/Shanghai）恢复并实际调用。已读取知识库、建立关联项目入口、补录历史 001–070，并逐步追加后续步骤后回读；远端页面链接、编号与边界见 [NOTION_REMOTE_SYNC.md](NOTION_REMOTE_SYNC.md)。[NOTION_SYNC_QUEUE.md](NOTION_SYNC_QUEUE.md) 保留原时点“无连接器/待同步”等历史状态，不能据此判断当前连接仍不可用。文本同步不等于本地文件本体已上传，历史实时记录缺口也未被补录消除。

## 剩余验收事项

下列明确标记“六项历史”的勾选只代表上一轮检查，不是九项源码最终通过结论。

- [ ] 完成九项最新源码的最终集成构建、永久回归、视觉基线复测和生产渲染自测；以最终日志核对可执行程序。

- [x] 六项历史：生产程序已构建；编译模块主题5/0、控件13/0、视觉烟测35/0、视觉回归7/0，日志及边界见六项历史节。
- [x] 六项历史：修复源码已落盘；编译模块昼夜图、最小窗口与榜单列对齐等指定细节已检查，三张预期基线经备份后局部更新并复测。
- [x] 六项历史：最终六项目标 CTest 6/6 通过，136.84 秒；当时完整交互单独运行 109/0。日志见六项历史节。
- [x] 六项历史：独立运行 `app_controller_test`（79 passed / 0 failed / 1 个只读 live-preview skip）与 `native_workspace_store_test`（25/0）；结果见 `TEST_VALIDATION_LOG.md`。
- [ ] 执行完整 CTest 并覆盖本次全部生命周期变动；此前核心 CTest 3/4 属较早快照，不替代当前完整回归。
- [~] 六项历史：离线矩阵汇总图与指定全尺寸细节已经审查；仍不代表真实直播内容、所有设备/DPI及全部用户流程验收完成。
- [x] 六项历史：当时生产程序 `--self-test` 退出0，日志 `six-issue-app-self-test.txt`；若交付安装包，正式载荷及安装器仍需另行验证。
- [ ] 配额恢复后补齐 Figma 组件/页面/品牌资产，复核 Secondary Disabled，并取得实际截图与节点回读。
- [x] Notion 已恢复，历史补录及已记录后续步骤完成远端写入和回读；范围见 [NOTION_REMOTE_SYNC.md](NOTION_REMOTE_SYNC.md)。
- [x] 九项最终记录已保存回读；2026-11-01四项后续反馈继续按步骤记录。

真实斗鱼长时多路播放、弹幕稳定性、不同 DPI/设备的性能与内存验收尚未执行。当前报告不将本地假服务、单位测试或静态截图等同于这些目标环境结果。

## 上一轮九项交付边界（当时记录）

当前EXE和验证证据以TEST_VALIDATION_LOG.md最新节为准。静态头像回退覆盖34/46排行、26/36定级赛，其他仍需线上数据，不能称全部头像真实网络验收。图8修复的是白天画内控件语义，没有修改mpv视频色彩。Notion已逐步写入回读，最终编号及链接见NOTION_REMOTE_SYNC.md；完整Figma、真实直播长时、DPI/设备和安装包仍是独立未完事项。
