# CSTG 特勤终端：当前 UI 源码审计

审计日期：2026-10-29。审计基线：`main`，HEAD `968557b`，以当前工作区源码为准。本文用于 UI 重设计前的功能与界面盘点，不是已批准的产品方案。

本次检查了 `native/app/qml` 下全部 **35 个 QML 文件、1 个弹幕调度 JS**，以及 QML 模块注册、AppController 和相关模型的公开接口。未启动应用、未执行构建或 QML 测试、未初始化 Serena、未操作 Figma。本文中的确定结论来自静态源码；布局表现和交互风险没有表述为运行复现结果。

用户要求先提供可在浏览器查看的网页候选，获得选定/批准后，再进入 Figma 和生产 QML 修改。当前阶段应保护已有未提交修改。**Notion 尚未同步**；本文只保存在本地仓库，不能视为已有 Notion 页面或同步记录。

## 1. 当前界面结构与完整文件映射

### 1.1 应用壳、导航和监控工作区

| 表面 / 文件 | 当前行为与数据 | 重设计需要保留的内容 |
|---|---|---|
| [Main.qml:9](D:/DouyuMonitor/native/app/qml/Main.qml:9) | 无边框 ApplicationWindow；默认 1280×720，最小 960×600；主题 Palette；标题“CSTG特勤终端”；`currentView` 为 `monitoring`、`settings`、`maoziRank`。 | 三个主视图、房间侧栏/公会导航互斥、工作区保存失败提示、所有弹层入口、快捷键、关闭/最小化/全屏生命周期。 |
| [AppHeader.qml:37](D:/DouyuMonitor/native/app/qml/components/AppHeader.qml:37) | 侧栏、公会导航、S1 野榜；弹幕、监控、预设、布局、音频、全屏工具；品牌和窗口拖动区。 | 图标操作及 Accessible 名称/提示；拖动窗口与双击最大化；`auto`、`primary`、`primary-two` 布局；双主画面至少4房间的资格和说明。 |
| [WindowControls.qml:14](D:/DouyuMonitor/native/app/qml/components/WindowControls.qml:14) | 最小化、最大化/恢复、关闭；图标随窗口状态变化；关闭根据偏好询问或调用控制器。 | 对应窗口命令、状态图标、键盘焦点、提示和可访问名称。 |
| [RoomSidebar.qml:27](D:/DouyuMonitor/native/app/qml/components/RoomSidebar.qml:27) | 房间号快加和完整搜索入口；当前/历史/收藏三标签；当前列表展示主播/标题/直播状态。 | 收藏、上移/下移、音频焦点或逐房静音、移除请求；头像回退；设置入口；房间列表顺序和主画面标记。 |
| [RoomLibraryView.qml:24](D:/DouyuMonitor/native/app/qml/components/RoomLibraryView.qml:24) | 基于 `libraryRooms`；收藏过滤或 `lastOpenedAtMs > 0` 历史过滤；按来源顺序显示。 | 当前已打开状态、重新添加未打开房间、删除未打开历史项、历史/收藏空状态。 |
| [GuildNavigationPanel.qml:73](D:/DouyuMonitor/native/app/qml/components/GuildNavigationPanel.qml:73) | 团长、配置队伍、队长/队员/其他、未分组；姓名/房间查询、拼音排序；增量同步扁平行。 | 队伍与角色层级、滚动/悬浮稳定性、每60秒榜单变化和直播状态检查、容量限制、确认未知房间号、快捷加入。 |
| [GuildMemberRow.qml:9](D:/DouyuMonitor/native/app/qml/components/GuildMemberRow.qml:9) | 头像/首字回退、主播名、房间号或“待确认”、直播状态、已添加状态。 | 未确认房间号输入和 Enter 提交、加入按钮资格、悬浮事件、未知/在线/离线/已添加状态。 |
| [GuildRankHoverCard.qml:5](D:/DouyuMonitor/native/app/qml/components/GuildRankHoverCard.qml:5) | 非模态 Popup；姓名/角色、雷达、综合评分、定级赛总评、游乐值或“暂无野榜数据”。 | Main 中450ms悬浮延迟、窗口边界定位、页面切换/失活/隐藏后的清理；有榜单与无榜单数据两种呈现。 |
| [WorkspaceGrid.qml:22](D:/DouyuMonitor/native/app/qml/components/WorkspaceGrid.qml:22) | 手工计算自动网格/主画面/双主画面几何；随房间数量分档；Repeater 创建 RoomTile。 | 主/次主绑定、自动重排、空工作区说明、实际 workspace 容量；不能从旧16路预览文案推断实际容量。 |
| [PrimaryRoomDivider.qml:12](D:/DouyuMonitor/native/app/qml/components/PrimaryRoomDivider.qml:12) | 比例拖动和键盘操作，吸附0.5/0.6/0.67，调用 `setPrimaryRoomRatio`。 | **已注册但当前无实例**，不应在候选中标作现有可用功能；若新增入口属于行为决策。 |
| [RoomTile.qml:14](D:/DouyuMonitor/native/app/qml/components/RoomTile.qml:14) | 舒适/紧凑/密集三档；资料、直播/播放/弹幕状态；播放器与覆盖层；悬停控制条，2.2秒后自动隐藏。 | 渲染暂停占位；主/次主标记；头像/资料回退；音量、音频焦点/静音、弹幕开关/重试、可用画质；刷新资料/重新检查源/设主次主/移除；画面双击切换整个窗口全屏。 |
| [WorkspaceStatusBar.qml:8](D:/DouyuMonitor/native/app/qml/components/WorkspaceStatusBar.qml:8) | 在线/离线数、弹幕连接数、声音焦点/多声道/全局静音、检查状态。 | 保留有意义的状态信息；弹幕数和检查状态当前为占位绑定，见问题清单。 |
| [ToastViewport.qml:13](D:/DouyuMonitor/native/app/qml/components/ToastViewport.qml:13) | 仅展示 error/warning/action；定时或手动关闭；固定380px宽、文字省略。 | 消息等级、超时、可关闭性；工作区保存失败的独立重试提示位于 [Main.qml:457](D:/DouyuMonitor/native/app/qml/Main.qml:457)。 |

Main 中左侧栏的可见性不依赖 `currentView`，设置和榜单页仍从左侧栏右边界开始布局，见 [Main.qml:335](D:/DouyuMonitor/native/app/qml/Main.qml:335) 和 [Main.qml:430](D:/DouyuMonitor/native/app/qml/Main.qml:430)。候选网页必须展示带侧栏时的页面可用宽度。

### 1.2 设置和赛季页面

| 文件 / 页面 | 当前内容 | 必须保留的交互与状态 |
|---|---|---|
| [SettingsPage.qml:85](D:/DouyuMonitor/native/app/qml/pages/SettingsPage.qml:85) | 当前只有四组：关闭按钮行为、队伍管理、更新、活动角色；滚动容器；内容宽900px阈值切换单/双列，最大1180px。 | 返回监控；立即生效的关闭偏好、保存失败重试；打开队伍对话框；更新检查/发布页；活动映射编辑和明确保存。 |
| [MaoziRankPage.qml:13](D:/DouyuMonitor/native/app/qml/pages/MaoziRankPage.qml:13) | 优先 `controller.s1Archive` 的野榜和定级数据，无档案才使用 live client；五标签为野榜、常规赛、总决赛、足球赛、定级赛。 | 搜索、标签切换、外部详情链接；实时回退时的直播筛选/刷新；档案模式隐藏实时筛选、禁用刷新；搜索重置参与者选择。 |
| [SeasonArchivePanel.qml:8](D:/DouyuMonitor/native/app/qml/pages/SeasonArchivePanel.qml:8) | 常规赛、总决赛、足球赛共用；查询、每页10场、详情、总览；常规赛日期过滤；足球比赛树、进球/助攻/MVP榜。 | 上下页、返回赛程、场次选择、日期/搜索改变后的选择和页码；只读可选择详情；各足球视图与榜单。 |

野榜表格展示排名、主播/头像/注释、房间号、队伍、评级、综合分、人数、状态，见 [MaoziRankPage.qml:364](D:/DouyuMonitor/native/app/qml/pages/MaoziRankPage.qml:364)。档案行可进入参与者维度详情并返回，见 [MaoziRankPage.qml:682](D:/DouyuMonitor/native/app/qml/pages/MaoziRankPage.qml:682)。档案模式人数为“—”、状态为“赛季成绩”，不应伪装为实时投票或直播状态。

定级赛含六个场次成绩及总评、参与者详情，以及总览/得分统计/赛程统计，见 [MaoziRankPage.qml:721](D:/DouyuMonitor/native/app/qml/pages/MaoziRankPage.qml:721)、[MaoziRankPage.qml:906](D:/DouyuMonitor/native/app/qml/pages/MaoziRankPage.qml:906)。三个赛程面板的实例绑定见 [MaoziRankPage.qml:992](D:/DouyuMonitor/native/app/qml/pages/MaoziRankPage.qml:992)。

#### 设置的实际四组

| 分组 | 当前操作 | 状态 / 边界 | 源码 |
|---|---|---|---|
| 关闭按钮行为 | 每次询问、进入后台、完全退出；选项变更立即调用控制器。 | “每次询问”不保存默认选择；会话生效但未保存时显示告警和“重试保存”。 | [SettingsPage.qml:102](D:/DouyuMonitor/native/app/qml/pages/SettingsPage.qml:102) |
| 队伍管理 | “管理队伍”打开 TeamManagerDialog。 | 队伍与现有直播间及旧分组是不同概念。 | [SettingsPage.qml:165](D:/DouyuMonitor/native/app/qml/pages/SettingsPage.qml:165) |
| 更新 | 检查更新；有更新时打开发布页。 | 检查中禁用；展示 `updateMessage`；发布按钮需有效发布地址。 | [SettingsPage.qml:181](D:/DouyuMonitor/native/app/qml/pages/SettingsPage.qml:181) |
| 活动角色 | 活动名称；角色团长/队长/队员；无回退队伍/红/黑/紫/蓝；从公会名单添加主播；保存角色配置。 | 只接受已确认房间号，忽略重复房间；显式保存、错误反馈；当前没有删除映射行入口。 | [SettingsPage.qml:203](D:/DouyuMonitor/native/app/qml/pages/SettingsPage.qml:203)、[SettingsPage.qml:239](D:/DouyuMonitor/native/app/qml/pages/SettingsPage.qml:239) |

通知、弹幕和工作区预设当前位于独立弹层，不属于 Settings 现有四组。候选可提出信息架构调整，但必须保留全部设置和当前保存语义，并明确该调整属于方案。

### 1.3 全部有效对话框、面板和浮层

[Main.qml:501](D:/DouyuMonitor/native/app/qml/Main.qml:501) 实例化三个面板和四个主对话框；`GroupManagerDialog` 没有有效调用。

| 浮层 | 当前入口 / 类型 | 内容与必须保留的行为 |
|---|---|---|
| [AddRoomDialog.qml:9](D:/DouyuMonitor/native/app/qml/dialogs/AddRoomDialog.qml:9) | 房间侧栏加号、添加快捷键；模态，约500px宽。 | 房间号/链接/主播名，最长200字符；idle/searching/empty/error；候选头像、名称、标题、已知直播状态、房间号/分类；添加成功才关闭；关闭入口和初始提示。 |
| [NotificationSettingsDialog.qml:12](D:/DouyuMonitor/native/app/qml/dialogs/NotificationSettingsDialog.qml:12) | 监控面板“通知设置”；模态，360px。 | 总开关、开播、下播、播放异常、播放恢复、收藏标题变化，共六项；即时保存；关闭总开关禁用子项；后端不可用状态；打开聚焦总开关。 |
| [CloseBehaviorDialog.qml:11](D:/DouyuMonitor/native/app/qml/dialogs/CloseBehaviorDialog.qml:11) | 关闭窗口且偏好为ask；模态，360px。 | 取消、进入后台、完全退出、记住选择；打开聚焦取消；未记住选择时保持以后询问；后台与完全退出走不同生命周期调用。 |
| [TeamManagerDialog.qml:9](D:/DouyuMonitor/native/app/qml/dialogs/TeamManagerDialog.qml:9) | 设置“管理队伍”；模态，620px。 | 最多20队文案；队伍新建/重命名/删除/上移/下移；选择队伍和成员分配/移除；4秒反馈；野榜导入idle/loading/error/preview；新增/修改/不变队伍、匹配/保留成员计数；未匹配/冲突/未分配问题；`canConfirm`资格；关闭取消待确认导入。 |
| [DanmakuSettingsPanel.qml:9](D:/DouyuMonitor/native/app/qml/panels/DanmakuSettingsPanel.qml:9) | 顶栏/快捷键；右上Popup，380px。 | 全局/逐房开关；显示/治理/统计三页。完整设置见下一表。 |
| [MonitoringStatusPanel.qml:9](D:/DouyuMonitor/native/app/qml/panels/MonitoringStatusPanel.qml:9) | 顶栏/快捷键；右侧非模态Drawer，320px。 | 在线/离线/异常计数、通知enabled/disabled/unavailable、通知设置入口、关闭。 |
| [WorkspacePresetsPanel.qml:9](D:/DouyuMonitor/native/app/qml/panels/WorkspacePresetsPanel.qml:9) | 顶栏/快捷键；右上Popup，360×380。 | 1–30字符名称保存当前工作区；预设应用/删除；应用通过 `Qt.callLater` 执行，保留其时序。 |
| [AppHeader.qml:275](D:/DouyuMonitor/native/app/qml/components/AppHeader.qml:275) | 音频按钮；自定义Rectangle浮层。 | 全局静音、单路/多路模式、关闭；当前不是标准Popup的自动关闭/焦点语义。 |
| [AppHeader.qml:382](D:/DouyuMonitor/native/app/qml/components/AppHeader.qml:382) | 布局按钮；Menu。 | 自动/主画面/双主画面；双主至少4房间及禁用提示。 |
| [RoomTile.qml:375](D:/DouyuMonitor/native/app/qml/components/RoomTile.qml:375) | 每房更多按钮；自定义Rectangle。 | 播放状态、刷新资料、设次主（双主模式）、重新检查源、设主画面、请求移除。 |
| [RoomTile.qml:659](D:/DouyuMonitor/native/app/qml/components/RoomTile.qml:659) | 每房画质ComboBox；自定义Popup。 | 真实可用画质和qualityRate；选择状态；与画面双击防误触保护联动。 |
| [GuildRankHoverCard.qml:5](D:/DouyuMonitor/native/app/qml/components/GuildRankHoverCard.qml:5) | 公会成员悬浮450ms；非模态、无焦点、NoAutoClose。 | 由Main控制悬浮与关闭生命周期；有/无榜单数据。 |
| [ToastViewport.qml:13](D:/DouyuMonitor/native/app/qml/components/ToastViewport.qml:13) | workspace消息；非模态提示层。 | 等级、超时、手动关闭，不是确认对话框。 |
| 标准下拉与提示 | ConsoleComboBox、ConsoleToolTip及房间画质下拉。 | 弹幕、活动角色、队伍和筛选的展开、选中、禁用、键盘焦点状态；网页候选不能只展示闭合态。 |

#### 弹幕设置完整内容

| 设置页 | 参数和操作 |
|---|---|
| 显示 | 时长4–15秒；字号14–36px；透明度0.3–1；区域全屏/上/下；密度海量/正常/精简；雅黑/黑体；原生/增强渲染；恢复七项默认值。 |
| 治理 | 全局或指定房间覆盖；过滤/限流；逗号分隔关键词，在编辑结束时提交；重复窗口1–10秒；峰值保护；清除逐房覆盖。 |
| 统计 | 逐房治理等级、近期/峰值速率、过滤、重复、限流、队列溢出、上游丢弃；清空统计；统计信号刷新当前房间。 |

来源：[DanmakuSettingsPanel.qml:9](D:/DouyuMonitor/native/app/qml/panels/DanmakuSettingsPanel.qml:9)。全局和逐房开关是独立层级；断开、失败、平台阻止、等待等房间状态不能简化为一个复选框。

### 1.4 弹幕显示、主题和公共控件

| 文件 | 当前职责 / 状态 |
|---|---|
| [DanmakuOverlay.qml:16](D:/DouyuMonitor/native/app/qml/components/DanmakuOverlay.qml:16) | 读取显示设置；密度节流、区域、字体/字号/透明度/时长、原生/增强；顶部/底部安全边距；最多64个活跃项；隐藏、暂停和销毁时清理；尺寸/显示参数变化重排。 |
| [DanmakuLine.qml:28](D:/DouyuMonitor/native/app/qml/components/DanmakuLine.qml:28) | 纯文本、描边/可选阴影、线性移动、结束后释放；不解析弹幕富文本。 |
| [DanmakuLaneScheduler.js:9](D:/DouyuMonitor/native/app/qml/components/DanmakuLaneScheduler.js:9) | 1.6倍字号的行高、区域/密度轨道分配、头部间距和追赶碰撞判断；属于行为逻辑，不能随视觉重排删除。 |
| [Theme.qml:6](D:/DouyuMonitor/native/app/qml/Theme.qml:6) | 深色表面、橙色强调、语义色、尺寸/间距/字号/圆角、统一Palette、减弱动效。 |
| [ControlBackground.qml:4](D:/DouyuMonitor/native/app/qml/controls/ControlBackground.qml:4) | 普通/主操作/破坏性/选中/禁用/悬浮/按压/键盘焦点样式。 |
| [ConsoleButton.qml:5](D:/DouyuMonitor/native/app/qml/controls/ConsoleButton.qml:5) | Basic.Button封装，32px高度下限、100px宽下限、图标和Palette、主操作与破坏性。 |
| [ConsoleToolButton.qml:5](D:/DouyuMonitor/native/app/qml/controls/ConsoleToolButton.qml:5) | 图标/工具按钮样式，悬浮或键盘焦点提示，Accessible名称回退。 |
| [ConsoleTextField.qml:5](D:/DouyuMonitor/native/app/qml/controls/ConsoleTextField.qml:5) | 文本、占位、禁用、悬浮边框、焦点边框。 |
| [ConsoleComboBox.qml:5](D:/DouyuMonitor/native/app/qml/controls/ConsoleComboBox.qml:5) | 下拉、选中/焦点/禁用、Popup底板和进入动画。 |
| [ConsoleCheckBox.qml:5](D:/DouyuMonitor/native/app/qml/controls/ConsoleCheckBox.qml:5) | 勾选、半选、未选、禁用、悬浮、按压、焦点。 |
| [ConsoleRadioButton.qml:5](D:/DouyuMonitor/native/app/qml/controls/ConsoleRadioButton.qml:5) | 单选、禁用、悬浮、按压、焦点。 |
| [ConsoleToolTip.qml:5](D:/DouyuMonitor/native/app/qml/controls/ConsoleToolTip.qml:5) | 提示文字换行、边框/底色、进出动画。 |

以上映射覆盖35个QML：根目录2、components14、pages3、panels3、dialogs5、controls8。第五个对话框为当前无入口的GroupManagerDialog，详见第4节；另有1个JS调度文件和qmldir。

## 2. 源码可确认的问题与需要验证的风险

### 2.1 静态结构和状态问题

“高/中/低”表示对重设计完整性或当前可用性的优先级，不是已测量的用户影响等级。

| 优先级 | 源码确认事实 | 证据与影响 |
|---|---|---|
| 高 | 公会房间号确认输入超出父行预留高度。 | 父delegate成员行固定46px，[GuildNavigationPanel.qml:373](D:/DouyuMonitor/native/app/qml/components/GuildNavigationPanel.qml:373)；子行确认态84px，输入在y48、高30，[GuildMemberRow.qml:23](D:/DouyuMonitor/native/app/qml/components/GuildMemberRow.qml:23)、[GuildMemberRow.qml:195](D:/DouyuMonitor/native/app/qml/components/GuildMemberRow.qml:195)。父布局不随确认态扩高，会占用后续行区域。 |
| 高 | 定级赛统计展开后，切换页签不复位且缺少activeTab约束。 | 按钮直接设 `placementSummary.visible = true`，[MaoziRankPage.qml:906](D:/DouyuMonitor/native/app/qml/pages/MaoziRankPage.qml:906)；ScrollView仅初始化 `visible:false`，[MaoziRankPage.qml:930](D:/DouyuMonitor/native/app/qml/pages/MaoziRankPage.qml:930)。它会继续参与其他页签的Column排布。 |
| 高 | 野榜固定列宽超出最小窗口带侧栏时的空间。 | 列宽总计730px，另有24px横向内边距，[MaoziRankPage.qml:42](D:/DouyuMonitor/native/app/qml/pages/MaoziRankPage.qml:42)。960px窗口扣268px侧栏和40px页面边距后约652px；ListView裁切且只有垂直滚动，[MaoziRankPage.qml:475](D:/DouyuMonitor/native/app/qml/pages/MaoziRankPage.qml:475)。 |
| 高 | 双主模式的房间更多菜单底板高度不足。 | 固定156px，[RoomTile.qml:381](D:/DouyuMonitor/native/app/qml/components/RoomTile.qml:381)；双主时六行×27px，加12px内边距和五个2px间距，需求184px；普通模式五行约155px。 |
| 中 | 状态栏弹幕数与检查状态没有真实绑定。 | [Main.qml:417](D:/DouyuMonitor/native/app/qml/Main.qml:417) 将danmakuCount固定0；[WorkspaceStatusBar.qml:12](D:/DouyuMonitor/native/app/qml/components/WorkspaceStatusBar.qml:12) silentChecking默认true，Main未覆盖。不能把它们作为实时状态设计。 |
| 中 | 侧栏元数据空间小、字号小。 | 268px栏内列表左右共16px；常驻139px五操作栏，[RoomSidebar.qml:300](D:/DouyuMonitor/native/app/qml/components/RoomSidebar.qml:300)，再扣头像和间隙后文本区域很窄；主播11px、标题/状态9px，[RoomSidebar.qml:245](D:/DouyuMonitor/native/app/qml/components/RoomSidebar.qml:245)。实际截断程度尚未截图验证。 |
| 中 | 密集视频块缺少底部关键操作入口。 | 宽<180px为dense，[RoomTile.qml:46](D:/DouyuMonitor/native/app/qml/components/RoomTile.qml:46)；底条 `visible: !dense`，[RoomTile.qml:448](D:/DouyuMonitor/native/app/qml/components/RoomTile.qml:448)；更多菜单不含音频/弹幕/画质操作。新方案需给出等价入口。 |
| 中 | 快捷键的输入焦点保护只覆盖五个名称。 | [Main.qml:171](D:/DouyuMonitor/native/app/qml/Main.qml:171) 仅检查roomSearchInput、presetNameInput、groupNameInput、teamNameInput、notificationTitleInput；不覆盖公会搜索/确认、侧栏快加、榜单搜索、活动名称等。源码可确认保护不完整，实际按键结果需运行验证。 |
| 中 | 更多菜单把所有非error播放态写成“正常”。 | [RoomTile.qml:394](D:/DouyuMonitor/native/app/qml/components/RoomTile.qml:394) 只有error分支；idle/loading/offline没有独立说明。 |
| 中 | 活动角色映射缺少删除行入口。 | [SettingsPage.qml:232](D:/DouyuMonitor/native/app/qml/pages/SettingsPage.qml:232) 有逐行角色/队伍编辑；[SettingsPage.qml:265](D:/DouyuMonitor/native/app/qml/pages/SettingsPage.qml:265) 有添加和保存；无移除操作。 |
| 中 | 收藏视图不能直接取消收藏。 | [RoomLibraryView.qml:169](D:/DouyuMonitor/native/app/qml/components/RoomLibraryView.qml:169) 只提供加入和历史删除；取消收藏需先打开房间，再在当前列表操作。 |
| 中 | 快速添加和保存预设无条件清空输入。 | [RoomSidebar.qml:27](D:/DouyuMonitor/native/app/qml/components/RoomSidebar.qml:27)、[WorkspacePresetsPanel.qml:77](D:/DouyuMonitor/native/app/qml/panels/WorkspacePresetsPanel.qml:77)，未按返回结果保留输入。失败恢复状态应在候选中覆盖。 |
| 低 | 头像错误回退不一致。 | 当前房间、公会行、搜索候选检查Image.Error；历史/收藏仅检查地址存在，[RoomLibraryView.qml:95](D:/DouyuMonitor/native/app/qml/components/RoomLibraryView.qml:95)；悬浮卡没有相同首字回退，[GuildRankHoverCard.qml:36](D:/DouyuMonitor/native/app/qml/components/GuildRankHoverCard.qml:36)。 |
| 低 | 主题封装和局部手写样式并存。 | Theme及8控件统一了token，但Toast、雷达、部分文字保留硬编码色和9–10px字号。例：[ToastViewport.qml:13](D:/DouyuMonitor/native/app/qml/components/ToastViewport.qml:13)、[GuildRankHoverCard.qml:88](D:/DouyuMonitor/native/app/qml/components/GuildRankHoverCard.qml:88)。属于一致性审查项，未做屏幕对比度实测。 |
| 低 | 无控制器预览仍存在16路旧说明。 | [Main.qml:215](D:/DouyuMonitor/native/app/qml/Main.qml:215) 预览数据和 [WorkspaceGrid.qml:229](D:/DouyuMonitor/native/app/qml/components/WorkspaceGrid.qml:229) fallback有16路表述。真实容量应使用workspace和布局规则。 |

### 2.2 未复现风险和设计判断

以下内容尚不能当作已复现缺陷：

- 自动隐藏控制条主要以opacity隐藏，见 [RoomTile.qml:457](D:/DouyuMonitor/native/app/qml/components/RoomTile.qml:457)。需运行验证隐藏按钮是否仍命中、Tab焦点是否进入不可见控件。
- TeamManager成员移除按钮资格与“从当前所选队伍移除”的调用可能不一致；处理函数明确使用selectedTeamId，见 [TeamManagerDialog.qml:134](D:/DouyuMonitor/native/app/qml/dialogs/TeamManagerDialog.qml:134)。需结合按钮资格和控制器返回契约验证。
- 头部密集图标依赖工具提示，发现性可能不足；音频自定义Rectangle的点击外关闭、Escape、焦点恢复和辅助技术语义需要运行检查。不能仅凭源码声称已重叠或无法键盘操作。
- Settings的JSON.parse未捕获异常，见 [SettingsPage.qml:20](D:/DouyuMonitor/native/app/qml/pages/SettingsPage.qml:20)。控制器是否始终提供合法、规范化JSON尚需契约验证；不能直接称为可触发崩溃。
- 定级赛/赛季详情的滚动、最小高度和不同缩放比例，以及320px/180px临界视频块，均需浏览器候选和后续原生运行验证。
- 当前榜单页面优先静态档案，但进入页面仍调用rank refresh，见 [Main.qml:107](D:/DouyuMonitor/native/app/qml/Main.qml:107)。是否保留这项后台同步属于行为核实事项，不能只根据“刷新按钮禁用”删除调用。

## 3. 必须保留的接口、数据和生命周期

### 3.1 QML与领域层的公开边界

| 接口 | 关键内容 | 源码 |
|---|---|---|
| AppController属性 | rooms、workspace、monitoring、danmaku、libraryRooms、guildRoster、notificationPreferences；后台/最小化/关闭；搜索；更新；榜单同步；s1Archive；队伍导入预览；活动映射；工作区和关闭偏好保存失败。 | [app_controller.h:37](D:/DouyuMonitor/native/src/ui/app_controller.h:37) |
| AppController命令 | 房间查找/添加/移除/排序/收藏；主次主；音频模式/焦点/静音/音量；画质/重试/刷新；弹幕；旧group与新team两套命令；布局/侧栏/导航；预设/通知；播放器绑定；窗口和退出生命周期。 | [app_controller.h:118](D:/DouyuMonitor/native/src/ui/app_controller.h:118) |
| RoomListModel | 房间资料；直播/播放态；主次主、收藏、音频；请求/实际画质与qualityRate、可用画质；弹幕开关/状态/错误/各统计；renderEnabled。 | [room_list_model.h:32](D:/DouyuMonitor/native/src/ui/room_list_model.h:32) |
| WorkspaceModel | 侧栏/导航、layoutId/layoutMode、主画面比例、主次主ID；音频焦点/模式/全局静音；弹幕、容量；teams/groups/presets；消息内容/等级/超时。 | [workspace_model.h:14](D:/DouyuMonitor/native/src/ui/workspace_model.h:14) |
| MonitoringModel | online/offline/error计数、notificationStatus、recoveryStatus。 | [monitoring_model.h:7](D:/DouyuMonitor/native/src/ui/monitoring_model.h:7) |
| DanmakuController | 全局开关；显示/治理设置；展示暂停；逐房治理覆盖；状态/统计、重试/清理、消息领取。 | [danmaku_controller.h:11](D:/DouyuMonitor/native/src/danmaku/danmaku_controller.h:11) |
| MaoziRankClient | 野榜/定级/游乐值；loading/error、投票数、更新时间/状态；同步待处理/错误；刷新/检查变化/按房间或姓名查询。 | [maozi_rank_client.h:16](D:/DouyuMonitor/native/src/app/maozi_rank_client.h:16) |

QML应继续通过AppController及公开模型访问领域层，不复制业务状态、不绕过MultiRoomCoordinator/RoomSession。布局视觉修改必须保留房间身份、主次主、音频和弹幕资格之间的同步。

### 3.2 播放器和弹幕生命周期

- RoomTile在播放器创建后attach，绑定变化时重绑，销毁时detach；见 [RoomTile.qml:105](D:/DouyuMonitor/native/app/qml/components/RoomTile.qml:105)、[RoomTile.qml:141](D:/DouyuMonitor/native/app/qml/components/RoomTile.qml:141)、[RoomTile.qml:783](D:/DouyuMonitor/native/app/qml/components/RoomTile.qml:783)。视觉调整不能改变这些调用的对象生命周期。
- `renderEnabled`决定是否创建/启用渲染项；页面、最小化和后台状态可能影响展示。新方案必须区分播放会话继续与视频展示暂停。
- 弹幕Overlay隐藏、暂停或销毁时停止/清理活跃项，见 [DanmakuOverlay.qml:188](D:/DouyuMonitor/native/app/qml/components/DanmakuOverlay.qml:188)。不能把界面隐藏等同于停止领域弹幕会话。
- 每房更多菜单/画质Popup与双击画面防误触保护联动；画面双击当前操作是整个窗口全屏，不是单房放大。
- 移除使用 `requestRemoveRoom` 的现有请求/消息机制；候选不能擅自改成直接销毁或引入未经确认的新确认流程。

### 3.3 窗口和快捷键

关闭、后台托管、最小化恢复由 [Main.qml:138](D:/DouyuMonitor/native/app/qml/Main.qml:138) 和控制器处理；退出只有quitRequested时接受。关闭偏好保存、会话生效与后台托管不是同一状态。

| 快捷键 | 当前行为 |
|---|---|
| Ctrl+Shift+A | 添加房间 |
| Ctrl+Shift+D | 弹幕设置 |
| Ctrl+Shift+M | 监控状态 |
| Ctrl+Shift+W | 工作区预设 |
| Ctrl+Shift+S | 房间侧栏 |
| Ctrl+Shift+R | 刷新主房间 |
| F11 | 切换窗口全屏 |
| Escape | 退出窗口全屏 |

来源：[Main.qml:542](D:/DouyuMonitor/native/app/qml/Main.qml:542)。输入焦点保护目前基于有限objectName，相关风险见第2节。

### 3.4 兼容、安全和容量

- 真实容量存在普通添加、双主布局、旧分组持久化和弹幕会话等不同约束。候选以控制器暴露的maxRooms和当前布局为准，不把9/10/16路任意统一为视觉文案。
- 不改变现有QSettings工作区键/schema及字段语义；关闭偏好、队伍、活动映射、预设和房间状态应保持原有持久化边界。
- 不把播放URL、查询参数、Cookie、Token、签名、原始弹幕、服务诊断写入候选数据、日志、截图说明或文档。网页候选使用合成资料。
- 错误、超时、取消、重试和通知状态应保留固定业务语义；不直接展示底层异常或服务诊断。

## 4. 注册情况、未使用界面与现有修改保护

### 4.1 注册与未使用内容

- [CMakeLists.txt:156](D:/DouyuMonitor/native/CMakeLists.txt:156) 定义8个公共控件；[CMakeLists.txt:167](D:/DouyuMonitor/native/CMakeLists.txt:167) 的DouyuMonitor主模块注册34个QML，另注册弹幕JS；相应测试资源列表也包含这些界面。
- 源码目录有35个QML，唯一未进入主模块列表的是 [GroupManagerDialog.qml:5](D:/DouyuMonitor/native/app/qml/dialogs/GroupManagerDialog.qml:5)。`rg`未发现当前QML调用；Main只有四个有效主对话框。
- GroupManagerDialog包含旧房间分组的新建/重命名/删除/激活、成员排序/移除、按房间号或资料库加入。它不是当前有效弹窗；底层group API仍存在，不能与team管理混为一谈，也不能因UI未用而删除领域能力。
- PrimaryRoomDivider在CMake注册，但未发现QML实例。拖拽比例与键盘比例功能当前没有有效入口。
- MaoziRankPage的selectedMatch、seasonPage、seasonDate、filteredMatches、pageMatches、filterMatches保留了旧赛程状态；真正三个赛程面板使用SeasonArchivePanel内部状态。当前候选无需复制无效双份状态。
- [qmldir](D:/DouyuMonitor/native/app/qml/qmldir)声明Theme singleton；不能把该文件内容当作完整运行时模块注册清单。

### 4.2 当前工作区与HEAD的差异

本审计基于脏工作区，不是HEAD旧界面。初查与文档写入前的重复检查均显示：35个QML中 **23个已跟踪文件有未提交修改，8个controls加SeasonArchivePanel共9个QML未跟踪**；其余3个QML为WorkspaceGrid、ToastViewport、GroupManagerDialog。

已修改QML范围：

- Main、Theme。
- AppHeader、DanmakuLine、DanmakuOverlay、GuildMemberRow、GuildNavigationPanel、GuildRankHoverCard、PrimaryRoomDivider、RoomLibraryView、RoomSidebar、RoomTile、WindowControls、WorkspaceStatusBar。
- AddRoomDialog、CloseBehaviorDialog、NotificationSettingsDialog、TeamManagerDialog。
- MaoziRankPage、SettingsPage。
- DanmakuSettingsPanel、MonitoringStatusPanel、WorkspacePresetsPanel。

未跟踪界面包括controls目录8文件、pages/SeasonArchivePanel.qml。仓库还存在C++、Python服务、测试、构建和发布相关的既有修改；不能把本节的UI清单视为整个仓库状态。

关键基线区别：

- Main对HEAD的变化只有Palette、减弱动效桥接和“CSTG特勤终端”标题；三个主页面路由预先存在。
- Theme和8控件、Settings重排、RoomTile控制层、S1静态档案页面及相关C++数据支持都已在当前工作区存在。
- AppController头新增s1Archive及关闭偏好保存失败/重试属性，见 [app_controller.h:48](D:/DouyuMonitor/native/src/ui/app_controller.h:48)、[app_controller.h:64](D:/DouyuMonitor/native/src/ui/app_controller.h:64)。新视觉必须覆盖这些状态。
- scoped `git diff --stat`在QML、CMake和AppController头共25个已跟踪文件显示1731行插入/1393行删除；未跟踪文件不计入，亦不代表整个仓库变更量。

后续生产实现应先重新检查git status/diff，以当前已接受方案为准做最小增量。禁止回退、覆盖或整理掉上述既有修改；未经用户要求不提交、不推送、不创建Release。

## 5. 网页候选需要覆盖的验收状态

候选的目的，是让用户在进入Figma/生产实现前评估完整信息架构和交互可用性。至少覆盖：

1. 监控空工作区、1房、多房、主/双主布局、最小窗口带侧栏；舒适/紧凑/密集视频块；在线/离线/加载/失败/渲染暂停。
2. 当前/历史/收藏、公会分层和搜索、待确认房间、已添加/容量满；悬浮卡有/无榜单数据。
3. 四组设置、保存成功/失败、工作区保存告警；更新检查中/无更新/有更新/错误。
4. 全部四个有效对话框、三个面板、音频/布局/更多/画质浮层；打开/关闭、禁用、展开、键盘焦点、提示和长文本。
5. 弹幕三页及逐房覆盖；等待/失败/平台阻止/重试；通知可用/关闭/不可用。
6. 野榜及参与者详情；定级赛总览/详情/两种统计；常规赛/总决赛赛程及详情；足球树和三种榜单；搜索无结果/空档案、日期和分页。
7. TeamManager普通管理及导入idle/loading/error/preview/不可确认；旧group不伪装为当前可用入口。
8. 窗口关闭询问、记住选择、后台/退出；可见性和焦点行为；减弱动效。

这是一份候选覆盖清单，不是对生产行为新增或修改的授权。真实斗鱼连接、长时间多路播放、弹幕稳定性、CPU/GPU/内存和安装包仍需后续目标环境验收。

## 6. 本次验证记录与限制

已执行并读取结果：

- `rg --files native/app/qml -g '*.qml'`：35个QML文件；逐文件阅读，并读取DanmakuLaneScheduler.js和qmldir。
- 带行号读取Main、全部页面/组件/控件/面板/对话框；检查公开属性和命令。
- `rg -n 'PrimaryRoomDivider|GroupManagerDialog' native ...`：PrimaryRoomDivider仅出现在CMake资源注册；GroupManagerDialog无注册/调用结果。
- 读取CMake主模块及测试资源注册，核对四个有效主对话框和三个面板的Main实例。
- `git status --short`、`git diff --name-only`、`git diff --stat`、Main和AppController头的focused diff；再次确认23个已跟踪QML有修改及9个未跟踪QML。
- `git rev-parse --short HEAD`为968557b；`git branch --show-current`为main。

本次未执行：应用启动、QML引擎/交互/视觉/关闭回归测试、C++/Python测试、构建、截图、真实斗鱼连接、安装包或性能验证。因此本文只支持“当前源码表面和状态已盘点”“静态约束已核对”，不支持“界面已修复”“运行验证通过”或“重设计已完成”。

本次审计文档以外的QML、C++、配置和其他文档未由该审计子任务修改。Notion尚未同步；Figma和生产QML修改等待用户选定网页候选。
