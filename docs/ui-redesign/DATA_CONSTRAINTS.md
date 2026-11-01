# CSTG UI 重设计：数据与业务约束

审计日期：2026-10-29。适用范围：网页评审原型、后续 Figma 设计和 Qt Quick/QML 实现。

本文件是当前源码与内置资源的只读审计。证据位置以本次审计时的仓库文件与行号为准；后续代码变更可能使行号移动。当前版本源为 `native/CMakeLists.txt:3`，版本号为 **0.2.20**。不能将本文件中的历史资源数量、示例或设计建议描述为真实斗鱼运行验证结果。

## 1. 原型与后续实现的约束

- 网页原型中的模拟房间、人数、状态、评分等应明确标注为“占位示例”。只有直接引用并注明来源的档案值才能标为 S1 归档数据。
- 分别标注“实时/缓存房间状态”“线上榜单快照”“S1 历史档案”。它们来自不同模型，刷新周期和失败行为也不同。
- 缺失值使用“—”“未记录”或“未知”等准确状态，不补造零分、投票数、头像、直播状态、趋势或统计。
- 保留原生 Qt、C++、libmpv、独立 StreamGet 服务的产品边界。HTML 是评审与交互说明载体，不意味着迁移到网页播放器。
- 页面只通过已有 `AppController` 和只读模型表达业务动作。后续 QML 不能复制领域状态或绕过房间、播放、音频、弹幕和保存生命周期。
- 不在原型、截图、文档、日志或测试快照中加入真实播放 URL、查询参数、Cookie、Token、签名、认证帧或原始服务诊断。

## 2. 三类数据来源

| 来源 | 当前实现与证据 | 时间语义 | UI 约束 |
| --- | --- | --- | --- |
| S1 历史档案 | `native/src/app/s1_archive.cpp:25` 校验；`:78` 读取 `:/archive/cstg_s1_archive.json`；`native/src/ui/app_controller.cpp:611` 提供静态 `S1Archive::bundled()` | 内置文件记录赛季结果，采集日期 2026-10-27 | 标注赛季和归档日期；刷新按钮不能暗示重新抓取赛季结果 |
| 线上 Maozi 榜单 | `native/src/app/maozi_rank_client.h:16`；`maozi_rank_client.cpp:209` 网络客户端 | 匿名会话中的线上快照，带 `updatedAt`、同步状态和版本检查 | 可失败或过期；线上数值不应悄然替换历史成绩 |
| 公会成员房间状态 | `native/src/ui/app_controller.cpp:479`；`native/src/workspace/guild_room_resolver.cpp:196` | 固定成员身份 + 缓存房间映射 + 独立在线状态查询 | 解析状态、在线状态、工作区加入状态必须分开 |

`native/app/qml/pages/MaoziRankPage.qml:14` 优先读取内置档案，只有档案对应部分缺失时才回退线上榜单。`native/app/qml/Main.qml:129` 进入该页仍会调用线上 `maoziRank.refresh()`；这一调用不能作为“页面归档是实时数据”的依据。

## 3. S1 档案的数据面与缺失值

资源为 `native/app/resources/cstg_s1_archive.json:1`。JSON 采用单行存储；本次通过 JSON 解析核对结构和数量，没有复制整行资源内容。

| 部分 | 已核对数量 | 实际字段或子结构 | 使用限制 |
| --- | --- | --- | --- |
| 档案元数据 | version 1；season `CSTG S1`；capturedAt `2026-10-27` | `version/season/capturedAt/sourceUrl` | `sourceUrl` 是公开赛季来源信息；未进行站外抓取验证 |
| 排行榜 | 46 名参与者 | `dimensions/grade/id/name/rank/roomId/score/team` | 排名和评分是已存赛季结果；没有档案投票数、头像或实时开播状态 |
| 定级赛 | 36 名参与者，6 个场次 | 参与者 `id/name/overall/stats/team/tier`；场次 `day/slot/text` | 页面逐人总评取 `overall`，各场次得分取 `stats`；不可自行推算未记录分数 |
| 常规赛 | 31 场 | `id/date/title/note/detail`，另有 `overview` | 详情主要为档案文本；不是完整结构化比赛事件数据库 |
| 决赛 | 4 场 | `id/date/title/detail`，另有 `overview` | 同上；不得从文本推导新的实时赛况 |
| 足球赛 | 10 场 | `id/summary/detail`；另有 `overview`、`views`、`leaderboards` | `views` 有 matches/stand/tree；球员榜有 goals/assists/mvp |

归档加载器还要求各主要部分非空、ID 唯一，并校验定级参与者与排行榜的引用关系（`native/src/app/s1_archive.cpp:25`）。不能把 JSON 能解析等同于全部领域验证通过；本次没有执行 `s1_archive_test`。

排行榜投影明确设置 `voters: "—"`、`live: false`、空头像与空备注，使用存储的 `rank/score/team/grade`（`native/app/qml/pages/MaoziRankPage.qml:101`）。其中 `live: false` 是该归档投影的实现值，产品表达应是“历史归档/不提供实时状态”，不能直接画成“该主播当前离线”。未记录头像可以使用中性占位，不应补上看似真实的主播照片。

定级投影将房间号和头像设为空、排名设为数组 `index + 1`，总评取 `Number(entry.overall)`（同文件 `:151`）。定级明细缺失分数显示“—”（`:168`）。六个场次对应 9 月 28 日至 30 日的下午/晚间（`:127`）。

现有归档页面有五个标签：排行榜、常规赛、决赛、足球赛、定级赛（同文件 `:317`）。在线过滤在归档模式隐藏（`:264`），刷新禁用（`:304`），标题显示归档采集日期（`:240`）。“打开野榜”保留站外入口（`:280`），不代表复制站外投票、登录或编辑功能。

`native/app/qml/pages/SeasonArchivePanel.qml:22` 按日期及标题/摘要查询过滤；`:37` 展示已有详情文本；`:155` 球员榜字段为 `rank/name/team/value`；`:163` 提供可选日期选择；`:206` 每页展示 10 场。常规赛启用日期筛选；足球另有赛程、站位、赛树和进球/助攻/MVP 视图。缺失详情或概览使用“未记录”。

## 4. 排序、评分与线上榜单状态

| 数据 | 当前排序/计算来源 | 证据 | 重设计约束 |
| --- | --- | --- | --- |
| 归档排行榜 | 保留参与者数组顺序及存储的 `rank` | `native/app/qml/pages/MaoziRankPage.qml:83`、`:101` | 未发现表头点击排序实现；新增排序属于行为变更，不能写成已保留功能 |
| 归档定级赛 | 保留参与者数组顺序，显示 `index + 1` | 同文件 `:138`、`:158` | 不把该序号解释为客户端重算的竞技排名 |
| 定级场次列 | 列键排序 | 同文件 `:117` | `keys.sort()` 仅是列顺序，不是选手成绩排序 |
| 线上综合榜 | 分数降序，同分按名称排序，生成 `index + 1` | `native/src/app/maozi_rank_client.cpp:841` | 保留同分行为；缺失分数不补为真实零分 |
| 线上定级榜 | 平均分降序，同分按名称排序 | 同文件 `:762` | 平均分与已评分场次数同时存在 |
| 线上整活/游乐值 | 积分降序，同分按名称排序 | 同文件 `:805` | 字段仍用于导航悬浮信息和兼容，不代表归档页还有该标签 |

线上维度均值是 `sum/count`，无回答时为 `-1`；总评分按各维度回答数加权（`native/src/app/maozi_rank_client.cpp:517`）。评级阈值来自线上配置。不能把雷达维度的缺失值连成真实零分图形，也不能把综合分称为简单维度平均。

线上条目实际字段（同文件 `:568`）：`id/name/guild/roomId/note/posterUrl/teamName/teamId/teamValid/score/grade/gradeColor/voters/live/dimensions/role/playValue/playValueBombed/playValueUpdatedAt/placementAverage/placementScoredSessions`。远端队伍信息优先；缺失时才使用本地队长映射补充，角色由本地活动配置提供（`:552`）。

客户端提供 `loading/errorMessage/totalVoters/updatedAt/statusText/syncPending/lastSyncError`（`native/src/app/maozi_rank_client.h:16`）。版本未变时仅清理同步错误，不重取完整快照；变化或版本缺失时请求新快照（`maozi_rank_client.cpp:429`）。允许保留且已有数据时，失败保留上次结果并标记“数据可能已过期”；否则清空榜单、定级、游乐值、总参与数和更新时间（`:899`）。设计应区分首次加载、无结果、刷新中、保留旧数据但已过期、不可用五类状态。

## 5. 公会导航身份、分组、查询与实时状态

内置名册 `native/app/resources/hamster_agent_roster.json:1` 为 59 人，字段为 `id/name/roomId/pinyinKey`。`native/src/workspace/guild_roster.h:7` 读取为 `id/anchorName/searchName/roomId/pinyinKey`；加载器规范化角色后缀和房间号（`guild_roster.cpp:14`、`:110`）。成员身份来自内置资源，不因在线搜索结果而自动扩张名册。

`AppController::guildRoster()`（`native/src/ui/app_controller.cpp:479`）将三个数据面合成只读投影：

| 字段 | 含义与来源 | 不能混用的语义 |
| --- | --- | --- |
| `id/anchorName/pinyinKey` | 固定成员身份 | 不是搜索结果列表中的临时身份 |
| `roomId/avatarUrl` | resolver 的已确认/手动映射和元数据 | 头像可能为空；房间号可能尚未确认 |
| `status` | 房间解析状态，如 idle/resolving/retrying/resolved/unconfirmed/error | 不是开播状态 |
| `liveState` | unknown/online/offline | unknown 不能显示为 offline |
| `active` | 该房间是否已在当前工作区 | 不是在线、播放健康或音频状态 |
| `role/rankMatched/rankHostId/radarDimensions/score/grade/gradeColor` | 线上榜单按主播名、房间号、规范化名称匹配 | 未匹配时为 other、空维度和 score `-1`；不能补造评级 |
| `placementAverage/placementScoredSessions/playValue/playValueBombed/playValueUpdatedAt` | 匹配后的线上统计 | 与 S1 静态归档不是同一数据源 |

导航搜索仅匹配主播名、房间号的大小写不敏感子串；拼音键用于排序，并没有拼音搜索实现（`native/app/qml/components/GuildNavigationPanel.qml:34`、`:50`）。团长显示一次；本地队伍按保存顺序；队内按队长、队员、其他分层，层内按拼音键和名称排序；未分队最后（`:73`、`:122`、`:143`）。

导航可见时检查榜单版本并刷新开播状态，周期 60 秒（同文件 `:59`、`:308`）；`AppController` 仅在未关闭且导航可见时调用 resolver 刷新（`native/src/ui/app_controller.cpp:1962`）。列表采用增量更新以保留滚动和悬停状态（QML `:160`）。重设计不能通过每次刷新重建整表破坏当前位置。

resolver 的元数据/搜索请求间隔 1200 ms，状态请求间隔 100 ms，重试间隔 3/15/75 秒，已检查开播状态缓存 15 分钟（`native/src/workspace/guild_room_resolver.cpp:11`）。可见期间的定时刷新强制穿透缓存期限（`:196`、`:325`）；不能将 60 秒理解为每个成员都在同一瞬间得到新结果。

resolver 只接受唯一精确匹配，多个候选进入 unconfirmed；无匹配或请求失败进入相应错误/重试路径（同文件 `:254`、`:352`）。不应以模糊搜索结果自动认领主播身份。手动房间映射只接受 1–20 位数字，设置后在线状态恢复 unknown 并安排刷新（`:224`）。映射缓存保存房间身份和检查时间（`native_workspace_types.h:66`），不能把缓存时间误作当前直播时长。

快捷加入必须有确认房间号，且房间未 active、当前工作区尚有容量（`GuildNavigationPanel.qml:272`）。动作由 `AppController::addGuildMemberRoom` 验证并进入正常原生播放流程，继承保存画质、收藏、音量并更新历史（`native/src/ui/app_controller.cpp:774`）。

## 6. 房间、监控、弹幕和容量

房间公开数据面以 `native/src/ui/room_list_model.h:32` 为准：身份、主播、标题、分类、`viewerLabel`、头像、在线状态、播放状态、主/副画面、收藏、音频焦点、请求/有效画质、静音、音量、可用画质、弹幕开关、所属房间分组、弹幕状态/错误码/近期速率/峰值速率/过滤/去重/限流/队列溢出/上游丢弃、是否渲染。

`viewerLabel` 是展示字符串（`native/src/workspace/native_workspace_types.h:24`），本次没有确认它具备可运算的稳定人数单位，不能拿它相加制作“总观众数”。

监控模型只有在线数、离线数、错误数、通知状态和恢复状态（`native/src/ui/monitoring_model.h:7`）。统计来自当前 coordinator 房间快照（`native/src/ui/app_controller.cpp:1968`、`:2010`）：

- unknown 不计入 online 或 offline。
- Error 状态或播放健康 Error 计入错误数；该房间可能同时 online，因此三个数字不是互斥分类，不得相加为总数。
- 通知状态为 disabled/enabled/unavailable。当前 `updateNotificationStatus()` 将恢复状态设置为 Healthy（`:2030`）；不能仅因枚举包含 Recovering/Error 就声称已有全面服务健康诊断。
- 在本次检查的公开 UI 模型中未找到 CPU、GPU、内存、在线时长、趋势或历史性能采样。若设计需要此类指标，应标为未来需求或明确占位，不能暗示已接入。

弹幕展示统计由管理器按房间映射到 presentation（`native/src/ui/app_controller.cpp:2041`）。弹幕优先级先主画面、再副主画面、再音频房间、再其他房间（`:2077`）。这些速率与治理计数不等于全站弹幕量，也不证明长时间稳定性。

| 构建容量 | 布局/默认解码/最高解码 | 默认/最高弹幕会话 | 默认/最高多音轨 |
| --- | --- | --- | --- |
| 默认 Phase16 | 16 / 16 / 16 | 12 / 16 | 4 / 8 |
| Phase24 开关 | 24 / 24 / 24 | 12 / 16 | 4 / 8 |

证据：`native/src/workspace/room_capacity.h:12`、`:25`；`room_capacity.cpp:5`；`native/src/workspace/multi_room_coordinator.h:30`；编译开关 `native/CMakeLists.txt:9`。有效布局的容量由 `RoomCapacity::maxRoomsForLayout()` 返回（`room_capacity.cpp:37`）。UI 应读取 `workspace.maxRooms`；导航的 16 是无模型时回退值，不是跨构建固定业务上限。

## 7. 本地队伍、房间分组与预设

三者不是同一种分组，数据结构见 `native/src/workspace/native_workspace_types.h`：

| 结构 | 键与内容 | 用途 |
| --- | --- | --- |
| NativeRoomGroup（`:50`） | id/name/roomIds | 将直播房间归组 |
| NativeTeam（`:58`） | id/name/memberIds | 给固定公会成员分配本地队伍 |
| NativeWorkspacePreset（`:88`） | layout/activeGroup/primary/secondary/audio/roomIds/sidebar/ratio/audioMode/globalMute/danmaku | 恢复一套工作区设置 |

队伍导入先刷新线上榜单、生成预览。确认条件要求未关闭、预览可用、线上未刷新/待同步/报错，且本地队伍、房间映射身份和榜单身份与预览来源仍一致（`native/src/ui/app_controller.cpp:309`）。过期预览要求重试；候选先保存成功才替换应用内队伍，失败恢复原设置并提示“本地队伍未更改”（`:342`）。

导入算法保护歧义或重复身份，冲突成员不自动重分配；合法匹配可从其他队伍移入目标队伍，不是简单追加。既有队伍以规范化队名匹配，缺失队伍才创建（`native/src/workspace/maozi_team_import.cpp:27`、`:93`、`:132`）。设计必须保留预览、冲突说明、来源变化失效和保存失败反馈。

## 8. 持久化、边界与保留动作

应用显示名为 CSTG特勤终端，技术名称和组织名仍是 DouyuMonitor；QSettings 使用 INI/UserScope，QML 模块仍为 DouyuMonitor（`native/app/main.cpp:142`、`:174`、`:192`）。不要因品牌更新改名这些标识或生成新的用户配置位置。

工作区键为 `DouyuMonitor/nativeWorkspaceV1`，内部 schema 当前版本 8（`native/src/workspace/native_workspace_store.cpp:18`）。快照包含房间库、房间组、队伍、房间映射缓存、预设、弹幕设置、活跃房间、布局、主副画面、音频、侧栏/导航开关（`native_workspace_types.h:106`）。房间库保存画质、收藏、打开时间、音量、弹幕开关与收藏顺序（`:35`），不存媒体源。

加载区分缺失、读取错误、无效、未来不支持版本和成功（`native_workspace_store.cpp:962`）；fromJson 按旧版本处理后进入 normalize（`:876`）。normalize 去重/过滤无效引用，按当前容量限制工作区/房间组/预设，最多 20 个本地队伍（`:207`、`:248`、`:284`）。保存归一化后写入并同步，存在失败及重试路径（`:1005`）。重设计需保留未保存标记和重试入口，不能在 UI 上先宣称已保存。

本地活动角色映射独立存于 `DouyuMonitor/eventMappingV1`，通过临时客户端验证后才提交（`native/src/ui/app_controller.cpp:1912`）。这不是在线榜单写入功能。

以下现有动作应在新信息架构中有明确入口，证据为 `native/src/ui/app_controller.h:118`：

- 房间：手输添加、搜索候选、移除、移除历史、刷新、重试、收藏及收藏顺序、工作区顺序。
- 播放：主画面、副主画面、音频焦点、单/多音轨、单房/全局静音、音量、请求画质与有效画质反馈、弹幕开关。
- 组织：房间组创建/重命名/删除/分配/移除/排序/激活；本地队伍创建/重命名/删除/排序/分配；手动成员房间映射。
- 工作区：布局、主画面比例、侧栏/导航、预设保存/应用/删除。
- 应用：通知选项、更新查询、站外打开、最小化/最大化/全屏、关闭选择、后台托盘、恢复、退出、关闭偏好保存失败重试。

关闭和后台不是单一退出动作。`native/src/ui/app_controller.cpp:1488` 起暂停渲染并处理后台托管；`:1520` 恢复渲染；`:1580` 附近按关闭偏好分支；`:389` 关闭服务与资源。视觉重设计不能用隐藏窗口代替关闭领域流程。

播放与服务边界保持 `RoomSession → RemotePlaybackController → StreamgetProcessClient → streamget_service → MediaSource → MpvQuickItem`。JSONL 服务操作实际包括 ping/resolve/search/status/cancel/shutdown；requestId 必须为正整数，错误使用固定码与 retryable（`native/service/protocol.py:11`、`:65`、`:143`；C++ `native/src/service/stream_service_protocol.h:54`）。错误、取消、超时和重试的呈现应保留边界，不能把原始异常直接放进界面。

## 9. QML 注册、打包与验证边界

生产 QML 模块列表见 `native/CMakeLists.txt:167`。列表包含 GuildNavigationPanel、GuildRankHoverCard、RoomLibraryView、WorkspaceGrid、RoomTile、弹幕与状态组件、设置页、MaoziRankPage、SeasonArchivePanel、TeamManagerDialog；没有 GroupManagerDialog。该文件存在不等于当前运行时已注册。

libmpv 源 DLL 为 `libmpv-2.dll`，部署名为 `mpv.dll`（`native/CMakeLists.txt:43`、`:1140`）。构建另复制 StreamGet 服务、执行 windeployqt 和运行时载荷校验（`:1145`、`:1166`、`:1179`、`:1190`）。安装器展示名为 CSTG特勤终端，AppId、DouyuMonitor 安装技术标识和 `douyu_monitor_native.exe` 保留（`native/installer/DouyuMonitor.iss:12`、`:22`、`:37`）。这些不应成为界面评审时顺带更改的内容。

后续生产改动的验证应与范围匹配：QML 需 engine smoke、interaction；视觉布局加 visual smoke/visual regression；关闭、托盘和播放器生命周期加 close regression 和生命周期测试；持久化加 native_workspace_store/app_controller；协议加双方协议及 Python 服务测试。相关目标已在 `native/CMakeLists.txt:283`、`:353`、`:390`、`:459`、`:559`、`:1031` 注册。不能用网页原型通过替代原生 QML 验证，也不能用构建成功代替测试。

## 10. 当前文档与源码冲突

| 冲突 | 源码事实 | 后续处理 |
| --- | --- | --- |
| AGENTS 的 9/10 路与独立 9 路弹幕说明 | 当前统一容量为默认16/可选24，弹幕默认12/最高16 | 使用当前 RoomCapacity；保留冲突记录，不按旧说明设计硬上限 |
| README `:21`、`:127` 仍描述游乐值标签页、参与人数和开播列 | 当前档案页五标签；归档参与人数“—”，无实时状态 | 页面设计依据当前数据面；后续单独校正文档 |
| `docs/文件职责索引.md:45` 等仍叫仓鼠特工 | 当前用户可见品牌为 CSTG；资源文件名仍可保留旧技术名称 | 仅更新可见品牌，不进行技术标识全局替换 |
| 职责索引 `:57` 描述 GroupManagerDialog | 当前 QML 模块列表未注册该文件 | 不能以索引证明入口可运行，实施前确认实际路径 |
| S1 设计稿要求保留排序（`docs/superpowers/specs/2026-10-26-cstg-s1-archive-and-desktop-fixes-design.md:29`） | 当前排行 QML 未发现表头排序处理，仅有列键排序 | 将排序列为待确认行为增强，避免错误描述已有实现 |
| 导航仍使用线上榜单，归档页也触发 refresh | 展示档案与线上导航评分是两条数据链 | 在文案和时间标签上区分，不能宣称同一时间快照 |

S1 设计文档 `:21` 明确要求赛季结果与实时直播状态分开，`:41` 明确保留 DouyuMonitor 技术标识。这两项与当前实现一致，可作为后续交付约束。

## 11. 本次核对范围与未验证项

已做：定向读取源码、公开模型、CMake/QML 注册清单、安装器展示配置与相关文档；用 PowerShell `ConvertFrom-Json` 核对两份内置资源的字段、版本、日期与数量；通过 scoped `rg` 核对字段使用、排序、状态、保存和容量路径。未读取用户 INI、私有配置或敏感运行日志。

未做：编译、CTest、原生窗口运行、Playwright/E2E、真实斗鱼/线上榜单请求、站外来源核验、安装器生成或安装、长时间多路播放/弹幕、CPU/GPU/内存验收。本文件只对已列证据作源码级结论，不宣称这些运行行为已通过。

本次仅创建本文件；生产代码和其他文档由相应负责人管理。Notion 读取/同步不在本次审计范围内，不声称已读取任何 Notion 内容。
