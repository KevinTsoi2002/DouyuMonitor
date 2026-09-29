# 多布局大规模直播间容量扩展方案

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将每个工作区布局的直播间容量分阶段扩展到第一阶段 16 路、第二阶段 24 路，并保证布局、播放、弹幕、音频、持久化和性能边界可控。

**Architecture:** 不直接把现有 `9/10` 常数改成更大的数字，而是建立集中式容量策略，将“布局槽位上限”“同时解码预算”“弹幕会话预算”“多声道预算”拆开。2026-09-29 的产品决策是正式版 16 路全部解码，24 路测试版也全部解码；播放预算仍集中管理，但当前两个构建阶段都不主动保留预览房间。持久化继续兼容现有 `nativeWorkspaceV1` 工作区数据。

**Tech Stack:** Qt 6.8.3、Qt Quick/QML、C++20、libmpv、Qt WebSockets、Qt Test、CMake/CTest、Inno Setup 6。

---

## 1. 目标容量

### 第一阶段：16 路

- `auto`、`primary`、`primary-two` 三种布局统一支持最多 16 路。
- `primary-two` 仍要求至少 4 路。
- 默认和最大同时解码预算均为 16 路。
- 弹幕默认预算为 12 路，硬上限第一阶段为 16 路。
- 多声道默认预算为 4 路，硬上限第一阶段为 8 路。
- 12 路以下保持当前完整体验；13–16 路开始使用紧凑房间卡。

### 第二阶段：24 路

- 三种布局统一支持最多 24 路。
- 默认和最大同时解码预算均为 24 路，其中两个主画面始终优先。
- 24 路模式下默认弹幕预算为 16 路，多声道预算为 4 路。
- 24 路全解码仅通过独立测试版构建开关启用，不作为正式版默认构建。

### 不允许的实现方式

- 不只修改 `MultiRoomCoordinator::kMaxRooms` 和文案。
- 不让正式版默认构建承担 24 路全解码的稳定性承诺。
- 不允许弹幕默认建立 24 个无优先级、无上限的 WebSocket 会话。
- 不通过缩小到不可操作的房间卡来掩盖高密度布局问题。
- 不通过持久化播放地址、Cookie、Token、签名或原始弹幕数据换取恢复能力。

## 2. 当前限制

| 模块 | 当前行为 | 扩容影响 |
| --- | --- | --- |
| `MultiRoomCoordinator::kMaxRooms` | 10 | 领域层硬上限 |
| `AppController::addRoom` | 非双主布局达到 9 路即拒绝 | 应改为容量策略，不再硬编码 9/10 |
| `NativeWorkspaceStore` | 活动房间、分组、预设均为 10 | 需要统一容量策略并兼容旧数据 |
| `WorkspaceGrid.qml` | 布局表最多覆盖 9 路，几何计算截断到 10 路 | 需要动态行列表和任意路数算法 |
| `DanmakuSessionManager` | 最多 9 个会话，按输入顺序抢占 | 需要优先主画面、次级主画面和活动高优先级房间 |
| `resolveRoomQuality` | 5 路以上主画面原画、其余标准画质 | 高密度下仍需保留，但不足以解决 GPU 和解码压力 |
| `RoomTile.qml` | 每张卡都会加载一个完整 `MpvQuickItem` | 24 路直接创建 24 个渲染上下文风险过高 |
| 持久化 | `nativeWorkspaceV1`，当前格式版本 5 | 扩容本身不应破坏旧版本可读性 |

## 3. 核心设计

### 3.1 集中式容量策略

新增容量策略模块，例如：

```cpp
// native/src/workspace/room_capacity.h
namespace RoomCapacity {

enum class Profile {
    Balanced,
    HighPerformance,
    LowResource,
};

struct Limits {
    int maxLayoutRooms = 24;
    int maxDecodedRooms = 16;
    int maxDanmakuSessions = 16;
    int maxMultiAudioRooms = 4;
};

int maxRoomsForLayout(const QString &layoutId);
Limits limitsForProfile(Profile profile);
}
```

规则：

- 所有容量限制只从该模块读取。
- `AppController`、`WorkspaceModel`、`NativeWorkspaceStore`、QML 不再分别硬编码 `9` 或 `10`。
- `WorkspaceModel::maxRooms()` 改为动态属性，并在容量策略变化时发出通知。
- 第一阶段通过配置把 `maxLayoutRooms` 设为 16；第二阶段切换为 24。
- 单元测试必须覆盖 16、17、24、25 四个边界。

### 3.2 布局策略

所有布局都必须支持 1–24 路，任何路数都不允许出现空白格或越界。

`auto` 布局推荐行列规则：

| 路数 | 行 x 列 |
| --- | --- |
| 1 | 1 x 1 |
| 2 | 1 x 2 |
| 3 | 1 x 3 |
| 4 | 2 x 2 |
| 5–6 | 2 x 3 |
| 7–9 | 3 x 3 |
| 10–12 | 3 x 4 |
| 13–16 | 4 x 4 |
| 17–20 | 4 x 5 |
| 21–24 | 4 x 6 |

`primary` 布局规则：

- 1 路：主画面占满。
- 2–4 路：主画面占 2/3，其余房间单列排布。
- 5–8 路：主画面居中，左右两侧均衡排布。
- 9–16 路：主画面约 45% 宽度，其余房间按动态网格排布。
- 17–24 路：主画面约 40% 宽度，其余房间进入紧凑网格。

`primary-two` 布局规则：

- 仍要求至少 4 路。
- 两个主画面位于顶部并平分宽度，顶部区域占可用高度的 55%–62%。
- 底部房间按以下列数排布：
  - 4 路：2 列。
  - 5–8 路：4 列。
  - 9–12 路：4 列。
  - 13–16 路：5 列。
  - 17–24 路：6 列。
- 底部行数使用 `ceil(剩余房间数 / 列数)` 计算。
- 主画面 ID 缺失时按房间顺序补位，不能出现几何空洞。

布局几何必须使用统一的间隙、边界和行高计算函数，禁止继续扩展成按路数手写的无限 `if/else`。

### 3.3 房间卡密度

新增三种密度：

| 密度 | 触发行宽 | 行为 |
| --- | --- | --- |
| `comfortable` | 宽度 ≥ 320 px | 保留当前顶栏、底栏、画质和弹幕控件 |
| `compact` | 宽度 180–319 px | 缩矮顶栏和底栏，隐藏次要分类与热度，操作按需显示 |
| `dense` | 宽度 < 180 px | 只保留直播状态、主播名、主画面标识和悬浮操作入口 |

规则：

- 紧凑模式不能隐藏直播状态、主播名、主画面标识、静音状态和错误状态。
- 房间卡文字不得与操作栏、弹幕层或相邻卡片重叠。
- `dense` 模式下画质下拉框、弹幕按钮等操作进入悬浮菜单。
- 最小窗口下允许进入 `dense`，但不能把文字压缩到不可读。

### 3.4 播放解码预算

新增 `RoomRenderBudget` 或等价协调层，按以下优先级分配解码槽位：

1. 第一主画面。
2. 第二主画面。
3. 当前声音焦点房间。
4. 视口内优先级最高的活动房间。
5. 其余房间按列表顺序。

超出预算的房间：

- 仍保留在 `RoomListModel` 和 `WorkspaceGrid` 中。
- 显示状态、头像、标题、热度和操作入口。
- 可以解析直播状态，但不应在默认模式下创建 libmpv 渲染上下文。
- 用户点击“开始播放”或将其设为主画面时，可以触发预算替换。

第一阶段默认值：

```text
最大布局槽位：16
默认解码：16
最大解码：16
```

第二阶段默认值：

```text
最大布局槽位：24
默认解码：24
最大解码：24
```

如果后续确认机器硬件能力不足，应单独评估降低默认解码预算，而不是直接回退已经验收的布局容量。

### 3.5 弹幕和音频预算

弹幕：

- 继续使用有界队列和去重策略。
- 会话优先级与播放预算保持一致。
- 第一阶段硬上限 16，默认 12。
- 第二阶段硬上限 16，默认 12；用户可提高到 16。
- 预算不足时，未获得会话的房间显示“等待弹幕会话”，不得静默失败。

音频：

- 单声道模式仍只保留一个声音焦点。
- 多声道模式默认最多 4 路，硬上限 8 路。
- 达到上限时，新增房间保持静音并提供明确提示。
- 主画面和声音焦点房间享有优先级。

## 4. 持久化与兼容

- 继续保持 `QSettings` 中的 `DouyuMonitor/nativeWorkspaceV1` 键名。
- 扩容本身不提升 JSON `version`，避免旧客户端因版本过新直接丢弃整个工作区。
- 新增字段必须是可选字段，旧版本缺少字段时使用默认值。
- `activeRoomIds`、分组房间、预设房间的规范化上限改为容量策略值。
- 旧版本把活动房间截断为 10 路时，只允许截断活动集合，不删除房间库记录。
- 如果第二阶段新增解码预算或密度偏好，字段缺失时按当前硬件档案使用默认值。
- 不向持久化文件增加播放地址、Cookie、Token、签名、原始弹幕帧或原始服务诊断。

## 5. 分阶段实施

### Task 1: 建立容量策略和失败测试

**Files:**

- Create: `native/src/workspace/room_capacity.h`
- Create: `native/src/workspace/room_capacity.cpp`
- Modify: `native/CMakeLists.txt`
- Modify: `native/src/workspace/multi_room_coordinator.h/.cpp`
- Modify: `native/src/ui/workspace_model.cpp`
- Test: `native/tests/multi_room_coordinator_test.cpp`
- Test: `native/tests/workspace_model_test.cpp`

- [ ] 写测试：`maxRoomsForLayout("auto") == 16`、`("primary") == 16`、`("primary-two") == 16`。
- [ ] 写测试：17 路在第一阶段被拒绝，刚好 16 路可接受。
- [ ] 写测试：第二阶段切换上限后，24 路可接受，25 路被拒绝。
- [ ] 运行 `ctest --preset windows-x64-release -R "multi_room_coordinator_test|workspace_model_test" --output-on-failure`，确认新测试先失败。
- [ ] 实现容量策略并把协调器、模型和控制器中的 9/10 判断改为策略调用。
- [ ] 重新运行定向测试并确认通过。
- [ ] 提交：`feat: centralize workspace room capacity`。

### Task 2: 扩展 1–16 路布局和 QML 几何

**Files:**

- Modify: `native/app/qml/components/WorkspaceGrid.qml`
- Modify: `native/app/qml/components/RoomTile.qml`
- Modify: `native/app/qml/Main.qml`
- Modify: `native/tests/qml_interaction_test.cpp`
- Modify: `native/tests/qml_visual_smoke_test.cpp`

- [ ] 为 1、2、3、4、6、9、12、16 路建立布局失败测试。
- [ ] 断言所有房间卡位于画布内、互不重叠、没有未使用空洞。
- [ ] 用动态行列算法替换当前截断到 10 路的 `autoRowCounts`、`primarySideCounts` 和 `Math.min(10, ...)`。
- [ ] 增加 `compact` 和 `dense` 密度判断，并让 `RoomTile` 按密度切换控件。
- [ ] 在 1280×720、1600×900、1920×1080 下运行 `qml_visual_smoke_test`。
- [ ] 检查截图中的 16 路布局、双主布局和窄宽度卡片。
- [ ] 提交：`feat: support sixteen-room layouts`。

### Task 3: 播放、弹幕和音频预算

**Files:**

- Create: `native/src/workspace/room_render_budget.h/.cpp`
- Modify: `native/src/ui/app_controller.h/.cpp`
- Modify: `native/src/workspace/room_session.*`
- Modify: `native/src/danmaku/danmaku_session_manager.*`
- Modify: `native/app/qml/components/RoomTile.qml`
- Test: `native/tests/multi_room_coordinator_test.cpp`
- Test: `native/tests/danmaku_session_manager_test.cpp`
- Test: `native/tests/qml_close_regression_test.cpp`

- [ ] 写测试：12 路以下全部进入解码预算。
- [ ] 写测试：16 路时创建 16 个渲染上下文。
- [ ] 写测试：主画面、第二主画面和声音焦点优先占用预算。
- [ ] 写测试：超出预算的房间保留资料、状态和操作，不创建播放器。
- [ ] 实现预算分配和房间优先级。
- [ ] 将弹幕会话上限、音频多声道上限接入预算策略。
- [ ] 提交：`feat: add scalable playback and session budgets`。

### Task 4: 完成第一阶段发布门槛

**Files:**

- Modify: `README.md`
- Modify: `native/README.md`
- Modify: `native/docs/visual-validation.md`
- Test: all affected CTest targets

- [ ] 运行 Release 构建：`cmake --build --preset windows-x64-release --target douyu_monitor_native -- -j1`。
- [ ] 运行 QML 测试：`ctest --preset windows-x64-release -R "^(qml_engine_smoke_test|qml_visual_smoke_test|qml_interaction_test|qml_close_regression_test)$" --output-on-failure`。
- [ ] 运行协调器和持久化测试：`ctest --preset windows-x64-release -R "^(multi_room_coordinator_test|native_workspace_store_test|app_controller_test|workspace_model_test)$" --output-on-failure`。
- [ ] 用固定本地媒体或 fake sidecar 运行 16 路关闭回归，确认 16 个房间卡都能正常创建、移除和释放。
- [ ] 人工检查 1、4、9、12、16 路截图。
- [ ] 真实斗鱼环境运行 12 路和 16 路各 30 分钟，记录峰值工作集、CPU、GPU、首帧成功数和关闭结果。
- [ ] 第一阶段验收门槛：16/16 房间可见，默认预算至少 12 路可用，30 分钟无崩溃，关闭在 10 秒内完成。
- [ ] 提交：`release: support sixteen-room workspaces`。

### Task 5: 扩展 17–24 路布局

**Files:**

- Modify: `native/app/qml/components/WorkspaceGrid.qml`
- Modify: `native/app/qml/components/RoomTile.qml`
- Modify: `native/tests/qml_interaction_test.cpp`
- Modify: `native/tests/qml_visual_smoke_test.cpp`
- Modify: `native/tests/qml_close_regression_test.cpp`

- [ ] 写测试：17、20、24 路布局均满足边界、无重叠和无空洞要求。
- [ ] 扩展 `auto` 到 6×4，扩展 `primary` 到 40% 主画面加 23 路辅画面。
- [ ] 扩展 `primary-two` 到底部 6 列，顶部两个主画面保持可辨识尺寸。
- [ ] 在 2560×1440 和 1920×1080 下生成 24 路截图。
- [ ] 在 1280×720 下生成 24 路 `dense` 截图，只验证可读性和控件可达性，不要求完整控件常驻。
- [ ] 运行 `qml_visual_smoke_test` 和 `qml_interaction_test`。
- [ ] 提交：`feat: support twenty-four-room layouts`。

### Task 6: 完成第二阶段预算和持久化

**Files:**

- Modify: `native/src/workspace/room_capacity.*`
- Modify: `native/src/workspace/native_workspace_store.cpp`
- Modify: `native/src/ui/app_controller.*`
- Modify: `native/src/ui/workspace_model.*`
- Modify: `native/app/qml/pages/SettingsPage.qml`
- Test: `native/tests/native_workspace_store_test.cpp`
- Test: `native/tests/app_controller_test.cpp`
- Test: `native/tests/qml_interaction_test.cpp`

- [ ] 写测试：24 路活动集合、分组和预设可以完整保存和恢复。
- [ ] 写测试：旧版本 10 路工作区可以加载，新增 11–24 路房间库记录不丢失。
- [ ] 写测试：解码预算、弹幕预算和音频预算能够持久化并恢复。
- [ ] 在设置页增加性能档位和“全部解码”风险提示。
- [ ] 默认档位为 `Balanced`，解码预算 16，弹幕预算 12，多声道预算 4。
- [ ] `HighPerformance` 才允许解码预算 24 和弹幕预算 16。
- [ ] `LowResource` 使用解码预算 8、弹幕预算 8、多声道预算 2。
- [ ] 提交：`feat: persist configurable room budgets`。

### Task 7: 第二阶段验收和性能门槛

**Files:**

- Modify: `native/docs/visual-validation.md`
- Modify: `README.md`
- Modify: `native/README.md`
- Test: Release CTest and performance harness

- [ ] 运行 Release 全量 CTest。
- [ ] 运行 24 路关闭回归，确认 24 个房间卡没有悬挂回调。
- [ ] 用固定媒体验证 24 路创建 24 个播放器并全部解码。
- [ ] 真实斗鱼环境运行 16 路和 24 路各 30 分钟。
- [ ] 记录峰值工作集、CPU、GPU、每路首帧、弹幕会话数、音频路数和关闭耗时。
- [ ] 第二阶段测试版门槛：24/24 房间可见并创建播放器，30 分钟无崩溃，关闭在 15 秒内完成。
- [ ] 24 路全解码在真实斗鱼环境完成 30 分钟资源验收前，只能标记为测试版。
- [ ] 更新知识库和发布说明。
- [ ] 提交：`docs: document scalable room capacity verification`。

## 6. 验收矩阵

| 场景 | 布局 | 解码预算 | 必须验证 |
| --- | --- | --- | --- |
| 第一阶段基线 | 1/4/9/12/16 | 16 | 布局、首帧、内存、CPU、关闭 |
| 第一阶段上限 | 16 | 16 | GPU、八路以上弹幕、多声道 |
| 第二阶段测试版 | 24 | 24 | 24 张卡可见，24 路创建播放器并解码 |
| 窄窗口 | 16/24 | 不提高 | `compact`/`dense` 文字与操作可达 |
| 旧数据迁移 | 10 路旧工作区 | 默认预算 | 加载、保存、恢复不丢房间库 |
| 降级回滚 | 16/24 路工作区 | 旧版本 | 不出现崩溃或敏感数据写入 |

## 7. 风险与决策点

### 风险

- 当前项目没有显式设置 `hwdec`，真实硬件解码能力必须先实测，不能假设 24 路都能硬解。
- 9 路合成关闭基线已约 822 MB 峰值工作集；24 路全解码可能显著超过普通 16 GB 机器承受范围。
- 24 路弹幕会同时增加网络连接、心跳、JSON 解码、治理队列和 UI 更新压力。
- 24 路多声道会导致音频混音和用户听觉不可用，必须设置预算。
- 24 路在 1280×720 下无法维持完整可读卡片，只能使用 `dense` 模式。
- 高密度布局的点击区域和悬浮菜单需要重新做视觉验证，不能只验证矩形不重叠。

### 已确认决策

1. 正式版 16 路全部创建播放器并解码。
2. 24 路全解码只作为独立测试版构建和 GitHub 预发布提供，不进入正式版默认构建。
3. 24 路弹幕仍按优先级受有界会话预算控制，不默认无上限建立 WebSocket。
4. 多声道模式继续使用有界预算；当前默认和硬上限分别为 4 路和 8 路。
5. 高密度布局按房间卡宽度自动进入 `compact` 或 `dense`，不额外增加用户可见的模式开关。

24 路全解码尚未完成真实斗鱼环境下的长时间资源验收，不能作为正式版能力声明。
