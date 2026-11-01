# DouyuMonitor 全局 UI 视觉系统升级设计

**日期：** 2026-10-20
**范围：** Qt Quick/QML 桌面客户端的全局视觉与布局升级
**状态：** 设计已获用户确认，等待规格文档复核

## 1. 目标与约束

DouyuMonitor 保持现有 Qt Quick/QML 技术栈、业务模型和交互语义，在现有深色蓝灰、橙色主操作和绿色直播状态的基础上，建立一套可复用的桌面设计系统，并迁移截图涉及的全部界面。

本轮允许改变颜色层级、间距、控件尺寸、排版、面板结构和状态反馈。以下内容保持不变：

- 主播列表不增加单选、多选或复选框式选择。
- 打开房间、移除主播和列表标签切换继续互不干扰。
- 排行榜排序、评分、数据来源和筛选含义不变。
- 设置持久化、角色和队伍数据、关闭行为、更新检查不变。
- 弹幕设置的默认值、治理逻辑、房间开关和统计语义不变。
- 工作区布局语义、音频焦点和声道策略不变。
- 不修改播放器、网络请求、后台任务生命周期和视觉回归基线。
- 不引入 Electron、Chromium、Web 技术、第三方 UI 依赖、高频 Timer、ShaderEffect、模糊或复杂动画。

## 2. 视觉基线

### 2.1 目标环境

- 平台：Windows 桌面。
- 主验收尺寸：`1280×720`。
- 窄窗口验收尺寸：`960×600`。
- 输入：鼠标和键盘，所有主要控件保留键盘焦点路径。
- 主语言：中文；本轮不新增 RTL 布局，但不得通过固定宽度破坏后续文本扩展。
- 观看距离：桌面约 60 cm；正文使用可读的系统字体尺寸。

### 2.2 视觉方向

采用“令牌优先的增量升级”：先扩充 `Theme.qml` 和共享控件，再按页面迁移。该顺序能在当前工作区已有大量未提交修改的情况下限制回归范围，并避免页面各自维护一套颜色和尺寸。

默认信息密度使用平衡型。列表行保持较高可见数量，同时给主播名称、房间号、状态和操作留下稳定间距。左侧导航在 `960×600` 下仍保留完整文字，不自动收成图标栏。弹幕设置、监控状态、工作区预设和声音总控继续采用右侧浮层语义，统一面板规格；设置页和排行榜页继续使用现有页面导航。

## 3. 共享设计系统

### 3.1 颜色令牌

以现有主题令牌为基础，补充角色化名称，禁止页面继续新增原始十六进制颜色。令牌至少覆盖：

- 页面背景、顶栏/侧栏背景、主内容面板、控件表面、输入凹槽和浮层表面。
- 主文本、次级文本、弱提示、禁用文本。
- 橙色主操作、悬停和按下状态。
- 直播中、警告、错误/危险、成功和焦点描边。
- 边框、强边框、分隔线和选中表面。

颜色表达状态时必须同时有文字、图标、形状或结构变化。橙色只承担主要操作、当前项和关键强调；绿色不用于没有业务含义的整行边框。

### 3.2 尺寸与排版

在 `Theme.qml` 集中管理：

- 顶栏高度、侧栏宽度、面板内边距、页面和分组间距。
- 控件高度、主播行高、头像尺寸、按钮命中区域、浮层宽度。
- 面板、控件和输入框圆角。
- 页面标题、分组标题、正文、辅助信息、表格数据和主播元信息的字号与字重。
- 滚动条宽度、列表分隔方式和 Tooltip 延迟。

页面使用三到四级文字层级，不用字号差异制造噪声。正文和关键数据不能依赖过小的 caption 字号。长文本统一使用省略、换行或 Tooltip，不能覆盖邻近控件。

### 3.3 交互状态

共享控件必须分别定义 `normal`、`hover`、`pressed`、`checked/active`、`focused` 和 `disabled`。Hover 只反馈悬停，不覆盖 Active；Focus 始终可见；Disabled 同时降低文字和表面对比度但保留可辨识边界。按钮使用图标或现有项目图标表达工具动作，文字只保留在清晰的命令按钮中。

统一以下控件：`ControlBackground`、`ConsoleButton`、`ConsoleToolButton`、`ConsoleTextField`、`ConsoleComboBox`、`ConsoleCheckBox`、`ConsoleRadioButton`、`ConsoleToolTip`，并复用到页面、面板和对话框。

## 4. 页面迁移

### 4.1 主窗口与主播列表

涉及 `Main.qml`、`AppHeader`、`RoomSidebar`、`RoomLibraryView`、`RoomTile`、`GuildNavigationPanel` 和 `GuildMemberRow`。

- 顶栏、导航和内容区用表面层级建立结构，减少无意义的分割线。
- 标签活动状态使用稳定的橙色边缘或选中表面；Hover 不覆盖活动态。
- 主播行固定头像、行高、名称/房间号/状态排布和操作按钮位置。
- 直播中、未开播和异常状态使用语义色配合文字或图标。
- 分组标题、空状态、长名称、滚动和列表较多时保持对齐。
- 添加、移除和打开房间的命中区域彼此独立，不引入选择框语义。

### 4.2 排行榜

涉及 `MaoziRankPage.qml` 及其共享控件。

- 标题、数据规模、筛选、搜索、刷新和榜单切换形成稳定的顶部区域。
- 表头和数据列使用明确列宽，排名、主播名称、评分和直播状态可快速比较。
- 等级标签统一尺寸和角色色，行使用轻量分隔或交替表面。
- Hover 仅改变当前行表面；移除没有业务含义的绿色整行边框。
- 大量数据和长名称继续使用一致裁切与 Tooltip 行为。

### 4.3 设置页

涉及 `SettingsPage.qml`、关闭行为、通知、队伍和角色相关对话框。

- 以信息架构组织关闭按钮行为、队伍管理、更新检查和其他设置。
- 活动角色区域统一输入框、主播行、身份选择器、队伍选择器和保存操作的对齐。
- 左侧设置导航与右侧内容区保持稳定比例，窄窗口使用滚动而不是裁切。
- 保持关闭行为、持久化、角色配置、队伍数据和更新功能。

### 4.4 浮层与对话框

涉及 `DanmakuSettingsPanel`、`MonitoringStatusPanel`、`WorkspacePresetsPanel`、声音总控、布局选择 Popup 和现有 dialogs。

- 右侧浮层统一标题栏、关闭按钮、内容滚动区、底部操作区、宽度和内边距。
- 弹幕设置的显示、治理、统计三个标签保持清楚层级；开关、滑块、分段选择器、下拉框、密度和渲染选项共享控件状态。
- 监控状态使用数字、状态名称和语义色/图标；内容较少时仍保持完整布局，计数变化不改变面板骨架。
- 工作区预设输入、保存、应用和删除操作主次分明；删除保留危险操作和必要确认语义；名称过长时不撑破布局。
- 布局选择明确当前项和可选项，保留单主播、双主播、主副直播间等原有语义。
- 声音总控明确全局静音、单声道/多声道的当前选择，悬停不冒充选中状态；保留音频焦点和声道策略。
- 对话框支持 Escape 关闭或取消，焦点不会被困在不可退出的浮层中。

## 5. 响应式、可访问性与性能

- `1280×720` 和 `960×600` 都要检查重叠、裁切、异常留白和滚动条可用性。
- 所有交互控件可用鼠标和键盘操作，焦点顺序按视觉阅读顺序排列，焦点描边可见。
- 状态不只靠颜色表达；错误和危险操作需要文字或图标辅助。
- 不新增持续运行的高频刷新；状态过渡只使用轻量的透明度或表面变化，尊重现有 `reducedMotion` 令牌。
- 不在 UI 线程加入同步网络请求或重型数据处理，不整页重建来刷新样式。
- 不使用大面积透明叠层、模糊、复杂阴影或高成本动画影响播放性能。

## 6. 实施边界与文件策略

实现时优先修改以下现有文件：

- `native/app/qml/Theme.qml`。
- `native/app/qml/controls/` 下共享控件。
- `native/app/qml/components/` 下导航、列表、工作区和状态组件。
- `native/app/qml/pages/` 下设置和排行榜页面。
- `native/app/qml/panels/` 和 `native/app/qml/dialogs/` 下浮层与对话框。

不修改 C++ 领域层、模型、服务协议、网络逻辑、持久化键、安装流程、视觉基线或依赖锁。`GroupManagerDialog.qml` 当前没有在 `native/CMakeLists.txt` 的 QML 模块文件清单中注册；本轮不默认把它加入运行时，若页面需要它必须先以当前构建事实核验。

当前工作区包含大量前序未提交修改。实施前必须针对每个目标文件检查现有 diff，只追加与本轮视觉升级相关的改动，不执行 `reset`、`restore`、`clean`、stash、暂存或提交。

## 7. 验证策略

### 7.1 自动化检查

按修改范围执行：

```powershell
cmake --build --preset windows-x64-release --target qml_controls_style_test qml_engine_smoke_test qml_visual_smoke_test qml_interaction_test
ctest --preset windows-x64-release -R 'qml_controls_style_test|qml_engine_smoke_test|qml_visual_smoke_test|qml_interaction_test'
```

必要时补充受影响页面的 QML 测试和静态语法检查。若构建目录需要重新生成或出现 `no work to do`，如实记录实际行为。

### 7.2 视觉与人工检查

使用隔离运行或现有安全的测试输出检查：

- 主窗口、主播列表、排行榜、设置页和全部浮层在两种窗口尺寸下的布局。
- 长名称、空数据、加载、错误、列表较多、Popup 打开、键盘 Tab/Escape 和焦点显示。
- 主播列表的打开房间、移除主播、标签切换互不干扰。
- 页面反复打开关闭不会重复创建组件或刷新任务。

不更新正式视觉回归基线。若 `qml_visual_regression_test` 产生差异，保存诊断证据并说明变化来自哪个视觉迁移，不通过降低阈值或替换基线取得通过。

## 8. 验收标准与已知限制

验收要求：

1. 截图涉及的所有页面都迁移到共享主题，而不是只修改主窗口。
2. 共享颜色、间距、尺寸和状态由主题/控件集中管理。
3. 业务入口、交互语义、持久化和后台任务行为保持不变。
4. 主要控件的正常、悬停、按下、选中、聚焦和禁用状态可区分。
5. `1280×720` 与 `960×600` 没有明显重叠、裁切或失控留白。
6. 相关 QML 测试执行并记录真实结果；未执行项目单独列出。

以下事项不由本轮自动推出：

- 真实 Windows DPI、系统大字体和色盲模拟结果，需要目标环境人工检查。
- 真实斗鱼连接下的长期播放性能，需要目标环境验证。
- 正式视觉基线是否批准，需要独立审核。

## 9. 复核记录

用户已确认以下设计选择：

- 方案 A：令牌优先的增量升级。
- 平衡型信息密度。
- 窄窗口保留完整文字侧栏。
- 右侧浮层保持现有语义并统一规格。

本规格文档只描述设计和验收边界。实现计划将在用户复核规格后单独生成。

## 10. 2026-10-21 规格复核

### 10.1 复核范围与工作区保护

本次复核只读取仓库、CMake QML 清单、QML 调用关系、测试定义和既有视觉验证记录。没有修改产品源码、QML、主题、测试、视觉基线、依赖、构建配置或发布产物。

复核时的仓库状态：

- 分支：`main`
- HEAD：`968557b7ea78359ddb88aa673e0a3d1567b008b3`
- 已修改跟踪文件：84 个
- 未跟踪条目：35 个
- 暂存区：空
- `git diff --check`：通过；Git 对部分既有文件报告 LF/CRLF 转换提示，但没有空白错误
- 本轮计划涉及的 QML 文件已有前序未提交修改，实施时必须逐文件查看当前 diff，只追加本轮视觉迁移内容

本轮不执行 `reset`、`clean`、`restore`、stash、分支切换、暂存或提交。既有审计证据、发布候选、安装器、视觉基线和未跟踪文件均视为用户资产。

### 10.2 实际 QML 注册与运行时入口

当前 `native/CMakeLists.txt` 的 `qt_add_qml_module(douyu_qml)` 注册了以下运行时文件：

| 区域 | 实际文件 | 当前入口/调用关系 | 本轮状态 |
| --- | --- | --- | --- |
| 根窗口 | `native/app/qml/Main.qml` | 应用根界面；实例化 Header、侧栏、导航、工作区、页面、浮层和对话框 | 本轮改造前置核查；只保留顶层布局和业务连接 |
| 主题与共享控件 | `native/app/qml/Theme.qml`；`native/app/qml/controls/ControlBackground.qml`、`ConsoleButton.qml`、`ConsoleToolButton.qml`、`ConsoleTextField.qml`、`ConsoleComboBox.qml`、`ConsoleCheckBox.qml`、`ConsoleRadioButton.qml`、`ConsoleToolTip.qml` | 被页面、面板和对话框共同引用；`Theme.qml` 通过 `qmldir` 注册为 singleton | 本轮改造 |
| 顶栏与窗口控制 | `native/app/qml/components/AppHeader.qml`、`WindowControls.qml` | `Main.qml` 直接实例化；顶栏内含声音总控 Popup 和布局选择 Popup | 本轮改造 |
| 当前主播列表 | `native/app/qml/components/RoomSidebar.qml` | `Main.qml` 在 `sidebarVisible` 且导航不可见时实例化；当前列表 delegate 内联 | 本轮改造 |
| 历史/收藏列表 | `native/app/qml/components/RoomLibraryView.qml` | `RoomSidebar.qml` 根据 `viewMode` 显示；打开房间、移除历史和收藏过滤在此处理 | 本轮改造 |
| 主播工作区卡片 | `native/app/qml/components/WorkspaceGrid.qml`、`RoomTile.qml`、`PrimaryRoomDivider.qml` | `Main.qml` 显示 `WorkspaceGrid`；`WorkspaceGrid` 使用 `Repeater` 创建 `RoomTile` | 仅核查；必要时只迁移表面令牌，不改变播放器 Loader、布局语义或容量 |
| 社团导航与成员 | `native/app/qml/components/GuildNavigationPanel.qml`、`GuildMemberRow.qml`、`GuildRankHoverCard.qml` | `Main.qml` 在 `navigationVisible` 时显示；悬停通过 `Main.qml` 的定时器打开排名卡 | 本轮改造 |
| 弹幕渲染与状态 | `native/app/qml/components/DanmakuOverlay.qml`、`DanmakuLine.qml`、`ToastViewport.qml`、`WorkspaceStatusBar.qml` | 工作区和根窗口直接使用；弹幕行由现有调度器创建和销毁 | 仅核查；保持渲染与刷新生命周期 |
| 排行榜 | `native/app/qml/pages/MaoziRankPage.qml` | `Main.qml` 将 `currentView` 切换为 `maoziRank`，进入时调用 `rankClient.refresh()` | 本轮改造 |
| 设置 | `native/app/qml/pages/SettingsPage.qml` | `Main.qml` 将 `currentView` 切换为 `settings`；通过 signal 打开队伍管理 | 本轮改造 |
| 右侧浮层 | `native/app/qml/panels/DanmakuSettingsPanel.qml`、`MonitoringStatusPanel.qml`、`WorkspacePresetsPanel.qml` | `Main.qml` 常驻实例化，分别由顶栏按钮打开 | 本轮改造 |
| 对话框 | `native/app/qml/dialogs/AddRoomDialog.qml`、`NotificationSettingsDialog.qml`、`CloseBehaviorDialog.qml`、`TeamManagerDialog.qml` | `Main.qml` 常驻实例化；设置、监控状态和关闭流程通过 signal/窗口事件打开 | 本轮改造 |
| 旧分组对话框 | `native/app/qml/dialogs/GroupManagerDialog.qml` | 不在当前 `qt_add_qml_module` 清单；`qmldir` 只有 `Theme` singleton；`Main.qml` 和测试未发现有效实例化路径 | 仅核查，不在本轮改造范围；不得自行注册或接入 |

声音总控和布局选择不是独立 QML 文件，而是 `AppHeader.qml` 内的 `soundMasterPopover` 与 `layoutMenu`。实施计划必须按这两个实际运行时对象迁移，不能创建没有复用价值的替代组件。

### 10.3 主题令牌与硬编码边界

`native/app/qml/Theme.qml` 已提供并被共享控件使用的令牌包括：

- 表面：`canvas`、`appBar`、`managementSurface`、`controlSurface`、`well`
- 边框：`border`、`borderStrong`
- 文本：`text`、`mutedText`、`disabledText`
- 交互：`accent`、`accentHover`、`accentPressed`、`accentText`、`selectedSurface`、`controlHover`、`controlPressed`、`controlDisabled`
- 语义：`online`、`warning`、`danger`、`dangerSurface`
- 尺寸：`topBarHeight=52`、`controlHeight=32`、`buttonMinWidth=100`、`fieldMinWidth=200`、`comboMinWidth=140`、`roomRowHeight=60`、`iconSize=16`、`indicatorSize=18`
- 间距/圆角：4/6/8/12/16 间距，`radiusIndicator=4`、`radiusSmall=6`、`radiusMedium=8`、`radiusLarge=8`
- 文字/动效：`fontCaption=11`、`fontControl=12`、`fontBody=14`、`fontHeading=16`、Tooltip 延迟 500 ms，动效 120/150/180 ms
- 动效可访问性：`reducedMotion` 已真实存在；`Main.qml` 将根窗口属性同步到 `Theme.reducedMotion`

缺口不是缺少一整套新控件，而是页面仍直接写入角色值和尺寸。复核发现的主要迁移对象如下：

- `AddRoomDialog.qml`、`DanmakuSettingsPanel.qml`、`MaoziRankPage.qml`、`RoomTile.qml`、`SettingsPage.qml`、`TeamManagerDialog.qml` 和 `AppHeader.qml` 中仍有原始颜色、字号、局部圆角、列表行高和列宽。
- `MaoziRankPage.qml` 仍直接使用多个表格列宽和直播行绿色边框；绿色边框只有在表达直播状态时才应保留，并同时显示状态文字。
- `DanmakuSettingsPanel.qml` 和部分对话框仍使用 `#f4f6f8`、`#d6dde5`、`#9ba5b1`、`#171d25`、`#343b45` 等页面级颜色。
- `GroupManagerDialog.qml` 的硬编码样式不属于当前运行时，不能作为本轮共享控件迁移的依据。

实施时优先复用已有令牌和 8 个共享控件。只有在多个页面确实需要同一角色且现有令牌无法表达时，才考虑新增 `overlaySurface`、`divider`、`focus`、页面标题/分组标题等角色令牌；不为单个页面的局部值建立新控件或新 singleton。新增令牌必须在 `qml_controls_style_test` 中增加最小的可读性和稳定尺寸断言。

### 10.4 性能保护边界

本轮将“打开导航页后直播卡顿”和“检查结束后仍持续卡顿”登记为保护性回归项目，不能通过视觉迁移顺便修改根因。以下代码边界保持不变：

- 不修改 `AppController`、`RoomSession`、`MultiRoomCoordinator`、播放器 Loader、网络请求、StreamGet 协议或后台任务生命周期。
- 不改变 `WorkspaceGrid` 的 `Repeater`、`RoomTile` 的 `Loader` 和控制隐藏计时器。
- 不改变 `DanmakuOverlay` 的创建/销毁与 `launchTimer`，不新增逐帧或高频 Timer。
- 不改变 `GuildNavigationPanel` 的 `rankSyncTimer` 和页面切换触发的刷新行为。
- 不通过整页销毁/重建刷新样式；`Main.qml` 当前常驻页面、浮层和对话框实例的结构保持不变。
- 只允许对可见性、表面、文字和轻量状态过渡做声明式调整；新动效必须受 `Theme.reducedMotion` 控制，且不动画化复杂子树的宽高。

若性能回归仍可复现，计划只保存现象、复现条件、测试输出和可能涉及的现有模块，作为独立问题处理，不扩展本轮源码范围。

### 10.5 视觉验收数值

以下数值作为实现和审查的初始验收基准：

- 窗口：`1280×720` 主基准，`960×600` 窄窗口；`Main.qml` 的最小尺寸和完整文字侧栏保持不变。
- 顶栏：52 px；当前主播侧栏：268 px；社团导航：284 px。
- 常规控件：32 px 高；图标默认 16 px；头像基准 30/32 px；主播行 60 px；状态栏当前 28 px，若调整必须保持工作区与状态栏无重叠。
- 浮层：弹幕设置 380 px、监控状态 320 px、工作区预设 360 px；声音总控当前 176 px。迁移后应统一标题栏、内边距、关闭按钮命中区和滚动区域，只有内容确实溢出时才改变宽度。
- 对话框：当前 AddRoom 500×560、TeamManager 620×680、CloseBehavior 360 宽、Notification 360 宽；任何尺寸变化都必须在两种基准窗口下检查。
- 间距：优先使用 4/6/8/12/16 px 令牌；页面内边距通常 16 px；列表行内部间距 6–8 px；弹窗标题与内容间距 8–12 px。
- 字号：页面标题 20 px 级别、分组标题 16 px、正文/关键数据 14 px、控件文本 12 px、仅用于辅助元信息的 caption 11 px。不得新增低于 11 px 的用户可读文字；现有 10 px/9 px 成员元信息需在迁移时评估是否能改为 11 px 而不破坏密度。
- 状态：Hover、Pressed、Active/Checked、Focused、Disabled 必须在表面或边框上各自可区分；Focus 使用稳定的 2 px 视觉边界或等效高对比反馈；Disabled 降低文字/表面对比度但不移除边界；颜色状态必须伴随文字、图标或结构变化。
- 对比度：沿用 `qml_controls_style_test` 的计算方式；普通文字与表面目标至少 4.5:1，较大文字和 UI 边界至少 3:1。页面迁移后应扩展检查到代表性的标题、正文、辅助文字、主按钮和危险按钮，而不是只检查一个 fixture。

测试应在 `1280×720` 和 `960×600` 下检查边界、重叠、裁切、滚动条和异常留白；另在 `1600×900`、`1920×1080` 保留当前多房布局和长列表回归。Windows DPI、多显示器、系统大字体、原生 Popup 位置和真实 GPU 合成仍需要目标机器人工检查，不能由 offscreen 结果替代。

### 10.6 弹窗、Escape、焦点和未保存修改

- `AddRoomDialog` 打开后将搜索输入框置于焦点；Escape/关闭只关闭对话框，不提交搜索、不添加房间。
- `NotificationSettingsDialog` 当前勾选项即时调用 `savePreferences()`；统一视觉时不得把关闭或 Escape 改成额外保存，也不得把取消误实现为提交。
- `CloseBehaviorDialog` 的取消按钮只关闭询问框；“进入后台”和“完全退出”继续调用各自 controller 命令。Escape 只能走取消语义，不能触发关闭应用。
- `TeamManagerDialog` 关闭时若正在导入，现有 `onClosed` 会调用 `cancelMaoziTeamImport()`；Escape 需要保持这一取消行为，不能确认导入或丢失未导入数据。
- 右侧浮层和声音/布局 Popup 的 Escape 只关闭当前浮层并恢复触发按钮或合理的下一个焦点；不改变音频、布局、弹幕、监控或预设数据。
- 所有新增或统一的关闭按钮必须保留可见焦点边界；关闭后焦点返回打开它的按钮，无法证明恢复时至少返回页面根 FocusScope，不得把焦点留在已隐藏对象。
- 工作区未保存提示仍由 `workspaceUnsaved` 控制；视觉迁移不能把“重试保存”变成自动保存，也不能在关闭路径中绕过现有确认。

当前自动化测试覆盖部分 Popup Escape、对话框加载、取消导入和关键打开/关闭流程，但尚未证明每个对话框的 Escape 后焦点恢复。因此计划将该缺口放入交互回归阶段。

### 10.7 测试目标与既有差异

当前 `windows-x64-release` preset 使用 Ninja/Release，构建目录为 `native/out/build/windows-x64-release`。`ctest --preset windows-x64-release -N -V` 当前注册 38 项，其中与本轮直接相关的目标真实存在：

- `qml_controls_style_test`
- `qml_engine_smoke_test`
- `qml_visual_smoke_test`
- `qml_visual_regression_test`
- `qml_interaction_test`
- `qml_close_regression_test`
- `qml_danmaku_overlay_test`

没有发现名为 `qml_controls_style_test` 之外的虚构“样式测试”目标；实施计划只使用上述真实目标和现有 `ctest` preset，不新增空测试来满足名称要求。

既有视觉证据来自前序验证，不是本轮新结果：

- `guild-navigation-284x720.png`：约 2.071% 差异
- `team-manager-720x760.png`：约 6.871% 差异
- `maozi-rank-1920x1080.png`：约 2.468% 差异
- 另一份记录报告了五个视觉 mismatch，基线和阈值均未被批准修改

实施前先把这些记录标记为“改造前已存在的差异”。实施后每次视觉回归都要把结果归类为：

1. 改造前已存在并仍然存在；
2. 本轮迁移新增；
3. 由字体、offscreen、截图时序或其他测试环境造成，当前无法判定。

不更新、覆盖或批准正式基线，不调整像素阈值来掩盖差异。

本次规格复核只执行了源码、配置、文件清单和 `ctest -N -V` 注册检查，没有运行任何 CTest、构建、应用启动或视觉截图回归；下文实施计划中的测试命令均为后续实施阶段的验证步骤。

### 10.8 规格复核结论与修订

原规格的设计方向继续有效，补充以下可执行约束：

1. 页面迁移清单以 `native/CMakeLists.txt` 和 `Main.qml` 的实际调用为准；`GroupManagerDialog.qml` 只核查，不注册。
2. 共享系统先扩展 `Theme.qml` 和现有 `controls/`，页面按风险分批迁移；不创建重复的按钮、弹窗或布局组件。
3. 视觉数值以当前 52/268/284/32/60 px 基准和 4/6/8/12/16 px 间距为起点，所有变化必须经两种窗口尺寸的结构检查。
4. `reducedMotion` 已存在，可以安全作为新轻量过渡的开关；不得再假设或引入另一个同义设置。
5. 播放、网络、后台任务、服务协议和持久化继续属于禁止修改范围；卡顿只作为保护性回归记录。
6. Escape、取消、焦点恢复和未保存修改按照上节的现有业务语义执行；视觉统一不能改变提交时机。
7. 实施计划必须按阶段提交可审查的增量，但当前项目规则禁止未经用户要求的提交，因此计划不包含 `git add`、`git commit` 或分支操作。

规格复核完成。独立实施计划见 `docs/superpowers/plans/2026-10-21-global-ui-visual-upgrade.md`。
