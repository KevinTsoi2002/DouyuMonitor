# R3 → QML 最小实施映射

日期：2026-10-30，Asia/Shanghai。输入为用户正式确认的最新 R3（C 组件、C1 夜间/C3 白天、80px 导航轨、36px 顶部与缩小图片字标、覆盖式观看工具、无常驻画内工具）。本文件是实施前源码映射；未修改生产文件，未运行新 QML 构建或运行验收。Notion 需由主代理另行记录，同步状态不能由本文件推定。

## 1. 当前事实与最小边界

- [Main.qml:37](/D:/DouyuMonitor/native/app/qml/Main.qml:37) 把 `workspace.sidebarVisible` 与 `workspace.navigationVisible` 映射成互斥的 268px 房间侧栏和 284px 公会面板；`leftPanelWidth` 直接扣减所有主页面。R3 的 80px 是新的**页面导航轨**，不能把 `RoomSidebar` 压成 80px。
- [Main.qml:314](/D:/DouyuMonitor/native/app/qml/Main.qml:314) 常驻 `AppHeader`、`RoomSidebar`、`GuildNavigationPanel`、`WorkspaceGrid`、设置、榜单与弹层。当前页面名为 `monitoring/settings/maoziRank`；导航是额外面板状态，尚无独立公会页面路由。
- [AppHeader.qml:30](/D:/DouyuMonitor/native/app/qml/components/AppHeader.qml:30) 同时承担品牌、三个导航入口、弹幕、监控、预设、布局、声音、全屏与原生窗口控制；[Theme.qml:34](/D:/DouyuMonitor/native/app/qml/Theme.qml:34) 顶栏为 52px。应迁移入口及面板归属，不重写对应命令。
- [WorkspaceGrid.qml:198](/D:/DouyuMonitor/native/app/qml/components/WorkspaceGrid.qml:198) 的 `Repeater` 持有 `RoomTile`。主题、工具抽屉、导航轨和沉浸切换只能改颜色/可见性/几何，不得替换 `roomModel` 或重建网格。`RoomTile` 的播放器 Loader 只由 `renderEnabled && controller !== null` 控制，见 [RoomTile.qml:141](/D:/DouyuMonitor/native/app/qml/components/RoomTile.qml:141)。
- 所有五个指定 QML、CMake、AppController 头及八个控件都有已有未提交工作；以当前工作区为底稿。不得以 HEAD 版本覆盖、回退或批量格式化。先前 `UI_AUDIT.md` 是较早静态快照；例如目前 `GuildNavigationPanel` 已按 confirming 状态使用 84px 行高，不能据旧审计再宣称该重叠仍未修复。

## 2. 推荐组件边界

| 位置 | 最小改动 | 保留与注意 |
| --- | --- | --- |
| `Main.qml` | 新增 80px `AppNavigationRail`、一个观看工具抽屉编排入口、观看区焦点容器；统一页面和弹层切换 | 保留 `appController` 注入、preview/fallback 路径、根对象名、关闭/最小化生命周期、状态警告、Toast、现有弹层实例 |
| `Theme.qml` | 引入显式 `night/day` 选择；现有 token 名保持，值按模式绑定；`topBarHeight=36`，新增 `navigationRailWidth=80` | 不改通用 `controlHeight=32` 来迁就顶栏；顶栏按钮另设 26–28px 几何。视频始终深色，另设 video 背景/前景 token |
| `AppHeader.qml` | 只呈现图片字标、标题栏拖动空白、观看工具入口、日夜选择与 `WindowControls` | 保留 `systemMoveRequested`、最大化、全屏、关闭信号；标题栏拖动区不能覆盖按钮；保留窗口标题和图片 Accessible.name |
| 新 `AppNavigationRail.qml` | 主播导航、团播榜单、直播工作区、设置；80px 固定宽，文字与图标完整键盘访问 | 页面导航只发信号；公会查询/分队/确认房间逻辑继续属于原公会组件；不增加网页专用“组件状态”生产菜单 |
| 新 `WatchToolsPanel.qml` | 同一个覆盖容器承载工具首页及房间/布局/声音区域，或明确只开一个子浮层 | 工具首页保留房间、添加、布局、声音与弹幕、监控、预设、全屏。后四项不能因网页只演示少量入口而丢失 |
| `RoomSidebar.qml` | 整体作为房间抽屉内容复用；保留当前/历史/收藏、输入、排序和操作 | 不再参与 `WorkspaceGrid` 横向排版；抽屉宽度独立于 80px 导航轨。原 settings 信号仍可导航并关闭抽屉 |
| `GuildNavigationPanel.qml` | 复用为导航页面内容；先只改宿主和宽度 | `syncActive/visible`、60秒排名检查、直播查询、鼠标悬停坐标与 hover card 关闭条件一起迁移 |
| `WorkspaceGrid.qml` | 保留实例、模型与现有布局算法，调整外部锚点与边距 | 不将网页四路截断或三个概念布局字符串直接写入领域层；原生 `auto/primary/primary-two` 能力必须保留 |
| `RoomTile.qml` | 保留播放器、数据角色、画质/音量/弹幕与房间菜单；补足聚焦控件不自动隐藏的条件 | 视频几何已 fill tile，不新增说明卡；真实等待、离线、错误、弹幕等待信息属于运行状态，不能当网页装饰删除 |

正常模式：顶栏占 36px；导航轨从其下方到窗口底；主页面从轨右侧铺满。观看区使用剩余空间，房间/工具展开不改变网格矩形。状态条可以保留原高度，但用真实监控信息；不要把现有硬编码 `danmakuCount: 0` 当成真实统计。[Main.qml:407](/D:/DouyuMonitor/native/app/qml/Main.qml:407)

沉浸建议复用现有 `Window.FullScreen` 作为可见状态，入口继续调用 `AppController.toggleFullScreen/exitFullScreen`，由控制器保留进入前窗口状态；隐藏顶栏、轨和状态条，网格改锚点，保留相同对象。不要另建未同步的“已全屏”布尔值。F11 与画面双击行为继续有效；若产品要额外的“窗口内沉浸”模式，应作为独立选择，不在本次最小实现里偷偷改变原全屏含义。[Main.qml:198](/D:/DouyuMonitor/native/app/qml/Main.qml:198)

### 2.1 新导航与旧持久字段

`setNavigationVisible` 不只是显示：它启动/停止 `GuildRoomResolver`、刷新资料并持久化；`setSidebarVisible` 也会写工作区。见 [app_controller.cpp:1300](/D:/DouyuMonitor/native/src/ui/app_controller.cpp:1300)。不能只改 `currentView` 而让解析器继续在隐藏页面运行。

建议集中 `showPage()`：进入公会页时调用 `setNavigationVisible(true)`，离开时 false；进入设置/榜单时关闭房间浮层并清理 hover。老 `toggleNavigationVisibility()` 留作转发入口。`roomSidebar` 的开闭必须与 `workspace.sidebarVisible`、预设应用保持同步，避免 QML 赋值破坏绑定；`sidebarVisible` 不再决定网格宽度。

启动默认闭合抽屉与旧工作区 `sidebarVisible=true` 恢复存在语义冲突。最小兼容阶段建议读取原持久值恢复抽屉，但宽度不挤压视频；若要强制首次闭合，应显式实现并测试一次迁移，不能在每次启动直接覆盖用户设置。`navigationVisible` 恢复同理，初始化路由需匹配控制器已启动的解析器状态。[app_controller.cpp:1699](/D:/DouyuMonitor/native/src/ui/app_controller.cpp:1699)

### 2.2 字标与日夜模式

最新批准的网页字标容器是 210×24px，顶栏仍 36px；原图为 RGBA，派生图仅裁透明边，详见 [wordmark-assets-metadata.json](/D:/DouyuMonitor/docs/ui-redesign/visual-review/assets/wordmark-assets-metadata.json)。将已确认的 day/night 派生图复制为生产资源；不能从 docs 路径在运行时读取。`Image.PreserveAspectFit`，设置 `sourceSize`，提供错误回退与“CSTG特勤终端”无障碍名称。

现有 `applicationBrandText` 测试要求 Text 的 `text`。不要为迁就测试保留不可见假文字；若改为图片，更新测试检查图片 source/Ready/尺寸以及 Accessible.name，同时仍断言窗口标题。`brandMark/brandIcon` 可保留于新图片容器/图像，有助稳定定位。

`Theme` 的现有 palette 由各 `Console*` 控件复用。日间视频控件不能直接跟随日间面板字体变暗；为视频覆盖条设置独立深色表面与亮文字。现有白色固定 SVG 图标在日间可能失去对比，需要逐图核对或提供现有控件支持的着色路径，而不是只换 Theme.accent。

AppController 当前没有主题属性。若本轮保留网页已确认的模式记忆，推荐增加小型受限 `themeMode` 属性和 setter，使用单独 QSettings UI key（名字在实现时明确），只接受 night/day；不修改 `nativeWorkspaceV1` schema。缺失/非法值回退夜间，写入失败有可测试语义。测试用注入 QSettings，避免 QML 单例直接写真实用户配置。未实现持久化时不能声称日夜偏好可恢复。

## 3. 必须保持的控制器与模型契约

权威接口为 [app_controller.h:35](/D:/DouyuMonitor/native/src/ui/app_controller.h:35)、[workspace_model.h:14](/D:/DouyuMonitor/native/src/ui/workspace_model.h:14)、[monitoring_model.h:7](/D:/DouyuMonitor/native/src/ui/monitoring_model.h:7) 与 [danmaku_controller.h:11](/D:/DouyuMonitor/native/src/danmaku/danmaku_controller.h:11)。以下是迁移宿主后仍要连通的完整受影响边界，不表示本次要重写这些接口。

| 边界 | 保留接口/状态 |
| --- | --- |
| 根窗口/视图 | `rooms`, `workspace`, `monitoring`, `libraryRooms`, `guildRoster`, `workspaceUnsaved`, `retryWorkspaceSave()`；`closeBehavior`, `quitRequested`, `backgroundHosted`, `windowMinimized` |
| 房间查找与列表 | `addRoom()`, `searchRooms()`, `searchResults/searchStatus/searchError`, `addRoomCandidate()`, `requestRemoveRoom()`, `removeHistoryRoom()`, `setFavorite()`, `moveFavoriteRoom()`, `moveRoom()`；移除继续使用延迟请求，不能改为在 QML handler 中同步销毁 delegate |
| 布局与主画面 | `setLayout("auto"/"primary"/"primary-two")`, `setPrimaryRoom()`, `setSecondaryPrimaryRoom()`, `setPrimaryRoomRatio()`；`rooms.roomCount` 与双主至少4房资格；`workspace.layoutMode/primaryRoomId/secondaryPrimaryRoomId/primaryRoomRatio/maxRooms` |
| 声音与播放 | `setAudioRoom()`, `setRoomMuted()`, `setAudioMode("single"/"multi")`, `setGlobalMuted()`, `setVolume()`, `setQuality(roomId, quality, rate)`, `refreshRoom()`, `retryPlayback()`；`workspace.audioMode/audioRoomId/globalMuted`；错误经 `workspace.setLastMessage()` 显示 |
| 播放器绑定 | `attachPlayer(roomId, MpvQuickItem*)`, `detachPlayer(roomId, MpvQuickItem*)`；`attachedPlayerRoomId`、`onRoomIdChanged` 重绑及 Component 销毁解绑顺序 |
| 弹幕 | `toggleDanmaku(roomId)`, `danmaku.globalEnabled`, `setGlobalEnabled()`, `displaySettings`, `setDisplaySetting()`, `governanceSettings`, `governanceSettingsForRoom()`, `setGovernanceSetting()`, `clearRoomGovernanceOverride()`, `statusForRoom()`, `statsForRoom()`, `clearStats()`, `retry()`, `presentationSuspended`；保留 overlay 消费链 `takeNextMessage()` |
| 工作区与持久状态 | `setSidebarVisible()`, `setNavigationVisible()`, `saveWorkspacePreset()`, `applyWorkspacePreset()`, `deleteWorkspacePreset()`；`workspace.sidebarVisible/navigationVisible/presets/lastMessage/lastMessageLevel/lastMessageTimeoutMs`。分组底层接口/schema不删除，但不恢复未注册 GroupManager UI |
| 公会/队伍/榜单 | `refreshGuildLiveStatus()`, `addGuildMemberRoom()`, `setGuildMemberRoomId()`, `createTeam/renameTeam/deleteTeam/moveTeam`, `assignGuildMemberToTeam/removeGuildMemberFromTeam`, `workspace.teams`, `maoziRank`, `s1Archive`, `rankSyncPending/rankSyncError`, `maoziTeamImport`, `previewMaoziTeamImport/confirmMaoziTeamImport/cancelMaoziTeamImport` |
| 设置/通知/更新 | `notificationPreferences`, `setNotificationsEnabled()`, `setNotificationPreferences()` 含收藏标题变化；`eventMappingJson/saveEventMapping()`；`updateState/updateMessage/currentVersion/latestVersion/updateReleaseUrl`, `checkForUpdates/openLatestRelease/openExternalUrl()`；`closeBehaviorUnsaved/closeBehaviorError`, `setCloseBehavior/clearCloseBehavior/retryCloseBehaviorSave` |
| 生命周期 | `minimizeWindow/restoreFromMinimized`, `toggleMaximizedWindow`, `toggleFullScreen/exitFullScreen`, `closeWindow/requestClose`, `minimizeToBackground/closeToTray/restoreFromBackground/requestQuit/shutdown`；保留 Main.onClosing 与 onVisibilityChanged，不能把新抽屉的“关闭”接到这些应用退出命令 |

`RoomTile` 现有必需角色逐个保留：`roomId, anchorName, title, category, viewerLabel, avatarUrl, liveState, playbackState, primary, favorite, audioFocused, requestedQuality, requestedQualityRate, effectiveQuality, muted, volume, danmakuEnabled, danmakuState, danmakuErrorCode, renderEnabled, index, availableQualities`；附加 `secondaryPrimary` 保留。[RoomTile.qml:14](/D:/DouyuMonitor/native/app/qml/components/RoomTile.qml:14)

`RoomSidebar` delegate 的最小模型角色为 `index, roomId, anchorName, title, category, viewerLabel, avatarUrl, liveState, primary, favorite, audioFocused, muted`。历史/收藏仍由 `RoomLibraryView` 使用 `libraryRooms`；不要把前四视频示例的限制带进真实房间名单。

## 4. 对象名与测试寻址约束

对象名尽量保留在迁移后的真实交互对象上。移动父组件意味着 `header->findChild()` 的测试应改成对应新面板/根窗口查找；不要在旧头部留下隐藏假按钮来让旧测试通过。重复对象名会使 `findChild()` 随声明顺序误命中。

| 范围 | 保留定位名（宿主可变化） |
| --- | --- |
| Main | `douyuQuickWindow`, `appHeader`, `roomSidebar`, `guildNavigationPanel`, `workspaceGrid`, `settingsPage`, `maoziRankPage`, `guildRankHoverCard`, `workspaceUnsavedWarning`, `retryWorkspaceSaveButton`, `toastViewport` |
| 字标/窗口 | `brandMark`, `brandIcon`, `applicationBrandText`（图片替换需明确更新测试契约）, `titleBarDragArea`, `windowControls`, `minimizeButton/minimizeIcon`, `maximizeButton/maximizeIcon`, `closeButton/windowCloseIcon` |
| 导航/工具 | `sidebarToggleButton`, `hamsterNavigationButton/hamsterNavigationIcon`, `maoziRankButton/maoziRankIcon`, `danmakuButton`, `monitoringButton`, `workspaceButton/workspacePresetIcon`, `layoutMenuButton/layoutMenuIcon`, `soundMasterButton`, `fullscreenButton/fullscreenIcon`；新增稳定的 `watchToolsButton/watchToolsDrawer/appNavigationRail/immersiveEdge` |
| 布局/声音 | `layoutMenu`, `layoutMenuBackground`, `layoutMenuItemLabel`, `autoLayoutOption`, `primaryLayoutOption`, `dualPrimaryLayoutOption`, `dualPrimaryLayoutOptionBackground`, `soundMasterPopover`, `audioModeControl`, `closeSoundMasterButton`, `globalMuteButton`, `audioSingleButton`, `audioMultiButton` |
| 房间抽屉 | `quickAddButton`, `historyTab`, `favoritesTab`, `roomList`, `roomLibraryView`, `roomSidebarSelectionSurface`, `roomSidebarSelectionIndicator`, `roomSidebarAvatarImage`, `sidebarRoomAnchorName`, `sidebarRoomTitle`, `sidebarRoomActionBar`, `roomFavoriteButton/roomFavoriteIcon`, `moveRoomUpButton`, `moveRoomDownButton`, `roomSidebarAudioButton`, `roomRemoveButton`, `roomSidebarHoverArea`, `settingsButton`（若与轨设置合并，则更新唯一归属） |
| RoomTile | `roomControlsTimer`, `roomCardSurface`, `roomPrimaryIndicator`, `danmakuWaitingLabel`, `roomTopBar`, `roomTopBarRule`, `roomTopMetadata`, `roomStatusLabel`, `primaryRoomBadge`, `roomViewerLabel`, `roomTopActions`, `roomControlMenu`, `refreshRoomAction`, `roomBottomBar`, `roomBottomBarRule`, `roomAnchorName`, `roomTitleText`, `roomTitle`, `roomAvatarImage`, `roomActionBar`, `roomActionDivider`, `roomVolumeSlider/roomVolumeSliderHandle`, `primaryRoomAction`, `roomAudioButton`, `danmakuRetryAction/danmakuConnectedIndicator`, `roomQualitySelector/roomQualityLabel/roomQualityPopup/roomQualityPopupList/roomQualityOptionLabel`, `roomPictureDoubleClickArea` |
| 既有独立面板/对话框 | `danmakuSettingsPanel`, `monitoringDrawer`, `workspacePresetsPanel`, `addRoomDialog`, `notificationSettingsDialog`, `closeBehaviorDialog`, `teamManagerDialog`；原各输入/取消/确定/保存对象名保持 |

`headerSidebarIcon`、`headerPrimaryActionsDivider`、`headerPlaybackActionsDivider` 等纯旧头部几何可随设计删除，但它们在图标/分组测试中有引用，必须同步改成新真实入口/分组测试。不要要求新36px头部继续容纳旧十余按钮。

## 5. 焦点、Escape、输入与播放器风险

1. **抽屉父级与实例。** 覆盖容器不参与主布局；只开一个观看子面板或明确关闭父抽屉后再开既有面板。既有 `reusesSurfacesAcrossRepeatedViewSwitches` 要求设置/榜单/三面板对象唯一且复用，见 [qml_interaction_test.cpp:2243](/D:/DouyuMonitor/native/tests/qml_interaction_test.cpp:2243)。此处应优先保留现有常驻实例，不能机械套用“所有弹层都用 Loader 销毁”。
2. **焦点返回。** 打开时记触发器；关闭后先验证它仍存在、可见且有效，再 forceActiveFocus；沉浸边缘若暂隐，先显露再回焦点。进入全屏前关抽屉，聚焦观看容器；退出回标题栏工具按钮。不要聚焦 tile 来制造永久播放条。
3. **Escape 优先级。** 先关闭最上层 Dialog/Popup/Drawer/房间菜单，再退出全屏。当前 [Main.qml:591](/D:/DouyuMonitor/native/app/qml/Main.qml:591) 是 ApplicationShortcut，未检查弹层；不能保留原条件直接叠新全屏工具，否则一次 Escape 可能跨两层。保留 CloseBehaviorDialog 的取消语义。
4. **T 与输入。** 当前 [Main.qml:171](/D:/DouyuMonitor/native/app/qml/Main.qml:171) 只按五个 objectName 判断输入，漏掉 `guildNavigationSearch`、榜单搜索和活动名称等。新增单字母快捷键必须使用实际可编辑/输入法状态或明确 scope，弹层打开时禁用；保留 Ctrl+Shift+A/D/M/W/S/R、F11，Ctrl+Shift+S 改为房间抽屉开闭。
5. **自动隐藏与键盘焦点。** [RoomTile.qml:87](/D:/DouyuMonitor/native/app/qml/components/RoomTile.qml:87) 目前只保护菜单、画质 popup 和 slider pressed；不能隐藏正在键盘操作的子控件。但旧测试明确允许 tile 本体保留鼠标焦点后隐藏，见 [qml_interaction_test.cpp:3733](/D:/DouyuMonitor/native/tests/qml_interaction_test.cpp:3733)。区分“FocusScope 本体活动焦点”与“子按钮/输入持焦点”，不要简单 `if (root.activeFocus) restart()` 导致永不隐藏。
6. **透明不等于不可交互。** 顶/底控件条当前仅 opacity=0；新增遮挡修复应确保隐藏控件不会拦截画面鼠标或造成不可见 Tab 停点，同时提供键盘进入时显露路径。保护 `roomPictureDoubleClickArea` 防止点菜单/画质时误触全屏。
7. **真实状态与热区。** 常态不新增常驻说明；等待播放/离线/异常/弹幕排队状态按实际业务显示。全屏边缘 hover、键盘、T 和触摸各有路径；焦点在显露入口内部时不能自动隐藏。读屏与触摸仍需真实设备验证。
8. **生命周期。** 导航/模式/抽屉不改 `renderEnabled`；它属于已有后台和播放器生命周期。保留 `onRoomIdChanged` 重绑、移除延迟、预设延迟应用、关闭先停止弹幕再退出服务。QML 几何变化不能算真实播放验证。
9. **容量与布局。** 保留领域当前容量和双主资格；本轮不统一9/10限制。不把网页 `主画面 + 底列` 新增为未实现的 `setLayout` 值；显示可解释为真实现有布局，但底层三枚举不变。

## 6. 要保留、要更新和要新增的测试

| 类型 | 当前证据/调整 |
| --- | --- |
| 更新旧几何 | [qml_visual_smoke_test.cpp:348](/D:/DouyuMonitor/native/tests/qml_visual_smoke_test.cpp:348) 硬断言 header=52/sidebar=268；改成36px顶栏/80px轨，抽屉关闭时0常驻占位；三尺寸仍检查边界、无重叠、工具不压网格 |
| 更新旧入口/父对象 | `placesSidebarToggleBeforeBrandAndOpensHistory`, `placesNavigationEntryAfterSidebarToggleAndBeforeBrand`, `usesGroupedHeaderControls`, `groupsHeaderActionsWithStableTooltips`, `groupsSoundControlsAndExposesFullscreen`, `opensMaoziRankPageFromHeader`；改用导航轨/观看工具的真实点击链，保留命令结果断言 |
| 保留布局行为 | [qml_interaction_test.cpp:3007](/D:/DouyuMonitor/native/tests/qml_interaction_test.cpp:3007) 只允许 auto/primary/primary-two；双主至少4房；5房3+2与主画面全高几何；`qml_visual_regression_test::keepsLayoutMatrixContainedAndNonOverlapping` 仍执行 |
| 保留导航业务 | `checksRankVersionOnlyWhileGuildNavigationIsVisible`, `updatesGuildStatusWithoutRecreatingRows`, `closesGuildHoverWhenNavigationHidden`, resolved/manual room加入、分队/筛选顺序；迁入全页后继续断言隐藏时停止查询 |
| 保留声音/房间操作 | 单音焦点 vs 多轨静音、错误 Toast、volume、画质 rate映射与实际列表、历史/收藏/排序边界、延迟移除、预设应用和模型刷新 |
| 保留弹层/持久化 | [qml_interaction_test.cpp:2000](/D:/DouyuMonitor/native/tests/qml_interaction_test.cpp:2000) Escape取消、焦点与输入保留；`reusesSurfacesAcrossRepeatedViewSwitches` 的对象身份与唯一性；保存失败重试和关闭偏好；预设保存/删除/应用 |
| 日间适配 | 当前 `keepsTransientDialogSurfacesDark`、`keepsInputAndMenuControlsOnDarkTheme`、`eventMappingPopupsUseDarkTheme` 及 [qml_controls_style_test.cpp](/D:/DouyuMonitor/native/tests/qml_controls_style_test.cpp) 不能继续把全部模式强制断言为暗色；夜间保留原语义，补白天/禁用/聚焦/菜单对比与样式覆盖 |
| 新增R3交互 | 标题栏工具→房间/布局/声音→返回；主题/抽屉开闭前后同一 WorkspaceGrid/RoomTile/MpvQuickItem；全屏进入关抽屉、焦点落观看区；T输入保护、Escape先关再退出、边缘hover/Tab/Enter/回焦点；聚焦子控件不自动隐藏 |
| 原生窗口/播放器 | 完整 [qml_close_regression_test.cpp:103](/D:/DouyuMonitor/native/tests/qml_close_regression_test.cpp:103) 双击、全屏前后最大化恢复、系统最小化同步、记住选择行、取消关闭、attached player关闭/移除、预设重绑。使用Windows后端，不能删断言回避offscreen差异 |

新增 QML 与图片必须同时注册生产 `qt_add_qml_module` 和各测试 qrc 清单，不能只改一个列表：入口在 [CMakeLists.txt:155](/D:/DouyuMonitor/native/CMakeLists.txt:155)，测试资源分别在243、313、419、511附近。可新增一组共享 `DOUYU_QML_SHELL` 变量最小消除本轮新增项的重复，但不做无关构建重构。保持 `Theme` singleton标记与 [qmldir](/D:/DouyuMonitor/native/app/qml/qmldir)。

## 7. 建议实施顺序与验收命令

1. 建立当前脏工作区快照/定向diff记录，先补新36px/80px与抽屉几何、节点稳定、T/Escape焦点的失败测试。
2. 图片资源与 Theme双模式→精简 AppHeader +80px轨→Main常驻网格重锚点→搬运RoomSidebar及观看工具→全屏边缘和焦点→日间控件/旧测试入口调整。每段只改自身必要部分。
3. 主题持久化若实现，单独补AppController/QSettings测试；旧工作区侧栏恢复规则写入测试，不隐式迁移。
4. Build/CTest通过后在真实Windows窗口检查三尺寸、日夜、1/4/5/9/双主布局与真实播放/关闭；截图与网页作视觉对照，但不把网页113项代替QML验收。

在已验证的 VS x64 / Qt / MPV 环境中，按实际可用 preset 执行：

```powershell
cmake --build --preset windows-x64-release --target douyu_monitor_native qml_engine_smoke_test qml_interaction_test qml_visual_smoke_test qml_visual_regression_test qml_controls_style_test qml_close_regression_test
ctest --preset windows-x64-release -R "qml_(engine_smoke|interaction|visual_smoke|visual_regression|controls_style|close_regression)_test" --output-on-failure
```

若触及主题/持久化C++，再构建并运行 `app_controller_test`；触及工作区字段或恢复策略，增加 `native_workspace_store_test`。命令只是计划，本轮未执行，不声称构建/测试通过。

## 8. 本映射核验范围

已定向读取五个指定QML、WorkspaceGrid/WindowControls/GuildNavigation、控件、AppController/model头、相关持久实现和测试；核对当前源码而非仅复用旧审计。主代理专属Serena未初始化。本文件链接在交付前检查存在且行号有效；生产文件无本子任务写入。外部Qt API的具体实现细节需主代理按当前Qt版本官方文档确认。
