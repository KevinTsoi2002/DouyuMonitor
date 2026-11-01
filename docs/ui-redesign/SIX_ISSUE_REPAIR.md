# 六项截图问题修复

日期：2026-10-31，Asia/Shanghai（UTC+08:00）。状态：本轮六项修复已完成并通过所列本地验证；最终选择性集成回归 6/6 通过。Notion 远端同步待处理，原整体重构仍有独立未完成项。

沿用用户确认的 C1 夜间 / C3 白天及 C 组件方案。本次修复不改变播放器、数据服务、持久化或榜单统计业务。保护现有工作区修改；不创建提交。

| 用户问题 | 已确认原因 / 修复边界 | 验收 |
|---|---|---|
| 左上角图标缺失 | 标题栏只有图片字标；补独立软件图标 | Image.Ready、尺寸和字标不重叠 |
| 名称悬停条突兀 | 通用 ToolButton 在名称范围绘制底色 | 整行轻微反馈；保留键盘焦点和详情 |
| 筛选文字不清晰 | Basic 下拉选中项文字/底色角色不匹配，实测 1.365:1 | 显式配对选中底色/文字，日夜切换及键盘选择 |
| 添加房间入口重复 | 观看工具、房间标题加号、内联输入同时存在 | 房间面板一个明确添加按钮，统一走 AddRoomDialog |
| 观看工具交互重叠 | 声音/布局另开 Popup 遮挡上层工具 | 单面板子页、返回/分级 Esc、外部点击关闭、焦点恢复 |
| 白天文字残留浅色 | 编译后无用 mode 依赖被消除，Palette 赋值为值拷贝；正常组还会覆盖禁用组初始化 | 逐角色绑定及显式 active/inactive/disabled；生产编译模块五轮主题切换通过 |

实施顺序：共享控件修复、标题栏/悬停修复、观看工具和房间入口整理、集成构建、交互/视觉/关闭回归、截图审查、记录。

受影响范围：`native/app/qml/controls/`，`AppHeader.qml`、`GuildMemberRow.qml`、`GuildNavigationPanel.qml`、`GuildRankHoverCard.qml`、`WatchToolsPanel.qml`、`RoomSidebar.qml`、`Main.qml`，相关原生 QML 测试。

验证目录：`native/out/build/ui-redesign-baseline-qt612-nmake`。目标：`douyu_monitor_native`、`qml_controls_style_test`、`qml_engine_smoke_test`、`qml_interaction_test`、`qml_visual_smoke_test`、`qml_visual_regression_test`、`qml_close_regression_test`。截图至少覆盖 C1/C3、960×600、1280×800、1366×768、1920×1080。只在检查实际差异后局部更新必要视觉基线。

工具限制：本会话没有暴露 Notion、Serena、Context7、Qt Creator 连接器；使用局部文件检索、现有 Qt 构建和测试。Notion 实时写入无法执行，每个实质步骤进入 `NOTION_SYNC_QUEUE.md` 待同步，不宣称远端同步完成。

## 实际修复

1. `AppHeader.qml` 增加 26×26 的 `icon-app.png`，保留用户批准的 210×24 日夜字标。图标资源运行态 Ready，未挤占画面。
2. `GuildMemberRow.qml` 移除名称宽度的悬停底条和重复通用提示，保留键盘焦点；`GuildNavigationPanel.qml` 提供整行反馈；`GuildRankHoverCard.qml` 按内容收紧高度。
3. `ConsoleComboBox.qml` 为选中项显式配对 Theme.selectedSurface 与 Theme.text。原生夹具测得原选中项对比约 1.36:1，修正后约 11.55:1，保留键盘和原生模型行为。
4. `RoomSidebar.qml` 移除内联输入及标题加号，只保留一个有文字的添加入口，统一打开 AddRoomDialog；关闭按钮合并到标题行。`WatchToolsPanel.qml` 移除重复添加入口。
5. `WatchToolsPanel.qml` 将总览、声音、布局放入同一面板，增加返回和分级 Esc；`Main.qml` 保证工具与辅助抽屉互斥；布局选项互斥并保持重复选择；`ConsoleToolTip.qml` 修复提示可见时外击仅关闭提示、留下工具的问题。保留原 AppController 命令及播放器实例。
6. 共享控件和 Main/七个管理弹层采用语义角色绑定，避免编译后主题文字滞留。`RoomTile.qml` 的浅色头像占位文字按真实底色选色。

## 构建及验证证据

环境：Qt 6.12.0、MSVC、NMake Release；Windows 原生/OpenGL，路径 `native/out/build/ui-redesign-baseline-qt612-nmake/`。

| 验证 | 本轮已取得结果 | 日志（位于 native/out/verification） |
|---|---|---|
| 生产应用及 QML 目标构建 | 退出 0 | six-issue-build-verified.txt |
| 共享控件 | 13 passed / 0 failed | six-issue-controls-final.txt |
| 编译资源主题 | 5 passed / 0 failed | six-issue-engine-verified.txt |
| 完整视觉烟测 | 35 passed / 0 failed；截图夹具 reducedMotion 重建后也通过最终 CTest | six-issue-visual-smoke-final.txt / six-issue-ctest-final.txt |
| 视觉回归 | 7 passed / 0 failed | six-issue-visual-regression-final.txt |
| 生产渲染自测 | --self-test 退出 0，Qt Quick renderer passed | six-issue-app-self-test.txt |
| 完整交互 | 109 passed / 0 failed，42.413 秒 | six-issue-interaction-final.txt |
| 最终集成回归 | 6/6 通过，136.84 秒；选择性运行，非完整仓库测试 | six-issue-ctest-final.txt |
| 普通环境启动 | 无额外 Qt 环境设置，--self-test 退出 0 | six-issue-app-default-env.txt |

编译主题测试走生产 `loadFromModule` 路径，检查 night/day/night/day/night、根窗口与七弹层的正常及禁用颜色、顶部与空态按钮及真实禁用 Switch。tooltip 外击问题已用原生鼠标夹具取得 red 2/1、green 3/0，并加入永久交互测试。

只更新三张逐张审查后的视觉基线：`guild-member-hover-shell-1280x800.png`、`watch-tools-dual-disabled-960x600.png`、`watch-tools-dual-enabled-960x600.png`。旧图先备份至 `native/out/verification/six-issue-baseline-backup/`；复测未启用自动更新基线。

截图使用原生 grabWindow 原图和独立 JSON 来源记录，不把离线名册/S1 归档说成当前直播。榜单本轮复核四尺寸双主题、首屏及列对齐，未重新设计数据模型、排序或统计规则。

最终矩阵校验为 76 张 PNG、8 份 JSON、0 尺寸不符，证据 `native/out/verification/six-issue-image-validation.json`。已重新查看全部缩略汇总及最小白天观看、筛选和榜单原图。CTest 中控件/引擎遵循仓库 offscreen 配置，其他四项使用 Windows/OpenGL；前两项另有 Windows 原生通过记录。

## 交付与边界

- 可执行程序：`native/out/build/ui-redesign-baseline-qt612-nmake/douyu_monitor_native.exe`，20:50:07 生成。
- 历史失败日志保留；最终验证不混用旧交互二进制。测试日志仍有引擎销毁时异步项警告，关闭测试还存在外部头像连接警告。
- 本轮不等于全项目完整 CTest、真实斗鱼长时播放、实体 DPI/设备、安装包或完整 Figma 交付。上述仍单独待验。
- Notion 的本轮步骤连续保存在本地待同步队列，远端未写入或回读。
