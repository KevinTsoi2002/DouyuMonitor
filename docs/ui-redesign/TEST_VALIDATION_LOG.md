# Test Validation Log

## 四项反馈：最终本地验证（2026-11-01，Asia/Shanghai）

| 验证 | 实际结果 | native/out/verification/日志 |
| --- | --- | --- |
| 生产及相关十目标构建 | exit0 | four-build.txt |
| 新增交互定向 | 7/0 | four-focused.txt |
| 完整控件 | 13/0 | four-controls.txt |
| 编译模块引擎 | 5/0 | four-engine.txt |
| 完整交互 | 115/0/0，56.985秒 | four-interaction-final.txt |
| 完整视觉烟测 | 35/0/0，71.686秒 | four-visual-smoke.txt |
| 名册/controller/播放器/远端解析/RoomSession | CTest5/5，44.77秒 | four-domain.txt |
| 最终Qt部署及载荷检查 | exit0 | four-deployment-retry.txt |
| EXE渲染自测 | exit0，stderr空 | four-app-self-test-result.json及stdout/stderr |
| 真实图片 | 46/46 Qt网络解码；生产页面46/46排行、36/36定级赛与美伢详情Ready | portrait-recovery-20261101/qt-results.txt、four-rank-real/results.txt |
| 筛选性能及功能 | 5/0；9次同步6–147ms→5–10ms，创建/销毁186/186→0/0 | guild-filter-performance/REPORT.md |

最终EXE为`native/out/build/ui-redesign-baseline-qt612-nmake/douyu_monitor_native.exe`，2026-11-01 02:29:27+08，8,251,392字节；SHA256 `11C1C6710F49A02C6788BDC07AB5FA4813ABCFD22F93A4452A530B496F8E2FCD`。最后链接后部署曾因领域测试占用Qt6Core失败；fast重试只up-to-date，随后等所有测试退出，显式执行windeployqt及项目校验才确认成功。失败日志保留。

首轮完整交互114/1，名称悬停未收到hover；隔离3/0。补窗口激活/第一帧等待后完整115/0。故障注入缺图、假弹幕接口与异步销毁警告均保留，不称零警告。未运行全仓库CTest、新安装包/设备DPI或当前真实长播；TLS时长探针失败，见[播放链接核查](PLAYBACK_LINK_LIFETIME.md)。下文为之前各轮历史结果。

## 九项截图修复：最终本地验证（2026-10-31，Asia/Shanghai）

本节为2026-10-31历史状态；以下 six-issue 和更早记录保留当时事实，不代替2026-11-01四项反馈的验证。原生生产源码已重建，本轮六种 QML 目标与五种领域目标均通过（分批串行运行，不冒称一次完整仓库 CTest）。

| 验证 | 结果 | native/out/verification/ 日志 |
| --- | --- | --- |
| 完整交互直接运行 | 114 passed / 0 failed / 0 skipped，67.981秒 | nine-interaction-verified.txt |
| 交互独占桌面CTest | 1/1，69.76秒 | nine-interaction-ctest-serialized.txt |
| 控件、编译模块引擎、视觉回归、关闭CTest | 4/4，69.02秒 | nine-qml-four-final.txt |
| 完整视觉烟测 | 35/0，115.928秒；四尺寸双主题 | nine-visual-smoke-final.txt |
| 名册、房间resolver、controller、workspace store、S1 archive CTest | 5/5，24.31秒 | nine-domain-final.txt |
| 生产EXE自测 | exit 0；Qt Quick renderer passed；stderr空 | nine-app-self-test-result.json、nine-app-self-test-stdout.txt |
| 截图清单 | 8 manifests、166 PNG、0尺寸不符；8组拼图与关键原图已看 | nine-image-validation.json；qml/repair-*-review.png |

最终生产EXE：`D:\DouyuMonitor\native\out\build\ui-redesign-baseline-qt612-nmake\douyu_monitor_native.exe`，修改时间2026-10-31 23:29:43（UTC+08:00），晚于本轮最后生产QML修改23:24:26；SHA256 `FD81E394F4B1A1548800820C04B3AA9D646C91F18F1CA1D70FC9DC29945402E9`。先前完整构建和最终资源增量构建均退出0，日志nine-final-remaining-build.txt、nine-final-refresh-build.txt；最后仅改测试的构建nine-hover-final-build.txt退出0。

新断言覆盖全部45场详情原文、目录62人/25男37女、过滤排除选择、头像异步/身份守卫、九维缺失值与0、定级赛房间号搜索、1.2秒hover无跳变与真实点击、画内深色控件及清晰度键盘。初测失败及定位保留在NOTION_SYNC_QUEUE 077–085；不删除红灯日志。一次CTest与EXE自测重叠时失败无正文，独占桌面复跑通过；焦点争抢是依据时间重叠的可能原因，不是已证明的唯一根因。

头像证据边界：四张真实公开头像Qt网络加载3/0，见nine-issue-remote-avatar/native-network.txt。离线矩阵使用明确标记的本地品牌图夹具；静态目录在不借助线上榜单时可覆盖34/46排行、26/36定级赛。其余12/10位保留在线头像刷新与缺图回退，未逐一验证真实头像成功，不能声称46/46加载。原AppController仍有既存线上精确名称索引；本轮新增QML关联使用ID/房间号/明确别名，不猜名字改写归档。

仍有既存测试假弹幕对象信号/方法、外部404及引擎销毁异步项警告；不声称零warning。自测未传媒体，不证明真实视频颜色或长时播放；未改mpv色彩参数。完整Figma、真实直播/弹幕长时、DPI/设备、安装包与全仓库CTest仍未验。Notion已恢复，当前同步证据见NOTION_REMOTE_SYNC.md，历史即时同步缺口如实保留。

Date: 2026-10-31  
Timezone: America/Los_Angeles  
Stage: Regression issue repair and verification  
Historical Notion status at this checkpoint: unavailable; current connection and readback are documented in NOTION_REMOTE_SYNC.md.

## Changes and Findings

- In `native/app/qml/components/RoomTile.qml`, quality-popup visibility now
  contributes to `hasTransientControls`, keeping the room controls visible
  while the popup is open.
- `native/tests/qml_interaction_test.cpp` covers popup identity, visibility,
  the controls timeout while open, and auto-hide after closing.
- Updated `native/docs/visual-validation.md` to list the current 23 baseline
  inputs and identify five retained legacy PNGs that the current test does not
  load. The two old dual-primary images are 960x260; current Watch Tools
  captures are 960x600.

## Verification

Build command completed successfully for `douyu_monitor_native`,
`qml_visual_regression_test`, `qml_interaction_test`,
`qml_close_regression_test`, `qml_engine_smoke_test`, and
`qml_visual_smoke_test` in
`native/out/build/ui-redesign-baseline-qt612-nmake`.

The active visual baseline check found 23 inputs, 0 missing files, and 0
dimension mismatches. `qml_visual_regression_test` passed. No baseline-update
environment variable was set; this run did not rewrite baseline images.

CTest command:

```powershell
ctest --test-dir native/out/build/ui-redesign-baseline-qt612-nmake --output-on-failure -R "^(qml_interaction_test|qml_close_regression_test|qml_visual_regression_test|qml_engine_smoke_test|qml_visual_smoke_test)$"
```

Result: 5/5 passed in 108.26 seconds:

- `qml_engine_smoke_test`: 2.53 seconds
- `qml_visual_smoke_test`: 37.88 seconds
- `qml_visual_regression_test`: 14.77 seconds
- `qml_interaction_test`: 32.68 seconds
- `qml_close_regression_test`: 20.29 seconds

The close regression passed under the current Windows test runner; the earlier
`QProcess CreateFile` permission failure did not reproduce. No production
process-launch behavior was changed.

## Follow-Up

Sync this record and the updated visual-baseline inventory to the DouyuMonitor
Notion knowledge base when Notion tools are available.

## Supplemental Controller and Workspace Tests (2026-10-31)

The existing Windows test executables were run separately with the unrestricted
runner after restricted runs encountered file and child-process permission
errors. These are targeted executable results, not a full `ctest` run:

- `app_controller_test.exe`: 79 passed, 0 failed, 1 skipped. The skipped case is
  the opt-in read-only live preview. Log:
  `native/out/build/ui-redesign-baseline-qt612-nmake/app-controller-unrestricted-20261031.txt`.
- `native_workspace_store_test.exe`: 25 passed, 0 failed. Log:
  `native/out/build/ui-redesign-baseline-qt612-nmake/workspace-store-unrestricted-20261031.txt`.

Earlier restricted-run logs for these targets contain filesystem/QProcess
permission failures and do not supersede the successful unrestricted runs.
Full CTest, real playback, and the four reported UI issues remain outside this
historical evidence. The six-issue repair below has separate, newer evidence.

## Six screenshot issues — final local verification

Date: 2026-10-31, Asia/Shanghai (UTC+08:00). This section supersedes the earlier
UI issue status, without rewriting the historical runs above.

Build: Qt 6.12.0 / MSVC / NMake Release in
`native/out/build/ui-redesign-baseline-qt612-nmake`. Production/QML targets built
successfully (`native/out/verification/six-issue-build-verified.txt`), followed by
a successful incremental build of the final interaction and screenshot tests
(`six-issue-build-last-tests.txt`). The production EXE was generated at 20:50:07.

Actual final command, with `QT_QPA_PLATFORM=windows`,
`QSG_RHI_BACKEND=opengl` in the calling process:

```powershell
ctest --test-dir native/out/build/ui-redesign-baseline-qt612-nmake --output-on-failure -j 1 -R '^(qml_controls_style_test|qml_engine_smoke_test|qml_interaction_test|qml_visual_smoke_test|qml_visual_regression_test|qml_close_regression_test)$'
```

Result: **6/6 passed, exit 0, 136.84 seconds**. Log:
`native/out/verification/six-issue-ctest-final.txt`.

| Target | Result | Time |
|---|---|---|
| qml_controls_style_test | Passed | 4.09 s |
| qml_engine_smoke_test | Passed | 2.73 s |
| qml_visual_smoke_test | Passed | 50.73 s |
| qml_visual_regression_test | Passed | 16.26 s |
| qml_interaction_test | Passed | 40.26 s |
| qml_close_regression_test | Passed | 22.69 s |

CTest retains the repository's explicit offscreen setting for controls and
engine; the four other targets use Windows/OpenGL. Separate Windows-native
controls and compiled engine tests also passed. This is a selected set, not
the whole repository CTest suite.

Separate direct-run evidence, all logs under `native/out/verification/`:

| Check | Result | Log |
|---|---|---|
| Windows shared controls | 13 passed / 0 failed | six-issue-controls-final.txt |
| Compiled production module theme changes | 5 passed / 0 failed | six-issue-engine-verified.txt |
| Full pointer/keyboard interaction | 109 passed / 0 failed, 42.413 s | six-issue-interaction-final.txt |
| Visual smoke | 35 passed / 0 failed | six-issue-visual-smoke-final.txt |
| Visual regression after reviewed baseline updates | 7 passed / 0 failed | six-issue-visual-regression-final.txt |
| Initial final-build close regression | 10 passed / 0 failed; subsequent final CTest also passed | six-issue-close.txt |
| Production --self-test | Exit 0, Qt Quick renderer passed | six-issue-app-self-test.txt |
| Production --self-test without extra Qt environment setup | Exit 0, Qt Quick renderer passed | six-issue-app-default-env.txt |

The production module test uses `loadFromModule` and five night/day states;
it checks eight palette owners, actual header/empty-state button text, and a
disabled native Switch. The tooltip outside-click failure has an independent
native red/green fixture (2/1 then 3/0) and a permanent interaction assertion.
The two final self-tests did not supply a media file, so they do not assert
media first-frame or real stream playback.

Only three intended PNG baselines were updated after viewing each current
image: guild-member-hover-shell-1280x800 and watch-tools-dual-disabled/enabled-
960x600. Existing images were backed up to
`native/out/verification/six-issue-baseline-backup/`. The final regression run
did not enable automatic baseline updates. Matrix captures were regenerated
after enabling reducedMotion in the test fixture (production motion unchanged)
to avoid mid-transition colors. Validation found 76 PNGs, eight provenance
JSON files and zero dimension mismatches (`six-issue-image-validation.json`).
All thumbnails and selected full-size minimum-window images were inspected.

Warnings remain in direct native runs: asynchronous items at QML engine
destruction and external avatar connection warnings in close tests. No
zero-warning claim is made. No AutoMoc or QProcess CreateFile permission error
occurred in the final build/CTest.

Scope: these results close the six screenshot fixes locally. Full Figma
delivery, Notion remote writes/readback, real Douyu long-duration playback,
device/DPI and installer acceptance remain separate unfinished work. The
Notion connector is unavailable; records are saved in NOTION_SYNC_QUEUE.md.
