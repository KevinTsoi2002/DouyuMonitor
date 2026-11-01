# 悬停提示、头像、筛选性能与播放链接检查

日期：2026-11-01，Asia/Shanghai。当前状态：本轮四项反馈的本地修复、调查与相关验证已完成；未将其等同整体重构验收。

## 范围与验收

- 取消组件鼠标悬停提示；保留 Accessible.name、快捷键和键盘焦点提示。
- 用精确成员 ID / 房间 ID 补齐历史榜单头像；在线头像失败时尝试可信备用源，不能用名字猜测身份。
- 测量真实 62 人目录的全部 / 男 / 女切换，减少委托重复创建；保留搜索、顺序、选择清理和实时状态更新。
- 只读检查 300 秒有效期、现有续播与登录能力，不提取账户 Cookie，不实现未经要求的登录接入。
- 保留已确认 C1/C3 视觉与已有工作区改动；生产继续原生 QML/C++。

## 执行与验证

1. 已完成来源定位与当前工作区检查。
2. 已补12位公开头像、候选失败回退与无关刷新保持；46图Qt网络解码、生产页面逐项加载及永久QML回归通过。
3. 原生筛选性能与功能5/0：9次同步6–147ms→5–10ms，创建/销毁186/186→0/0；完整目录稳定保留，隐藏项禁用。
4. 控件13/0、引擎5/0、完整交互115/0、视觉烟测35/0、领域CTest5/5均通过；最终EXE已重链接、部署校验和渲染自测exit0。GUI串行。
5. 播放链调查完成，详见[时效核查](PLAYBACK_LINK_LIFETIME.md)。实时匿名地址仍有expire=300，无登录永久地址证据；媒体TLS探针失败，未完成当前长时实播。

相关技能：systematic-debugging、writing-plans、qt-qml、qt-qml-profiler、verification-before-completion。Serena、Context7、Qt Creator MCP 当前不可调用，使用范围受控的源码调查和官方文档替代。现有大范围未提交改动保留，不提交或推送。

## 证据与剩余事项

当前证据：

- 头像46图：`native/out/verification/portrait-recovery-20261101/`，3/0、46个Image.Ready。
- 生产榜单QML：`native/out/verification/four-rank-real/`，3/0、排行46/46、定级赛36/36，美伢详情Ready；已查看夜间/白天榜单及详情截图。
- 筛选：`native/out/verification/guild-filter-performance/REPORT.md`及两个qmlprofiler trace。创建248→62（保留首次62）；不能把含QtTest等待的全局JS时间当卡顿指标。
- 独立审查后修正相同URL刷新重新尝试失败源的问题；稳定导航行外部解析后关闭旧输入框。
- Notion记录088–092已即时保存并回读；根页面`40a0f4bdec4881f0ac8fe8082135e167`。
- 新编译永久测试：定向交互7/0；完整控件13/0；引擎5/0；完整交互最终115/0/0（56.985秒）。日志分别为`four-focused.txt`、`four-controls.txt`、`four-engine.txt`、`four-interaction-final.txt`。
- 首轮完整交互114/1，旧名称悬停用例未等窗口激活/第一帧；隔离3/0。修订等待与顶部五按钮鼠标悬停检查后全套115/0；`four-interaction.txt`与`four-hover-isolated.txt`保留失败过程。
- Notion记录093–096包含以上真实结果，均保存并回读；现有假弹幕/故障注入图片警告不称零警告。
- 最终记录097–099包含视觉、领域、部署失败与重试、EXE自测和交付检查；项目主页回读088–099共12条唯一记录，无缺号重复。

八份最新截图清单共166张PNG，尺寸检查0异常（`four-image-validation.json`）；已查看当前日夜真实榜单、美伢详情、定级赛、白天导航筛选和夜间工具总览，未发现新的遮挡/缺图。未改视觉基线。

最终EXE：[douyu_monitor_native.exe](../../native/out/build/ui-redesign-baseline-qt612-nmake/douyu_monitor_native.exe)，修改时间2026-11-01 02:29:27+08，8,251,392字节；SHA256 `11C1C6710F49A02C6788BDC07AB5FA4813ABCFD22F93A4452A530B496F8E2FCD`。

部署曾因测试占用Qt6Core.dll失败，随后在测试退出后显式重跑windeployqt和项目运行库校验并通过；日志`four-deployment-retry.txt`。最终`--self-test`退出0、stderr空，见`four-app-self-test-result.json`。

截图为原生生产组件夹具，不等同真实播放验收。当前真实媒体时长探针TLS失败；没有证明登录可取得永久链接，保留现有预取和失败恢复。全仓库CTest、安装包、设备DPI、长期实播/弹幕和完整Figma不在本轮完成范围。已有假弹幕、故障图片和异步销毁警告保留。
