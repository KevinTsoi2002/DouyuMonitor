# DouyuMonitor Global UI Visual Upgrade Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (or an equivalent task-by-task execution workflow) to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将现有 Qt Quick/QML 桌面界面迁移到统一的令牌优先视觉系统，同时保留主播列表、播放、弹幕、设置、排行榜、工作区、音频和窗口生命周期语义。

**Architecture:** 保留 `Theme.qml` 作为唯一应用级视觉令牌源，扩展现有 `controls/` 共享控件，并按页面风险分批迁移现有组件。`Main.qml` 继续只负责根窗口和模块接线；页面、浮层和对话框不重建业务模型，不改变 `AppController`、播放器 Loader、网络请求、StreamGet 协议或持久化边界。

**Tech Stack:** Qt 6.8.3、Qt Quick、Qt Quick Controls Basic、QML singleton、C++20/Qt Test、CMake/Ninja/MSVC、Windows x64。

---

## 0. 实施前状态与硬边界

### 当前实现盘点

- 根入口：`native/app/qml/Main.qml`
- 主题：`native/app/qml/Theme.qml`
- 共享控件：`native/app/qml/controls/ControlBackground.qml`、`ConsoleButton.qml`、`ConsoleToolButton.qml`、`ConsoleTextField.qml`、`ConsoleComboBox.qml`、`ConsoleCheckBox.qml`、`ConsoleRadioButton.qml`、`ConsoleToolTip.qml`
- 顶栏/导航/列表：`native/app/qml/components/AppHeader.qml`、`WindowControls.qml`、`RoomSidebar.qml`、`RoomLibraryView.qml`、`GuildNavigationPanel.qml`、`GuildMemberRow.qml`、`GuildRankHoverCard.qml`
- 工作区与状态：`WorkspaceGrid.qml`、`RoomTile.qml`、`PrimaryRoomDivider.qml`、`DanmakuOverlay.qml`、`DanmakuLine.qml`、`ToastViewport.qml`、`WorkspaceStatusBar.qml`
- 页面：`native/app/qml/pages/SettingsPage.qml`、`MaoziRankPage.qml`
- 浮层：`native/app/qml/panels/DanmakuSettingsPanel.qml`、`MonitoringStatusPanel.qml`、`WorkspacePresetsPanel.qml`
- 对话框：`native/app/qml/dialogs/AddRoomDialog.qml`、`NotificationSettingsDialog.qml`、`CloseBehaviorDialog.qml`、`TeamManagerDialog.qml`
- `native/app/qml/dialogs/GroupManagerDialog.qml` 未注册、未从 `Main.qml` 调用，本计划只核查，不将它接入运行时。

### 当前令牌基线

复用 `Theme.qml` 中的 `canvas`、`appBar`、`managementSurface`、`controlSurface`、`well`、`border`、`borderStrong`、`text`、`mutedText`、`disabledText`、`accent*`、`selectedSurface`、`controlHover`、`controlPressed`、`controlDisabled`、`online`、`warning`、`danger`、`dangerSurface`、`topBarHeight`、`controlHeight`、`roomRowHeight`、间距、圆角、字号和 `reducedMotion`。

实施前必须按当前源码确认每个候选属性，而不是把候选名当作现有 API：

| 属性 | 当前源码状态 | 计划规则 |
| --- | --- | --- |
| `focus` | 不存在 | 共享控件继续使用现有 `Theme.accent` 表达焦点；只有至少两个页面需要独立焦点角色时才新增，并同步测试 |
| `focusBorderWidth` | 已存在，值为 2 | 直接复用 |
| `borderWidth` | 已存在，值为 1 | 直接复用 |
| `fontPageTitle` | 已存在，值为 20；由 Step 81 新增 | Settings 与排行榜页面标题已证明跨页面复用；后续页面迁移直接复用 |
| `fontSectionTitle` | 不存在 | 分组标题先使用现有 `fontHeading`/`fontBody`；只有跨页面复用时才新增 |
| `overlaySurface` | 不存在 | 先复用 `managementSurface` 或 `controlSurface`，证明存在跨页面差异后再新增 |
| `divider` | 不存在 | 先复用 `border`；只有分隔线需要独立语义时再新增 |
| `reducedMotion` | 已存在，默认 `false`，由 `Main.qml` 同步到 `Theme.reducedMotion` | 直接复用，不创建同义属性 |

共享控件的真实契约也必须按现有实现使用：

- `ControlBackground.qml` 的必需属性是 `targetControl`，并有 `primary`、`selected`、`flat`、`destructive`；它已经读取 `targetControl.visualFocus`、`enabled`、`down`、`hovered` 和现有主题令牌。
- `ConsoleButton.qml` 和 `ConsoleToolButton.qml` 的根 `id` 都是 `control`，额外暴露 `destructive`；前者把 `highlighted`/`checked` 传给背景，后者还创建键盘/悬停 Tooltip。
- `ConsoleTextField.qml` 使用 `activeFocus`；`ConsoleComboBox.qml` 使用 `visualFocus` 和 `popup.visible`；`ConsoleCheckBox.qml`、`ConsoleRadioButton.qml` 使用 `visualFocus` 和各自的 `checked`/`checkState`。
- `ConsoleToolTip.qml` 的根 `id` 是 `control`，没有 `targetControl` 或 `visualFocus` 公共属性。

因此计划中的状态示例只能作为决策表，不能直接复制到所有控件。若需要独立 `Theme.focus`，先新增属性并验证所有引用；否则使用现有 `Theme.accent`、`Theme.focusBorderWidth` 和 `Theme.borderWidth`。

### 禁止修改

- `native/src/`、`native/service/`、播放器、网络请求、StreamGet 协议、持久化键和业务模型。
- 安装/发布脚本、依赖锁、CMake preset、正式 Release、安装器、视觉基线和基线阈值。
- `native/app/qml/dialogs/GroupManagerDialog.qml` 的运行时注册或业务接入。
- 新增高频 Timer、整页反复重建、复杂 ShaderEffect、模糊、大面积透明叠层和为视觉目的新增第三方依赖。
- 未经用户要求的 `git add`、`git commit`、分支切换、stash、reset、clean、restore。

### 执行前检查

- [x] 记录 `git status --short --branch`、`git rev-parse HEAD`、`git diff --check` 和暂存区。
- [x] 对每个即将修改的 QML 文件执行 `git diff -- <path>`，把前序修改和本轮修改分开记录。
- [x] 保存已有 `native/out/build/windows-x64-release/verification/qml-regression` 诊断输出到新的只读证据目录；不复制或覆盖 `native/tests/visual/baselines`。
- [x] 运行 `ctest --preset windows-x64-release -N -V`，保存当前 38 项注册清单。
- [x] 在任何产品源码修改之前完成改造前基线采集，并将命令、退出码、通过/失败/跳过、标准输出、标准错误、截图路径和哈希写入新的独立证据目录。

**完成标准：** 工作区保护记录完整；目标文件清单固定；既有视觉差异和测试输出已可追溯；没有产品源码改动被误认作本轮改动。

### 阶段 0 执行记录（Step 79）

- 报告：`D:\DouyuAuditS7-b0e669b6-step79-ui-baseline-20261021-01\step79-ui-baseline-report.md`
- 阶段 0 已完成。相关 7 项 CTest 中 6 项通过、1 项失败、0 项跳过；失败项为 `qml_visual_regression_test`，CTest 退出码为 8。
- Step 79 保存的视觉诊断为 44 个文件，其中 21 张 diff mask 与既有记录一致；正式视觉基线哈希未改变。
- Step 80 进一步确认这 21 张差异属于改造前已有视觉差异；`room-quality-popup-640x420` 另有捕获时序敏感性。21 张差异仍未获得视觉基线批准，不能标记为通过、覆盖或用阈值调整消除。
- 阶段 0 没有修改产品源码、测试源码、视觉基线或阈值。

### 改造前基线采集

基线目录必须是本次运行新建且此前不存在的目录，例如：

```powershell
$evidence = Join-Path $env:TEMP ("DouyuUIVisualBaseline-" + (Get-Date -Format "yyyyMMdd-HHmmss"))
if (Test-Path -LiteralPath $evidence) { throw "Evidence directory already exists: $evidence" }
New-Item -ItemType Directory -Path $evidence -ErrorAction Stop | Out-Null
```

在第一项产品源码改动前执行以下步骤：

1. 将 `git status --short --branch`、`git rev-parse HEAD`、`git diff --check`、`git diff --cached --name-only` 和当前 38 项 `ctest -N -V` 输出分别保存到 `$evidence\workspace`。
2. 将本计划所有可能的 Modify 文件的 `git diff --binary -- <paths>` 保存为 `preexisting-tracked.diff`；用 `git ls-files --others --exclude-standard -- <paths>` 保存未跟踪清单；对每个存在的目标文件保存大小、SHA-256 和独立快照。未跟踪目录 `native/app/qml/controls` 必须递归保存文件清单和哈希。
3. 将当前 `native/out/build/windows-x64-release/verification/qml-regression` 复制到 `$evidence\visual-regression-before`。目标目录必须预先不存在；不删除源目录，不覆盖任何 baseline PNG。
4. 先用当前 preset 构建基线所需的已注册目标，记录 Ninja 是否输出 `no work to do`：

```powershell
cmake --build --preset windows-x64-release --target qml_controls_style_test qml_engine_smoke_test qml_visual_smoke_test qml_visual_regression_test qml_interaction_test qml_close_regression_test qml_danmaku_overlay_test *> "$evidence\baseline-build.log"
$buildExit = $LASTEXITCODE
$buildExit | Set-Content "$evidence\baseline-build.exitcode"
```

5. 使用当前构建目录运行真实存在的相关 CTest，并分别保存 stdout、stderr 和退出码。该基线命令不排除视觉回归：

```powershell
$ctest = (Get-Command ctest -ErrorAction Stop).Source
$regex = '^(qml_controls_style_test|qml_engine_smoke_test|qml_visual_smoke_test|qml_visual_regression_test|qml_interaction_test|qml_close_regression_test|qml_danmaku_overlay_test)$'
$proc = Start-Process -FilePath $ctest `
    -ArgumentList @('--preset','windows-x64-release','-R',$regex,'--output-on-failure','-V') `
    -WorkingDirectory (Resolve-Path 'native') `
    -RedirectStandardOutput "$evidence\baseline-ctest.stdout.log" `
    -RedirectStandardError "$evidence\baseline-ctest.stderr.log" `
    -NoNewWindow -Wait -PassThru
$proc.ExitCode | Set-Content "$evidence\baseline-ctest.exitcode"
```

6. 记录 CTest 每个测试的实际退出码、通过/失败/跳过、截图和 red diff mask 路径；对无法运行或已有失败的目标标记为基线限制，不将其归因于本轮迁移。

### 阶段级前序修改分离账本

每个实施阶段在第一次修改前和最后一次修改后都必须创建独立账本目录，例如 `$evidence\stage-03-topbar-sidebar\before` 和 `after`。账本至少包含：

- 本阶段目标文件的明确路径清单和 `git status --short -- <paths>`。
- 每个文件的存在性、大小、SHA-256；目录目标还要有递归文件清单。
- `git diff --binary -- <paths>` 的 tracked diff。
- 未跟踪目标的完整快照，不以最终聚合 diff 代替。
- 阶段完成后的新增、修改、删除文件清单和哈希变化。

如果文件在阶段开始时已经被前序修改，账本必须保存原始 diff/快照，并在阶段报告中分别列出“前序已存在”和“本轮新增”。如果 Git 状态和文件快照无法可靠区分两者，必须标记为“无法可靠分离”，不得把全部最终变化归为本轮。

每个阶段使用同一套只读账本函数，传入该阶段 `Files` 清单，不传入整个仓库：

```powershell
function Save-UiStageLedger {
    param(
        [Parameter(Mandatory)] [string]$StageId,
        [Parameter(Mandatory)] [ValidateSet('before', 'after')] [string]$Phase,
        [Parameter(Mandatory)] [string[]]$Paths,
        [Parameter(Mandatory)] [string]$EvidenceRoot
    )

    $dir = Join-Path $EvidenceRoot "$StageId\$Phase"
    if (Test-Path -LiteralPath $dir) { throw "Ledger already exists: $dir" }
    New-Item -ItemType Directory -Path $dir -ErrorAction Stop | Out-Null
    git status --short -- $Paths | Set-Content -Encoding UTF8 (Join-Path $dir 'status.txt')
    git diff --binary -- $Paths | Set-Content -Encoding UTF8 (Join-Path $dir 'tracked.diff')
    $records = foreach ($path in $Paths) {
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            $item = Get-Item -LiteralPath $path
            $hash = Get-FileHash -Algorithm SHA256 -LiteralPath $path
            [pscustomobject]@{ Path = $path; Kind = 'file'; Exists = $true; Length = $item.Length; SHA256 = $hash.Hash }
        } elseif (Test-Path -LiteralPath $path -PathType Container) {
            Get-ChildItem -LiteralPath $path -File -Recurse | ForEach-Object {
                $hash = Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName
                [pscustomobject]@{ Path = $_.FullName; Kind = 'file'; Exists = $true; Length = $_.Length; SHA256 = $hash.Hash }
            }
        } else {
            [pscustomobject]@{ Path = $path; Kind = 'missing'; Exists = $false; Length = $null; SHA256 = $null }
        }
    }
    $records | ConvertTo-Json -Depth 4 | Set-Content -Encoding UTF8 (Join-Path $dir 'files.json')
    foreach ($path in $Paths) {
        if (Test-Path -LiteralPath $path) {
            $relative = $path -replace '[:\\\/]', '_'
            Copy-Item -LiteralPath $path -Destination (Join-Path $dir "snapshot-$relative") -Recurse -ErrorAction Stop
        }
    }
}
```

阶段开始和结束分别调用 `Save-UiStageLedger -Phase before` 与 `-Phase after`；阶段验证失败也必须先写入 `after` 账本，再停止，不得删除失败证据。

## 1. 主题令牌收敛

**Files:**
- Modify: `native/app/qml/Theme.qml`
- Test: `native/tests/qml_controls_style_test.cpp`
- Inspect only: `native/app/qml/qmldir`, `native/CMakeLists.txt`

**输入：** Stage 0 改造前基线、Theme 属性真值表、目标页面硬编码迁移表。**账本目录：** `stage-01-theme-tokens\before` 与 `stage-01-theme-tokens\after`。

- [x] **Step 1: 建立硬编码迁移表**

从 `AppHeader.qml`、`RoomSidebar.qml`、`RoomTile.qml`、`MaoziRankPage.qml`、`DanmakuSettingsPanel.qml`、`AddRoomDialog.qml`、`TeamManagerDialog.qml` 中逐项记录原始颜色、字号、圆角、宽度、高度和间距，并给每一项标注现有令牌、候选新令牌或“保留局部值”的理由。

- [x] **Step 2: 只增加跨页面复用的令牌**

若迁移表证明同一角色在两个以上页面重复出现，才扩展 `Theme.qml`，并保留旧令牌。属性不存在时先继续使用现有属性；不能把候选名直接写入控件：

```qml
// Only add a role after the migration table proves cross-page reuse.
readonly property color focus: accent
readonly property color divider: border
```

这段只说明可能新增的属性，不是要求全部新增。`focusBorderWidth`、`borderWidth`、`reducedMotion` 已存在，直接复用；`fontPageTitle` 已由 Step 81 证明并新增，值为 20。`fontSectionTitle`、`overlaySurface`、`divider`、`focus` 仍需逐项证明必要性；当前分别复用 `fontBody`/局部字号、`managementSurface` 或 `controlSurface`、`border` 和 `accent`。

- [x] **Step 3: 扩展共享测试的角色断言**

在 `QmlControlsStyleTest::themeHasReadableTextAndStableDimensions()` 中只读取实际存在的属性；若新增角色，增加其最小对比度/颜色断言。保持现有 `>=4.5` 普通文字目标、32 px 控件高度和 `reducedMotion` 检查。测试不得读取正式视觉基线作为通过条件。

- [x] **Step 4: 运行主题阶段验证**

Run:

```powershell
cmake --build --preset windows-x64-release --target qml_controls_style_test qml_engine_smoke_test
ctest --preset windows-x64-release -R '^(qml_controls_style_test|qml_engine_smoke_test)$' --output-on-failure
```

实际结果（Step 81）：在加载 VS x64 开发环境后，两个目标构建退出码为 0；`qml_controls_style_test` 与 `qml_engine_smoke_test` 均通过，CTest 为 2/2、0 失败、0 跳过。共享样式测试通过 QML probe 实际读取并断言 `Theme.fontPageTitle == 20`。按 Step 80 的约束，本阶段未运行 `qml_visual_regression_test`。

**停止条件：** 如果属性真值表、硬编码迁移表或共享控件测试无法证明新增令牌必要，停止新增令牌并只迁移到现有角色。完成标准：所有新增令牌有跨页面使用理由；共享控件仍保持尺寸和输入语义；没有页面继续为同一角色新增原始十六进制颜色。

### 阶段 1 执行记录（Step 81）

- 报告：`D:\DouyuAuditS7-b0e669b6-step81-theme-token-convergence-20261021-01\step81-theme-token-convergence-report.md`
- 迁移表：`D:\DouyuAuditS7-b0e669b6-step81-theme-token-convergence-20261021-01\analysis\token-migration-table.md`
- 已新增且仅新增 `Theme.fontPageTitle: 20`；`focus`、`fontSectionTitle`、`overlaySurface` 和 `divider` 仍不存在。
- 已扩展 `native/tests/qml_controls_style_test.cpp` 的 Theme 断言。Step 81 指定的 `qml_controls_style_test` 与 `qml_engine_smoke_test` 实际结果为 2/2 通过。
- 页面、共享控件、业务逻辑和正式视觉基线未在本阶段迁移或修改；`fontPageTitle` 的页面引用留给后续页面阶段。

## 2. 共享控件状态与焦点统一

**Files:**
- Modify: `native/app/qml/controls/ControlBackground.qml`
- Modify: `native/app/qml/controls/ConsoleButton.qml`
- Modify: `native/app/qml/controls/ConsoleToolButton.qml`
- Modify: `native/app/qml/controls/ConsoleTextField.qml`
- Modify: `native/app/qml/controls/ConsoleComboBox.qml`
- Modify: `native/app/qml/controls/ConsoleCheckBox.qml`
- Modify: `native/app/qml/controls/ConsoleRadioButton.qml`
- Modify: `native/app/qml/controls/ConsoleToolTip.qml`
- Test: `native/tests/qml_controls_style_test.cpp`

**输入：** Stage 1 主题真值表和现有 8 个控件的根 `id`/属性契约。**账本目录：** `stage-02-shared-controls\before` 与 `stage-02-shared-controls\after`。

- [x] **Step 1: 保持控件公共属性不变**

保留现有 `text`、`checked`、`currentValue`、编辑模式、模型角色、`highlighted`、`destructive`、`Accessible.name`、ToolTip 元数据和 Qt signal。只统一背景、边框、字体、焦点和禁用状态。

- [x] **Step 2: 明确定义六类状态**

在每个控件的现有实现中按其真实属性选择状态优先级，避免 Hover 覆盖 Active。以下是安全的状态映射参考，不是可复制到所有控件的统一代码：

```qml
// ControlBackground.qml: targetControl.visualFocus exists here.
border.color: targetControl.visualFocus ? Theme.accent : Theme.border
border.width: targetControl.visualFocus ? Theme.focusBorderWidth : Theme.borderWidth
```

`ConsoleTextField.qml` 继续使用 `control.activeFocus`；`ConsoleComboBox.qml` 继续使用 `control.visualFocus` 和 `control.popup.visible`；CheckBox/RadioButton 继续使用各自的 `checkState`/`checked`。对不支持 `checked` 或 `visualFocus` 的对象不能访问不存在的属性；禁用状态不能依赖透明度隐藏焦点或边界。

- [x] **Step 3: 补齐键盘和 Tooltip 回归**

在 `qml_controls_style_test.cpp` 中保留并扩展已有 Hover/Pressed/Disabled、键盘焦点、Space 激活、ComboBox Escape、Tooltip 键盘焦点和 `reducedMotion` 测试。新增断言只验证状态差异，不绑定到具体截图像素。

- [x] **Step 4: 运行控件验证**

Run:

```powershell
cmake --build --preset windows-x64-release --target qml_controls_style_test
ctest --preset windows-x64-release -R '^qml_controls_style_test$' --output-on-failure
```

**停止条件：** 如果某个控件的现有 Basic 属性不足以表达状态，先记录契约缺口并停止扩展，不修改业务接口。完成标准：控件的 Hover、Pressed、Active/Checked、Focused、Disabled 状态可区分；焦点可见；键盘操作和模型行为不变；没有新增高频刷新。

### 阶段 2 执行记录（Step 82）

- 报告：`D:\DouyuAuditS7-b0e669b6-step82-shared-controls-20261021-01\step82-shared-controls-report.md`
- Step 1–4 均有实际报告证据：公共属性和 Qt 信号保持；六类状态、焦点和 Tooltip 断言扩展；`qml_controls_style_test` 与 `qml_engine_smoke_test` 构建及 CTest 为 `2/2` 通过。
- `Theme.reducedMotion`、Step 81 的 `Theme.fontPageTitle` 和既有尺寸/对比度断言均保留。未运行视觉回归，未修改正式视觉基线。

## 3. 顶栏、侧栏和主播列表迁移

**Files:**
- Modify: `native/app/qml/components/AppHeader.qml`
- Modify: `native/app/qml/components/WindowControls.qml`
- Modify: `native/app/qml/components/RoomSidebar.qml`
- Modify: `native/app/qml/components/RoomLibraryView.qml`
- Modify: `native/app/qml/components/GuildNavigationPanel.qml`
- Modify: `native/app/qml/components/GuildMemberRow.qml`
- Modify: `native/app/qml/components/GuildRankHoverCard.qml`
- Modify: `native/app/qml/components/RoomTile.qml`
- Modify: `native/app/qml/components/PrimaryRoomDivider.qml`
- Modify: `native/app/qml/components/WorkspaceStatusBar.qml`
- Inspect only: `native/app/qml/Main.qml`, `native/app/qml/components/WorkspaceGrid.qml`
- Conditional Modify: `native/app/qml/Main.qml` only for minimal focus-return signal/wiring when tests prove it is required
- Test: `native/tests/qml_interaction_test.cpp`, `native/tests/qml_visual_smoke_test.cpp`

**输入：** Stage 2 控件状态验证、现有 `Main.qml` signal/visible 接线和主播列表交互基线。**账本目录：** `stage-03-header-sidebar-list\before` 与 `stage-03-header-sidebar-list\after`。

- [x] **Step 1: 迁移顶栏和窗口操作**

保留 `AppHeader` 的 signal、`controller.setLayout()`、`setAudioMode()`、`setGlobalMuted()`、窗口拖动、最大化、全屏和关闭连接。统一 52 px 顶栏、32 px 工具按钮、16 px 图标和焦点/危险关闭态；不把声音或布局 Popup 拆成不复用的新文件。

- [x] **Step 2: 迁移侧栏与导航**

保持 268 px 房间侧栏、284 px 社团导航、当前/历史/收藏三态和互斥可见性。将当前列表 delegate、`RoomLibraryView` delegate、`GuildMemberRow` 的文字/状态/操作区域迁移到令牌；保持打开房间、移除、收藏、上下移动、音频焦点和房间号提交的命中区域互不重叠。

- [x] **Step 3: 迁移工作区卡片的外壳**

只迁移 `RoomTile`、`PrimaryRoomDivider` 和 `WorkspaceStatusBar` 的表面、文字和状态令牌；不改 `RoomTile` 的 `playerLoader`、`controlsTimer`、菜单处理或 `WorkspaceGrid` 的布局计算。保持 60 px 侧栏行高、房间卡片标题与操作栏不重叠。`Main.qml` 只有在关闭 Popup/Dialog 后焦点无法恢复时才允许增加最小 signal/`forceActiveFocus()` 接线，不能改根窗口业务逻辑。

- [x] **Step 4: 运行列表和尺寸验证**

Run:

```powershell
cmake --build --preset windows-x64-release --target qml_visual_smoke_test qml_interaction_test
ctest --preset windows-x64-release -R '^(qml_visual_smoke_test|qml_interaction_test)$' --output-on-failure
```

Expected: `1280×720` 和 `960×600` 结构检查通过；侧栏/导航互斥、列表操作、长名称裁切、布局菜单、声音总控和工作区预设现有交互仍通过。

**停止条件：** 若焦点恢复可以在现有 Popup/Dialog 文件中完成，则不修改 `Main.qml`；若必须修改，只保留可由交互测试证明的 UI 接线。完成标准：主窗口、当前/历史/收藏、导航、主播行、顶栏、声音总控和布局菜单使用同一套状态语言；主播列表没有新增选择框语义。

### 阶段 3 执行记录（Step 83 / Step 83A）

- Step 83 报告：`D:\DouyuAuditS7-b0e669b6-step83-header-sidebar-20261021-01\step83-header-sidebar-report.md`
- Step 83A 补充报告：`D:\DouyuAuditS7-b0e669b6-step83a-list-hover-20261021-01\step83a-list-hover-report.md`
- 顶栏、窗口控制、当前/历史/收藏、社团导航、主播行和工作区外壳已完成增量迁移；`Main.qml`、`WorkspaceGrid.qml`、播放器、网络和后台任务未修改。
- Step 83A 进一步核对三种列表：当前列表使用 `RoomSidebar.qml` 自有 delegate；历史和收藏共同使用 `RoomLibraryView.qml`，由 `favoritesOnly` 过滤。当前列表新增 `roomSidebarHoverArea`/`roomRemoveButton` 测试定位；历史列表复用 `historyHoverArea`/`removeHistoryButton`；收藏列表验证 `favoritesOnly` 下 `openHistoryRoomButton` 可悬停且删除按钮隐藏。
- Step 83A 最终构建退出码为 `0`；`qml_visual_smoke_test` 与 `qml_interaction_test` 为 `2/2` 通过。三种列表均通过操作区域几何隔离和按钮 Hover 状态断言，当前活动行颜色在 Hover 后保持不变。
- 计划中的尺寸验证已执行：`1280x720` 直接结构检查通过，窄窗口最小约束为 `960x600`，交互测试覆盖 960 px 顶栏宽度；当前没有独立完整窗口 `QSize(960, 600)` fixture，仍需后续补充或人工确认。未运行视觉回归，历史差异仍未批准。

## 4. 排行榜页面迁移

**Files:**
- Modify: `native/app/qml/pages/MaoziRankPage.qml`
- Test: `native/tests/qml_interaction_test.cpp`, `native/tests/qml_visual_regression_test.cpp`, `native/tests/qml_visual_smoke_test.cpp`

**输入：** Stage 3 顶栏/列表结构通过结果、现有榜单数据 fixture 和改造前视觉差异证据。**账本目录：** `stage-04-rank-page\before` 与 `stage-04-rank-page\after`。

- [ ] **Step 1: 固定榜单数据边界**

保留综合排行、定级榜、游乐值榜的排序、评分、筛选、刷新、搜索、站外打开和 `rankClient.refresh()` 调用。只调整标题、工具栏、表头、行表面、列宽令牌、等级标签和直播状态表达。

- [ ] **Step 2: 收敛表格几何**

将当前直接写入的列宽和行高集中为 `MaoziRankPage` 内的只读布局值或 `Theme` 共享尺寸；不得把单页表格列宽伪装成全局控件令牌。保持表头和 delegate 使用同一组列宽，保留长名称省略和 Tooltip。

- [ ] **Step 3: 移除无业务意义的整行绿色边框**

直播状态保留绿色文字/状态点并保留可读的“直播中/未开播”文字；普通行只使用 `border` 或交替表面。不得改变 live filter 或数据模型。

- [ ] **Step 4: 运行榜单验证**

Run:

```powershell
cmake --build --preset windows-x64-release --target qml_interaction_test qml_visual_regression_test
ctest --preset windows-x64-release -R '^(qml_interaction_test|qml_visual_regression_test)$' --output-on-failure
```

Expected: 交互目标验证打开、筛选和刷新；视觉目标把差异写入隔离诊断目录。历史 baseline mismatch 不得通过替换 PNG 或放宽阈值解决。

**停止条件：** 若列宽调整改变排序、筛选、评分或站外打开行为，停止榜单迁移并保留当前实现。完成标准：榜单列对齐、状态和排名易于扫描；数据含义不变；视觉差异有逐图审查记录。

## 5. 设置页与业务对话框迁移

**Files:**
- Modify: `native/app/qml/pages/SettingsPage.qml`
- Modify: `native/app/qml/dialogs/AddRoomDialog.qml`
- Modify: `native/app/qml/dialogs/NotificationSettingsDialog.qml`
- Modify: `native/app/qml/dialogs/CloseBehaviorDialog.qml`
- Modify: `native/app/qml/dialogs/TeamManagerDialog.qml`
- Inspect only: `native/app/qml/dialogs/GroupManagerDialog.qml`
- Test: `native/tests/qml_controls_style_test.cpp`, `native/tests/qml_interaction_test.cpp`

**输入：** Stage 4 视觉差异分类、现有设置/对话框 objectName、controller fake 和取消/导入交互基线。**账本目录：** `stage-05-settings-dialogs\before` 与 `stage-05-settings-dialogs\after`。

- [x] **Step 1: 统一设置页层级**

保持设置页返回、关闭行为、更新检查、活动角色映射和队伍管理 signal。将页面标题、说明、分组标题、输入框、身份选择、队伍选择和保存操作迁移到 Theme/controls；窄窗口使用现有滚动结构，不通过裁切隐藏设置。

- [x] **Step 2: 统一对话框骨架**

保留现有 Dialog 尺寸作为初始基线、标题、`palette`、header/footer、按钮 objectName 和 controller 调用。统一 16 px padding、32/34 px 控件命中区、关闭按钮 Tooltip、边界和危险操作反馈；不创建新的通用 Dialog 组件，除非两个以上对话框实际共享且不会改变业务接口。

- [x] **Step 3: 固定 Escape 和未保存语义**

为每个 Dialog/Popup 先写交互断言，再改样式：

```text
AddRoom: Escape => close only; no search/add.
CloseBehavior: Escape/cancel => close only; no background/quit.
TeamManager: Escape while importing => existing cancelMaoziTeamImport path.
Notification: checkbox changes remain immediate save; close does not add a second save.
```

焦点必须在打开时进入合理输入或首个控件，关闭后返回触发按钮；如果 Qt 基础 Dialog 无法提供该恢复路径，先在对话框内部尝试最小修复，再由 `Main.qml` 提供仅限焦点恢复的 signal/接线；两种方式都必须由 `QMLInteractionTest` 的 window fixture 证明。不能借焦点修复修改 controller 或窗口关闭业务。

- [x] **Step 4: 运行设置和对话框验证**

Run:

```powershell
cmake --build --preset windows-x64-release --target qml_controls_style_test qml_interaction_test
ctest --preset windows-x64-release -R '^(qml_controls_style_test|qml_interaction_test)$' --output-on-failure
```

**停止条件：** 任一 Escape/取消路径出现提交、退出或未保存数据丢失，停止后续页面迁移并先恢复原业务语义。完成标准：设置分组、输入/选择控件、关闭/取消/保存语义和焦点路径保持；`GroupManagerDialog.qml` 仍未注册或调用。

**Step 5 execution record (2026-10-21):** Stage 5 migrated `SettingsPage.qml`, `AddRoomDialog.qml`, `NotificationSettingsDialog.qml`, `CloseBehaviorDialog.qml`, and `TeamManagerDialog.qml` to the existing Theme tokens and verified shared controls without changing controller calls, persistence, import cancellation, or close behavior. `qml_controls_style_test` and `qml_interaction_test` built successfully with the VS x64 environment and CTest completed `2/2` passed. The interaction fixture covers AddRoom Escape, immediate notification saves, close-behavior cancel/background/quit paths, TeamManager import cancellation, focus entry/return, and 960x600 dialog reachability. The 960x600 check initially failed against the fixed 680 px TeamManager height, then passed after the dialog gained a bounded height and scrollable content. No visual regression test ran; the 21 historical mismatches and all later visual differences remain unapproved. Windows DPI, native Popup placement, GPU composition, and real playback remain manual checks.

## 6. 弹幕、监控、预设与音频浮层迁移

**Files:**
- Modify: `native/app/qml/panels/DanmakuSettingsPanel.qml`
- Modify: `native/app/qml/panels/MonitoringStatusPanel.qml`
- Modify: `native/app/qml/panels/WorkspacePresetsPanel.qml`
- Modify: `native/app/qml/components/AppHeader.qml`
- Inspect only: `native/app/qml/components/DanmakuOverlay.qml`, `native/app/qml/components/DanmakuLine.qml`, `native/app/qml/components/ToastViewport.qml`
- Test: `native/tests/qml_interaction_test.cpp`, `native/tests/qml_visual_smoke_test.cpp`, `native/tests/qml_danmaku_overlay_test.cpp`

**输入：** Stage 5 对话框焦点/取消回归、现有面板尺寸和弹幕 overlay 生命周期基线。**账本目录：** `stage-06-floating-panels\before` 与 `stage-06-floating-panels\after`。

- [x] **Step 1: 统一右侧浮层骨架**

保持现有弹幕 380 px、监控 320 px、预设 360 px 的内容语义和滚动区域；统一标题栏、关闭按钮、内边距、分组标题、底部操作区和 Popup/Drawer 状态。不要在每个面板内复制一套颜色常量。

- [x] **Step 2: 迁移弹幕设置**

保留显示、治理、统计三个 tab、全局/房间治理范围、滑块计算、字体下拉框、房间开关和重置/清除调用。只替换硬编码表面、文字和选中态；面板高度变化必须仍由现有 Flickable/Section 结构承载。

- [x] **Step 3: 迁移监控、预设、声音**

监控面板继续显示直播中/未开播/异常计数和 Windows 通知状态；预设删除继续是危险操作并保留现有 controller 调用；声音 Popup 继续区分全局静音、单声道、多声道和当前选中项。Hover 不得替代 checked/active。

- [x] **Step 4: 运行浮层验证**

Run:

```powershell
cmake --build --preset windows-x64-release --target qml_visual_smoke_test qml_interaction_test qml_danmaku_overlay_test
ctest --preset windows-x64-release -R '^(qml_visual_smoke_test|qml_interaction_test|qml_danmaku_overlay_test)$' --output-on-failure
```

**停止条件：** 若面板迁移引入新的创建/销毁循环、Timer 或 controller 调用，停止该阶段并回退到仅表面令牌修改。完成标准：三个右侧浮层和两个顶栏 Popup 在视口内稳定显示；弹幕设置行为、状态计数、预设增删、声音和布局选中态均保持；弹幕渲染创建/销毁和刷新频率没有新增工作。

### 阶段 6 执行记录（Step 86，2026-10-22）

- 报告：`D:\DouyuAuditS7-b0e669b6-step86-floating-panels-20261022-01\step86-floating-panels-report.md`
- 三个浮动面板的实际文件为 `DanmakuSettingsPanel.qml`、`MonitoringStatusPanel.qml` 和 `WorkspacePresetsPanel.qml`；声音 Popup 与布局 Popup 均嵌入 `AppHeader.qml`。`DanmakuOverlay.qml`、`DanmakuLine.qml` 和 `ToastViewport.qml` 仅检查，未修改。
- Step 1–3 已按阶段前快照实施：面板和 Popup 复用现有 `Theme` 令牌及共享控件，保留面板定位、滚动、标签、治理/统计、预设保存/应用/删除、声音模式、全局静音和布局选择的业务调用。
- Step 4 的目标构建退出码为 `0`；`qml_controls_style_test`、`qml_visual_smoke_test` 和 `qml_interaction_test` 在修正布局菜单未选中高亮颜色后的最终 CTest 结果为 `3/3` 通过。`qml_danmaku_overlay_test` 单独构建输出 `ninja: no work to do.`，CTest 为 `1/1` 通过。
- 阶段前后账本、阶段专属差异和最终哈希保存在上述审计目录；未修改阶段测试源文件。正式视觉基线 24/24 个文件 SHA-256 保持不变，未运行 `qml_visual_regression_test`，未更新阈值或批准历史差异。
- 一次直接 `qml_interaction_test -vs` 诊断因无可读输出而中断；该事件不作为测试结果，阶段报告仅将其记录为诊断限制。自动化结果来自有界 CTest 运行。Windows 原生 Popup 定位、DPI/系统字体、多显示器、GPU 合成和真实播放性能仍需人工确认。

## 7. 两种窗口尺寸、DPI 限制和性能保护

**Files:**
- Modify only if required by prior stages: `native/tests/qml_visual_smoke_test.cpp`, `native/tests/qml_interaction_test.cpp`
- Inspect only: `native/app/qml/Main.qml`, `native/app/qml/components/WorkspaceGrid.qml`, `native/app/qml/components/RoomTile.qml`, `native/app/qml/components/PrimaryRoomDivider.qml`, `native/app/qml/components/WorkspaceStatusBar.qml`, `native/app/qml/components/DanmakuOverlay.qml`, `native/app/qml/components/DanmakuLine.qml`, `native/app/qml/components/GuildNavigationPanel.qml`

**输入：** Stage 6 浮层回归输出、前序性能保护记录和当前结构 smoke/interaction 测试。**账本目录：** `stage-07-layout-performance\before` 与 `stage-07-layout-performance\after`。

- [x] **Step 1: 扩展结构检查而非像素基线**

保留 `qml_visual_smoke_test` 对 `1280×720`、`960×600`、`1600×900` 和 `1920×1080` 的窗口边界、面板 containment、房间标题/操作栏不重叠、面板内容非黑的检查。只有现有 objectName 无法定位迁移后的真实控件时才增加 objectName，不增加截图基线。

- [x] **Step 2: 检查视图切换负担**

在 `qml_interaction_test` 中重复打开/关闭设置、排行榜、弹幕、监控和预设面板，确认常驻实例数量不增加、重复打开不重复创建 controller 任务。测试只观察 QML object tree 和现有 fake controller 计数，不访问网络。

- [x] **Step 3: 记录 DPI 和性能限制**

在验证报告中单独记录 offscreen/Basic 后端、Windows DPI、系统大字体、多显示器、GPU 合成和长时间播放未在本轮自动化中证明。若卡顿只在真实播放器/网络/后台任务中复现，停止 UI 迁移并建立独立问题记录。

**停止条件：** 若只能通过修改播放器、网络、后台任务或业务模型解释性能问题，停止 UI 计划并登记独立问题。完成标准：两个主尺寸无重叠、裁切和失控留白；没有新增高频 Timer、同步网络请求或整页刷新；限制项明确记录。

### 阶段 7 执行记录（Step 88、Step 88A、Step 88B，2026-10-22）

- Step 88 报告：`D:\DouyuAuditS7-b0e669b6-step88-layout-performance-20261022-01\step88-layout-performance-report.md`。初次四尺寸结构检查在 `960×600` 发现可见 `roomAnchorName` 负宽度，阶段 7 暂不关闭。
- Step 88A 报告：`D:\DouyuAuditS7-b0e669b6-step88a-compact-roomtile-layout-20261022-01-02\step88a-compact-roomtile-layout-report.md`。在受限子阶段中仅修复 `RoomTile.qml` 紧凑底栏空间分配，保留六个操作、播放器 Loader、`controlsTimer` 和 controller 调用；四个尺寸的元数据正尺寸、containment 和操作区不重叠检查均通过。
- Step 88B 报告：`D:\DouyuAuditS7-b0e669b6-step88b-stage07-closeout-20261022-01\step88b-stage07-closeout-report.md`。最终构建实际为 `ninja: no work to do`，`qml_visual_smoke_test` 与 `qml_interaction_test` CTest 为 `2/2` 通过，阶段 7 的自动化条件完成。
- 阶段 7 的自动化范围确认了 `960×600`、`1280×720`、`1600×900`、`1920×1080`，五个浮层 containment、内容非黑、元数据与操作区几何，以及三轮设置/榜单/弹幕/监控/预设切换中的常驻对象和 fake 榜单刷新计数。
- Step 88A 是针对阶段 7 失败证据的受限产品布局修复，不改变播放器、网络、后台任务、业务模型或构建配置；该例外在两个阶段报告中单独记录，不能把它误记为阶段 7 原始测试文件之外的无记录变更。
- Windows DPI、系统字体、多显示器、原生 Popup、GPU 合成、真实播放密度和长时间直播性能仍未由本阶段自动化证明。`native/out/verification/team-import` 更早运行缺少阶段前保护清单的历史缺口仍保留并已在 Step 88A/88B 报告中披露；本阶段只保护当前已有内容。

## 8. 视觉差异审查与基线保护

**Files:**
- Inspect only: `native/tests/visual/baselines/`
- Inspect only: `native/docs/visual-validation.md`
- Evidence output: new external directory under the existing audit temp root

**输入：** Stage 0 改造前视觉证据、Stage 4 榜单差异记录和 Stage 7 结构/性能验证结果。**账本目录：** `stage-08-visual-review\before` 与 `stage-08-visual-review\after`。

- [ ] **Step 1: 保存改造前证据**

在运行视觉回归前复制现有 `native/out/build/windows-x64-release/verification/qml-regression` 到新的只读证据目录，记录复制源、时间和文件哈希；不触碰 `native/tests/visual/baselines`。

- [ ] **Step 2: 运行确定性视觉回归**

Run:

```powershell
cmake --build --preset windows-x64-release --target qml_visual_regression_test
ctest --preset windows-x64-release -R '^qml_visual_regression_test$' --output-on-failure -V
```

- [ ] **Step 3: 逐图分类**

将每个失败图归类为历史差异、迁移新增差异或环境/时序无法判定差异。必须保留当前截图和 red diff mask，不调整阈值、不覆盖 baseline、不运行 `update-visual-baselines.ps1 -Approve`。

- [ ] **Step 4: 人工确认后再决定基线**

只有用户或明确的视觉负责人逐图批准后，才另行安排基线更新；本计划不包含该批准或基线写入步骤。

**停止条件：** 任一差异无法区分为历史、本轮或环境原因时，保留证据并标记为无法判定，不修改阈值或基线。完成标准：每次视觉差异都能追溯到具体文件和迁移阶段；正式基线没有被本轮自动修改。

## 9. 最终验证与交付报告

**输入：** Stage 1–8 的阶段账本、构建/CTest日志、结构结果、视觉差异分类和未解决风险。**账本目录：** `stage-09-final-report\before` 与 `stage-09-final-report\after`。

- [ ] **Step 1: 构建受影响 QML 目标**

Run:

```powershell
cmake --build --preset windows-x64-release --target qml_controls_style_test qml_engine_smoke_test qml_visual_smoke_test qml_visual_regression_test qml_interaction_test qml_close_regression_test qml_danmaku_overlay_test
```

记录实际编译目标；若 Ninja 输出 `no work to do`，明确写入报告，不把它描述为重新编译。

- [ ] **Step 2: 运行直接相关 CTest**

Run:

```powershell
ctest --preset windows-x64-release -R '^(qml_controls_style_test|qml_engine_smoke_test|qml_visual_smoke_test|qml_visual_regression_test|qml_interaction_test|qml_close_regression_test|qml_danmaku_overlay_test)$' --output-on-failure
```

记录注册目标、通过/失败/跳过、每个失败项和实际输出路径。视觉回归失败与功能/交互失败分开报告。

- [ ] **Step 3: 执行静态卫生检查**

Run:

```powershell
git diff --check
git status --short --branch
git diff -- native/app/qml/Theme.qml `
    native/app/qml/controls/ControlBackground.qml `
    native/app/qml/controls/ConsoleButton.qml `
    native/app/qml/controls/ConsoleToolButton.qml `
    native/app/qml/controls/ConsoleTextField.qml `
    native/app/qml/controls/ConsoleComboBox.qml `
    native/app/qml/controls/ConsoleCheckBox.qml `
    native/app/qml/controls/ConsoleRadioButton.qml `
    native/app/qml/controls/ConsoleToolTip.qml `
    native/app/qml/components/AppHeader.qml `
    native/app/qml/components/WindowControls.qml `
    native/app/qml/components/RoomSidebar.qml `
    native/app/qml/components/RoomLibraryView.qml `
    native/app/qml/components/GuildNavigationPanel.qml `
    native/app/qml/components/GuildMemberRow.qml `
    native/app/qml/components/GuildRankHoverCard.qml `
    native/app/qml/components/RoomTile.qml `
    native/app/qml/components/PrimaryRoomDivider.qml `
    native/app/qml/components/WorkspaceStatusBar.qml `
    native/app/qml/pages/SettingsPage.qml `
    native/app/qml/pages/MaoziRankPage.qml `
    native/app/qml/panels/DanmakuSettingsPanel.qml `
    native/app/qml/panels/MonitoringStatusPanel.qml `
    native/app/qml/panels/WorkspacePresetsPanel.qml `
    native/app/qml/dialogs/AddRoomDialog.qml `
    native/app/qml/dialogs/NotificationSettingsDialog.qml `
    native/app/qml/dialogs/CloseBehaviorDialog.qml `
    native/app/qml/dialogs/TeamManagerDialog.qml `
    native/app/qml/Main.qml `
    native/tests/qml_controls_style_test.cpp `
    native/tests/qml_visual_smoke_test.cpp `
    native/tests/qml_interaction_test.cpp `
    native/tests/qml_visual_regression_test.cpp
```

确认没有 C++ 业务层、服务、依赖锁、发布产物、视觉基线或无关文件变化；对未跟踪控件目录使用 Stage 账本中的快照/哈希核对，而不是依赖 `git diff`。

- [ ] **Step 4: 生成实施报告**

报告必须列出：

1. 实际修改文件和前序 diff 分界；
2. 每个页面的迁移内容；
3. 新增/复用的 Theme 令牌和共享控件；
4. 状态、焦点、Escape、取消、未保存修改验证；
5. `1280×720`、`960×600`、长名称、空数据、Popup 和列表密度验证；
6. QML CTest 实际结果；
7. 历史视觉差异、本轮新增差异和无法判定差异；
8. DPI、GPU、真实播放性能和人工审批限制。

**停止条件：** 任一阶段账本缺失、前序修改无法可靠分离、或测试结果无法区分功能失败与视觉差异时，停止交付报告并明确证据缺口。完成标准：计划范围内的源码和测试已按阶段验证；所有未运行或无法证明的项目单独列出；没有把视觉基线通过、QML 测试通过或构建成功描述成真实 Windows 安装、长时间播放或视觉审批通过。

## 分层视觉验收

实施报告必须分开记录三类结果，不能互相替代：

1. **功能与布局回归：** QML 模块加载、共享控件行为、尺寸、重叠、裁切、列表操作、Popup/Dialog 打开关闭、Escape、键盘焦点、取消和未保存修改。证据来自 `qml_controls_style_test`、`qml_engine_smoke_test`、`qml_visual_smoke_test`、`qml_interaction_test`、`qml_close_regression_test` 和 `qml_danmaku_overlay_test` 的实际输出。
2. **视觉差异审查：** `qml_visual_regression_test` 的截图和 red diff mask，逐项标记“改造前已存在”“本轮新增”“环境/时序无法判定”。历史 mismatch 不得自动归入本轮回归，也不得通过覆盖 PNG 或调整阈值消除。
3. **人工验收：** Windows DPI、字体/系统大字体、原生 Popup 位置、GPU 合成、多显示器、真实使用密度和实际播放表现。自动化 QML 通过不能推出这些条件通过；本计划不执行真实安装、真实直播或长时间播放验收。

## 依赖关系和人工判断

执行顺序固定为：工作区/基线冻结 → Theme 令牌 → 共享控件 → 顶栏/侧栏/列表 → 排行榜 → 设置/对话框 → 浮层/Popup → 尺寸/性能回归 → 视觉差异审查 → 最终报告。

页面不能在共享令牌和控件状态未稳定前大规模迁移。任何会触及 `AppController`、`RoomSession`、`MultiRoomCoordinator`、`MpvQuickItem`、StreamGet、网络、持久化或发布链的变更都必须从本计划撤回，作为独立任务评估。

以下事项需要人工判断：

- 既有三张或五个视觉 mismatch 是否得到视觉负责人批准；
- Windows DPI、系统大字体、多显示器、原生 Popup 位置和 GPU 合成；
- 打开设置/排行榜后的真实播放卡顿是否仍存在；
- 真实主播列表、长文本、空数据和错误状态下的产品密度是否可接受；
- 是否需要增加 `overlaySurface`、`divider` 或 `focus`，还是现有角色已经足够；`fontPageTitle` 已由 Step 81 建立，后续只需在页面迁移中复用。
