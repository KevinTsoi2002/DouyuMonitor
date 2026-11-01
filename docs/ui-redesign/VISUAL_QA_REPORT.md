# 视觉评审与原生验证记录

## 2026-11-01 四项反馈：当前验证

新编译原生视觉烟测35/0（71.686秒），四尺寸日夜矩阵重新生成。真实网络头像另由生产MaoziRankPage夹具逐行验证排行46/46、定级赛36/36及美伢详情Ready，3/0；已查看`native/out/verification/four-rank-real/`的日夜榜单、详情和46图联络图。完整交互115/0包括顶部五个按钮各650ms悬停无提示、头像失败回退和多轮筛选身份保持；键盘焦点提示保留。

导航性能为真实62人、空头像以隔离网络的源QML夹具，9次同步6–147ms→5–10ms，不称全链路输入到呈现耗时。视觉矩阵使用可重复品牌图夹具，真实头像验证分开记录。未修改基线图片，未以占位视频证明真实颜色/长播。细节见[四项修复](FOUR_ISSUE_REPAIR.md)。以下段落保留各历史时点。

## 九项截图修复：2026-10-31历史视觉审查

四尺寸（960×600、1280×800、1366×768、1920×1080）×昼夜主题，166张原生PNG与8份来源清单全部尺寸一致。主代理查看夜间四组拼图及代表原图，独立审查查看日间四组与10张关键原图；导航筛选、榜单、九维雷达、四队布局、赛事回合、足球事件/MVP与原文入口无新增重叠或关键内容丢失。滚动详情上下边缘裁剪属于视口状态，配对顶部/下部图及滚动测试覆盖完整内容。

18张基线经旧图备份与人工对比后限定更新：14个layout、房间菜单、清晰度弹窗、2个导航图。阈值未放宽；最后视觉回归在未启用更新模式下通过。旧图备份native/out/verification/nine-issue-baseline-backup，对比报告nine-issue-visual-comparison/REVIEW.md。24路旧有姓名截断/细长头像占位不属于本轮新增问题。

画内控件在白天模式保持独立深色；清晰度闭合/弹出文本居中。没有更改真实解码色彩。头像矩阵为品牌图夹具；真实网络四图和静态覆盖34/46、26/36分开记录，不能视为全部真实头像已验。各测试、EXE及限制见TEST_VALIDATION_LOG.md最新节。Notion实际链接和同步进度见NOTION_REMOTE_SYNC.md。

更新：2026-10-31，Asia/Shanghai（UTC+08:00）。下方 R1–R3 网页记录属于历史评审证据；本节新增原生 Qt 阶段结果，仍不构成最终视觉或功能验收。

## 上一轮六项截图问题：历史视觉证据（2026-10-31，Asia/Shanghai）

本轮已实际修改标题栏图标、主播悬停、下拉选中对比、重复添加入口、观看工具层级及编译后的日夜文字绑定。实施细节、最终集成状态见 [SIX_ISSUE_REPAIR.md](SIX_ISSUE_REPAIR.md)。下面“历史复核补充”和 R1–R3 保留各自当时的事实，不作为本轮状态。

- 控件 Windows 原生验证 13/0；生产编译模块主题往返验证 5/0。已经逐张打开 `native/out/build/ui-redesign-baseline-qt612-nmake/verification/theme-controls/compiled-day.png`、`compiled-night.png` 和 combo-day/night：图标、用户字标、顶部工具、空态按钮及下拉选中项颜色正确。源码和 AOT 证据覆盖五轮主题状态；单纯 Token 计算未被当成实窗验证。
- 最新视觉烟测 35/0，截图矩阵覆盖 960×600、1280×800、1366×768、1920×1080 与两主题，76 张原图和 8 份来源 JSON 位于 `native/out/verification/qml/repair-*`。已查看汇总图、最小窗口白天空态/观看/抽屉/筛选/榜单、夜间声音/布局、1366 工具总览，以及 1280 主播悬停全图。矩阵采用已有 reducedMotion 以避免主题颜色动画中间帧，重建后通过最终 CTest；重新查看全部缩略汇总和最小白天观看/筛选/榜单原图，尺寸校验 0 不符，见 `six-issue-image-validation.json`。
- 视觉回归 7/0，日志 `native/out/verification/six-issue-visual-regression-final.txt`。逐张查看单面板布局启用/禁用、整行悬停及指标卡，保留现有键盘焦点；仅将对应三张预期变化图作为新基线。旧图先保存到 `native/out/verification/six-issue-baseline-backup/`，未批量重写其他基线，复测无更新变量。
- 生产程序 `--self-test` 退出 0，Qt Quick renderer 通过；这次未使用媒体文件，不代表真实播放验收。
- 最终六目标集成 CTest 6/6 通过，136.84 秒；完整交互单独运行 109/0。普通环境下无额外 Qt 设置的启动自测也退出 0。详见 `native/out/verification/six-issue-ctest-final.txt`、`six-issue-interaction-final.txt`、`six-issue-app-default-env.txt`。

原图没有顶部后绘标注；离线名册和 S1 归档来源记录在 JSON，不代表实时直播。榜单本轮做布局复核与既有列对齐验证，不声称重新完成榜单产品设计。未验证部分：真实斗鱼长时播放、设备/DPI/安装包、完整 Figma、Notion 远端同步。测试仍保留引擎销毁时异步项警告。

## 历史复核补充：六项修复前（2026-10-31）

较早段落记录的交互 103/1、AutoMoc spawn 失败和关闭测试 QProcess 权限错误，均已由 [TEST_VALIDATION_LOG.md](TEST_VALIDATION_LOG.md) 的后续验证更新。其指定的引擎烟测、视觉烟测、视觉回归、交互与关闭回归 5/5 通过；质量弹窗定向测试 3/0。没有运行完整 CTest，也没有据此宣称产品视觉验收完成。

原生 R3 离线截图是视觉夹具，不代表实时直播。截图测试的 saveOfflineScreenshot() 会在图片顶部绘制深色离线说明带；该带是后处理标注，尤其会让 C3 白天截图看起来像标题栏仍是暗色。它不是生产 UI 的颜色结果。白天字体颜色需用不带该标注的原生截图和实际 Windows 运行状态复查。

用户新增的白天字体颜色、观看工具交互、标题栏主图标、榜单布局问题尚未完成修复或验收，具体证据和验收条件见 [PLAN_REVIEW.md](PLAN_REVIEW.md)。当前最新状态以 [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md) 为准。

## 原生 Qt 阶段：历史证据与截图记录

隔离构建目录为 `native/out/build/ui-redesign-baseline-qt612-nmake/`。`build-final-pass2.txt` 记录应用及相关 QML 测试目标成功构建。`visual-smoke-final-20261031.txt` 为 33 passed / 0 failed，包含 960×600 等视口约束、房间信息位置及双主题截图矩阵；运行有 QML 引擎销毁时 item 尚在创建的警告。`controls-final-pass2.txt` 为 12 passed / 0 failed。

`native/out/verification/qml/` 中存在 30 张 `r3-{night,day}-{1280x800,1366x768,1920x1080}-{watch,tools,navigation,ranking,settings}.png`，数量和尺寸已核对。生成截图不等于全部人工审查；尚无 960×600 白天矩阵图。长中文、表格横向与详情纵向滚动、菜单边缘、弹窗、焦点及实际视频内容仍需最终视觉复核。

下列是已被后续验证替代的历史结果：`interaction-final-pass3.txt` 曾为 103 passed / 1 failed；失败在 `keepsControlsVisibleDuringQualityPopupInteraction()` 的控件可见断言。随后 `build-interaction-final3.txt` 曾因 `moc` 子进程 spawn 权限失败。2026-10-31 后续构建与选择性 CTest 已通过 5/5，质量弹窗专项为 3/0，关闭回归中的 `QProcess CreateFile` 权限失败未复现；具体以 [TEST_VALIDATION_LOG.md](TEST_VALIDATION_LOG.md) 为准。更早的 `integrated-core-ctest.txt` 仍是旧快照 3/4 结果，不代表当前完整 CTest 状态。当前选择性回归通过不等于原生统一 QA 完成，也没有关闭用户报告的四项 UI 专项验收。

Figma 的可编辑文件和部分组件存在，但 Starter MCP 配额阻塞后续组件、Screens 页面和品牌导入；Secondary Disabled 截图疑点未复核。Notion 无可调用连接器，本地 [NOTION_SYNC_QUEUE.md](NOTION_SYNC_QUEUE.md) 不是远端同步证据。

## R3：网页历史验证结果

用户截图中的scene-caption与底部watch-dock已删除，常态工具入口在原有标题栏。日夜模式已固定C1/C3。

最新尺寸微调（2026-10-30 01:39）：按用户“适当缩小”反馈，仅将桌面字标容器240×28px改为210×24px；窄窗口和图片文件未变。Playwright重新检查三桌面尺寸×日夜共6组合：资源对应正确且加载成功、contain保留比例、无标题裁切/重叠/横向溢出，标题栏36px，观看区与前版完全相同。结果见[r3-wordmark-small-results.json](visual-review/r3-wordmark-small-results.json)。夜间/白天标题截图已实际查看，完整截图见[r3-wordmark-small-c1-watch-1366.png](visual-review/assets/r3-wordmark-small-c1-watch-1366.png)和[r3-wordmark-small-c3-watch-1366.png](visual-review/assets/r3-wordmark-small-c3-watch-1366.png)。此次未重跑未修改的窄屏、交互或原生Qt；下文240px记录为前版历史。

最新字标替换（2026-10-30 01:32）：用户提供的深色图片用于C3、浅色图片用于C1，替换下文历史字体版。Playwright实际执行8组合（3个指定桌面尺寸及390×844，均含日夜模式），验证每种模式只显示对应图片、资源加载、等比contain、标题无裁切重叠、标题栏36px、无横向溢出与pageerror。日夜容器位置相同，视频节点未重建，桌面观看区与此前尺寸一致；观看工具可打开，刷新后恢复白天字标。测试结束恢复浏览器主题与视口。[完整工具结果](visual-review/r3-wordmark-results.json)已保存。实际查看[夜间标题](visual-review/assets/r3-wordmark-c1-detail.png)和[白天全图](visual-review/assets/r3-wordmark-c3-watch-1366.png)。原图与派生过程见[品牌资产记录](ASSET_REPORT.md)。本次未重跑113全量交互、未实施或验收原生Qt。

2026-10-30 标题字体追加调整：index.html为标题添加app-title类，review-r2.css仅局部调整字体。桌面18px/24px行高、700px以下16px/22px；紧凑模式收起评审原型说明，标题与工具均保留。Playwright检查1280×800、1366×768、1920×1080、390×844各自C1/C3共8组合，全部无标题栏裁切、控件重叠、横向溢出、缺图或pageerror；标题栏仍36px，三个桌面观看区与此前完全相同。浏览器实际字体查询返回DengXian-Bold（4个中文字形）和Bahnschrift-Bold（5个拉丁/空格字形），均为本机字体。已查看[夜间标题细节](visual-review/assets/r3-title-c1-detail.png)和[白天完整画面](visual-review/assets/r3-title-c3-watch-1366.png)；另保存夜间全图与白天标题细节。主题及视口恢复。本步未重复全量交互检查，未作原生Qt字体/分发验收。

2026-10-30 01:05 侧栏追加调整：仅在review-r2.css的桌面媒体规则内放大字标和图标。Playwright重新检查1280×800、1366×768、1920×1080与C1/C3的6个观看组合：字标16px/20px行高、品牌图标40px、线性图标26px，侧栏仍80px；无导航内部溢出、上下菜单碰撞、页面横向溢出、缺图或JS错误。观看区分别保持1158×599、1244×567、1782×879，与此前记录一致。夜间/白天1366截图已实际查看：[夜间](visual-review/assets/r3-sidebar-c1-watch-1366.png)、[白天](visual-review/assets/r3-sidebar-c3-watch-1366.png)。浏览器主题与视口已恢复。下表30组合和113交互为侧栏调整前的R3完整验证；本次CSS修改进行了上述6组合专项检查，没有将旧测试当作本次重跑结果。700px及以下紧凑规则未改，本次未重复验证。

| 检查 | 实际结果及证据 |
| --- | --- |
| JavaScript语法 | review.js、check-r3-matrix.js、check-r3-interactions.js，node --check退出0 |
| 桌面矩阵 | 1280×800 / 1366×768 / 1920×1080 × C1/C3 × 五页，共30组合；无横向溢出/缺图/检查控件内部溢出；标题栏36px、工具在视频区域之外；[结果摘要](visual-review/r3-matrix-results.json) |
| 交互回归 | [实际完整结果](visual-review/r3-interaction-results.json)：113/113通过，0条pageerror；模式/偏好/视口均恢复。覆盖抽屉几何、视频节点身份、第五房、音频弹幕、模式切换、持久化、T/Tab/Enter/Escape和边缘工具焦点 |
| 独立视觉检查 | 9张日夜核心PNG，三尺寸，无新P0/P1；没有用R1或R2旧截图代替 |
| 真实截图 | assets/r3-c{1,3}-{directory,ranking,watch}-{1280,1366,1920}.png，共18张；另有工具抽屉、沉浸边缘展开/收起截图 |

修复过程：矩阵脚本的导航选择器双命中，限定nav-item后通过；交互测试改为等待原生close事件和实际hover后读取隐藏控件文字。运行中发现边缘工具关闭后焦点无法回到隐藏触发器，产品关闭回调先显露入口再回焦点，完整重跑113项通过。失败轮次没有计入通过数量。

画面面积只作为当前布局测量，不声称与R1对比提升百分比：1366×768下观看区1244×567，四个scene合计672379.97 CSS平方像素，16:9完整图像合计632349.20。黑边未计入图像；该值不是实际直播有效内容或原生性能指标。抽屉打开前后网格边界保持相同，节点未重建。

保留范围：单路控件在hover/focus时仍覆盖画面边缘，辅助抽屉只在主动打开时覆盖右侧；不是所有状态永远零遮挡。实体触摸、读屏器、Windows DPI、原生Qt及真实播放未执行。1280榜单辅助字较小，需在Qt阶段按DPI验证。

审批记录：本轮部分正常Playwright请求首次超时，按工具许可各重试一次后获准执行。未切换端口或浏览器绕过。此前R2的未执行结果仍保留为历史。

## R2：历史验证边界

本轮固定 C 组件，增加三个绿色系候选与画面优先观看结构。

| 检查 | 实际结果 |
| --- | --- |
| review.js / check-r2.js 语法 | node --check 退出0 |
| 独立源代码复核 | 确认配色不调用renderPage、观看状态局部更新、Escape先关闭dialog；修正无脚本旧候选标签和移除反馈文案 |
| CSS 静态复核 | 配色、抽屉和沉浸选择器存在；非浏览器结果，不证明布局正确 |
| Playwright导航 | 正常MCP调用打开原8899地址且标题正确；发生在新CSS保存之前，有1条缺失样式资源错误，不能作为最终渲染验收 |
| check-r2.js矩阵与交互 | 首次自动审批超时；工具允许一次重试，同一请求再次超时。检查没有执行，没有通过数或面积结果 |
| 新截图/独立视觉复核 | 尚无R2截图，未执行；没有使用旧截图代替 |
| 原生Qt | 本轮未修改和运行，无新增验收结论 |

检查脚本 [check-r2.js](visual-review/check-r2.js) 已准备：三桌面尺寸×三配色×五页、抽屉前后几何和节点身份、焦点/Tab/Escape、配色保留状态、隐藏控件不可Tab、第五房、R2偏好、榜单状态。画面指标区分scene容器与16:9 contain有效图像，另算工具条重叠。所有这些运行断言均待批准后执行，不写作通过。

待明确批准继续同一Playwright检查；不通过其他浏览器、端口或执行路径绕过。Notion无可调用接口，继续记为待同步。

## R1 历史验证（2026-10-29，以下结果仅适用于归档版）

## 实际执行结果

| 检查 | 证据 | 实际结果 |
| --- | --- | --- |
| JavaScript语法 | `node --check docs/ui-redesign/visual-review/review.js` | 退出0 |
| 浏览器加载 | Playwright打开 `http://127.0.0.1:8899/` | 页面标题正确，HTML/CSS/JS及3图标返回200/304；无远端业务请求 |
| 桌面布局矩阵 | [check-matrix.js](visual-review/check-matrix.js)、[matrix-results.json](visual-review/matrix-results.json) | 最终45组合全部满足检查条件；无body/main横向溢出、关键控件内部溢出、缺失图像；9组观看页控制栏在可见范围，main无纵向溢出 |
| 交互回归 | [check-interactions.js](visual-review/check-interactions.js)、[interaction-results.json](visual-review/interaction-results.json) | 最终106断言通过；临时偏好在测试后恢复，当前无已记录偏好 |
| 中文组合输入 | 浏览器合成compositionstart/input/compositionend事件 | 组合过程中节点保留；提交“北屿”后仅1行，输入值正确。没有实体Windows输入法验收 |
| 控制台 | browser_console_messages | 最终矩阵后复核0 error、0 warning |
| 纯逻辑缺陷回归 | 独立审查代理Node VM | 原6项先失败后通过，另17项通过；不能替代浏览器证据 |
| 独立视觉审查 | 新上下文代理仅看PNG | 三套结构差异成立；修复复核后无阻止方向比较的P0/P1视觉问题 |

桌面矩阵为：1280×800、1366×768、1920×1080 × A/B/C × 主播导航/榜单/观看/设置/组件。每种方案分别设置对应light/dark媒体及reduced-motion；候选本身由显式方案切换控制。

交互断言包括：方案切换、姓名与房间搜索、空结果、开播与队伍组合筛选、成员详情、待确认房间守卫、模拟加入、加载/空/错误及重试、榜单查询/详情/缺失值、五标签、四路构图、弹幕与静音、布局、四组设置、关闭与导入预览、本地偏好刷新保留、斜杠聚焦搜索、Tab弹窗循环、Escape和焦点返回。

## 缺陷与修复

| 级别 | 发现 | 修复及复核 |
| --- | --- | --- |
| P1 | A/C选中行辅助文字对比4.317/4.383，低于4.5 | 改用muted后重新计算为5.837/6.146；浏览器截图查看选中行 |
| P1 | B蓝色焦点圈在暗色视频上不清晰 | 视频专用亮色焦点圈对比12.197；106交互检查含键盘循环 |
| P1 | 矮窗口主视频控制栏溢出 | 收紧观看页头和最小高度；最终9组watch检查全部完整可见 |
| P1 | B音频设置入口在侧栏底部裁切 | 移到标题下；独立1366截图复核完整显示 |
| P1 | B/C示意画面比例误导 | SVG内容固定16:9、contain保留黑边；独立截图确认无拉伸/裁切 |
| P1 | 弹窗Tab可能到BODY | 浏览器复现后加入首尾循环；最终106断言全部通过 |
| P1 | 组合输入期间重建输入框 | composing阶段保留节点，结束后刷新；合成IME浏览器检查通过 |
| P2 | 选中卡片/榜单行缺少明显对应 | 增加背景、边框或左条；独立截图复核 |
| P2 | 弹窗导航未关闭、第五房不显示、赛事筛选/定级状态/音频状态不一致 | JS范围修复及6+17项VM检查；106浏览器覆盖主要流程；以下补充边界脚本未取得执行结果 |
| P2 | 小屏图标按钮缺名称 | 添加固定aria-label；静态核对 |

## 视觉审查结论

使用5-Second观察页面/当前对象/主操作，Squint观察结构与视觉权重。独立审查先看1280九张核心图与1920三张榜单，修复后复看1366的B/C观看、B导航、A/C榜单。最终矩阵再生成全部27张核心截图。

声明元素核对：表面分层与低阴影[呈现]；控件几何、连续行和数字对齐[呈现]；主播、历史赛季和多路观看领域结构[呈现]。详见 [VISUAL_REVIEW.md](VISUAL_REVIEW.md) 的来源声明。Register为product，不执行品牌营销页相似度测试；领域特征检查通过。没有循环动画，无需动画性能结论。

仍保留的P2取舍：B四格与C底部画面在1366窗口中的实际16:9内容较小，真实游戏文字可能难读；宽屏榜单姓名与队伍之间距离偏大。它们不阻止选择方向，但必须在后续Qt真实内容中验证，不能视为已完成最终设计。

## 未执行及工具限制

- [check-edge-cases.js](visual-review/check-edge-cases.js) 的综合调用多次遇到自动审批超时，未取得执行结果。超时没有说明动作不安全，也不算测试通过。缩小检查后，合成IME取得了上述真实结果。
- 390×844与960×600补充矩阵、无JavaScript运行、实体输入法、DPI、浏览器跨引擎及真实Qt窗口未完成验证。页面有静态无JS概览与窄屏降级CSS，仅作源码证据。
- 真正的视频、声音、弹幕、设置持久化、服务通信、全屏/托盘/关闭及真实斗鱼长时间运行没有在网页中验证。
- 原生旧基线四项测试均加载失败，0xc0000135，0/4；没有完成新的Qt6.12构建。详见 [MCP_SETUP.md](MCP_SETUP.md)。
- Notion没有可调用接口，所有记录仍待同步；没有正式Figma页面或生产QML修改。
- 展示工具：open_in_codex两次返回MessagePort错误；内嵌浏览器随后自动审批失败，429 Too Many Requests。该打开动作未执行，未绕过，使用已验证的URL与本地截图交付。

## 截图索引

九宫格：[candidate-comparison.png](visual-review/assets/candidate-comparison.png)。原尺寸截图位于 `visual-review/assets/qa-{a,b,c}-{directory,ranking,watch}-{1280,1366,1920}.png`，共27张。

R1 历史检查点状态：当时网页候选供用户审阅，正式设计与生产实施等待用户确认。该关卡已于 2026-10-30 通过；当前原生阶段状态以本文首节为准。
