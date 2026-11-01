# 已确认视觉方向的实施计划

更新：2026-10-31，Asia/Shanghai。用户已确认 R3，本轮六项截图问题修复完成并通过专项验证及最终六项目标 CTest 6/6。原整体重构尚未完成全部交付与验收。

## 工作约束

沿用当前脏工作区及已保存快照，不回退既有变更。C1 夜间/C3 白天、C 组件结构、36px 图片字标标题栏、80px 导航轨；观看工具覆盖打开，不改变视频网格和播放器身份。所有真实业务功能继续通过现有控制器。Notion 工具当前不可用，每步即时保存在 NOTION_SYNC_QUEUE.md，不能标记已远端同步。

## 最新复审状态（2026-10-31）

用户已确认 C1 夜间/C3 白天和 C 组件方向；当前没有新的配色方向待确认。后续六张截图明确了图标、名称悬停、筛选框对比度、重复添加入口、观看工具交互和白天字色问题。本轮已修改这些区域；原榜单需求继续按真实数据与指定尺寸复核，未将本轮修复描述为全应用重新设计完成。修复前审查与当前处置见 [PLAN_REVIEW.md](PLAN_REVIEW.md)，专项范围见 [SIX_ISSUE_REPAIR.md](SIX_ISSUE_REPAIR.md)。

状态定义：已验证表示有该项最新证据；已落源码表示代码存在但不代表视觉或功能验收通过；待办表示仍需实施或验证。

- [x] 设计方向、Token、源码映射和原生基线已记录。
- [x] 本轮六项源码修复已落盘：标题栏独立图标、整行悬停、筛选框选中对比度、统一添加入口、观看工具内联子页与关闭/焦点流程、AOT 下稳定的日夜角色绑定。
- [x] 本轮专项原生结果：引擎 5/0、控件 13/0、视觉烟测 35/0、视觉回归 7/0；证据分别为 `six-issue-engine-verified.txt`、`six-issue-controls-final.txt`、`six-issue-visual-smoke-final.txt`、`six-issue-visual-regression-final.txt`，位于 `native/out/verification/`。
- [x] 编译模块的无标注昼夜截图已生成并人工检查；最小窗口、榜单列对齐和指定截图细节已复核。三张预期变化基线先备份后局部更新，并完成不带更新模式的视觉回归。没有把 R3 离线说明带当成产品标题栏。
- [x] 生产程序已重新构建（20:50:07，Asia/Shanghai），`--self-test` 退出 0，日志 `native/out/verification/six-issue-app-self-test.txt`。
- [x] 最终六项目标 CTest：控件、引擎、交互、视觉烟测、视觉回归、关闭回归，6/6 通过，136.84 秒；`native/out/verification/six-issue-ctest-final.txt`。独立完整交互 109/0；普通环境启动自测退出 0。
- [~] 原全应用统一验收仍有边界：本轮榜单是复核，未重写数据或排序；手动打开的工具仍会临时覆盖画面。截图和离线测试不证明真实直播内容、所有 DPI/设备或全部用户流程均已验收。
- [ ] Figma：文件、变量和部分组件已存在；剩余公共组件、页面、品牌图片和禁用态疑点因 Starter MCP 配额未完成。
- [~] 历史回归：六项修复前选择性 QML 5/5，独立 `app_controller_test` 79 passed / 0 failed / 1 个只读 live-preview skip、`native_workspace_store_test` 25/0，见 `TEST_VALIDATION_LOG.md`。这些不能替代本轮最终运行；完整仓库 CTest、真实斗鱼长时间播放、设备/DPI 与安装包验收仍未完成。
- [ ] Notion：本地日志已写入待同步队列；当前无可用 Notion 工具，未做远端写入或回读。

## 验证方法与证据边界

使用 Qt 6.12.0 + MSVC + NMake 隔离构建目录 `native/out/build/ui-redesign-baseline-qt612-nmake`。六项修复前的 5/5 运行解决了当时的 AutoMoc spawn、交互与关闭回归 QProcess 权限问题；这些失败保留为历史。当前专项结果以上述 `six-issue-*` 日志为准。最终六项目标 CTest 即使通过，也仍是选择性回归，不是完整仓库 CTest 或全产品验收。

本轮最终六项目标 CTest 和记录已完成；后续按原总体计划跟踪独立剩余验收。具体构建环境见 [BASELINE_NATIVE.md](BASELINE_NATIVE.md)，历史结果与边界见 [IMPLEMENTATION_REPORT.md](IMPLEMENTATION_REPORT.md)、[VISUAL_QA_REPORT.md](VISUAL_QA_REPORT.md)。完整应用服务包仍须遵守现有 provenance 校验。

Figma 需等待可用配额后补齐；Notion 需工具可用后写入并回读。这些工具状态与本地文件记录分开报告，不将待同步日志视为远端完成。
