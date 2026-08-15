# 弹幕筛选与高峰治理 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在现有多直播间弹幕链路中加入全局默认与房间覆盖的关键词筛选、重复抑制、自适应高峰治理和统计面板。

**Architecture:** 新增无副作用的 `danmaku-governance` 纯函数引擎，由 `danmaku-store` 在进入 pending 队列前调用；工作区保存全局治理设置和按房间的部分覆盖。设置面板使用三个页签，弹幕连接和播放链路保持不变。

**Tech Stack:** TypeScript、React、Zustand vanilla store、Vitest、Vite、Electron renderer。

---

## 文件变更总览

- Create: `src/renderer/danmaku/danmaku-governance.ts`，治理配置规范化、峰值分级、批次处理和运行时状态。
- Create: `tests/danmaku-governance.test.ts`，纯治理引擎的失败测试和回归测试。
- Modify: `src/renderer/danmaku/danmaku-settings.ts`，加入治理配置、覆盖类型和解析器。
- Modify: `tests/danmaku-settings.test.ts`，加入默认值、范围和关键词清洗测试。
- Modify: `src/renderer/store/danmaku-store.ts`，在队列前应用治理并暴露分项统计。
- Modify: `tests/danmaku-store.test.ts`，加入 store 集成行为测试并更新高峰批次断言。
- Modify: `src/renderer/store/danmaku-context.tsx`，把工作区有效配置同步给弹幕 store。
- Modify: `src/renderer/store/workspace-persistence.ts`，快照版本 4、治理覆盖解析和迁移。
- Modify: `tests/workspace-persistence.test.ts`，覆盖版本 4 与旧版本迁移。
- Modify: `src/renderer/store/workspace-store.ts`，全局治理与房间覆盖 actions。
- Modify: `tests/workspace-store.test.ts`，覆盖 actions、持久化和继承关系。
- Modify: `src/renderer/components/DanmakuSettingsPanel.tsx`，显示、治理、统计三个页签和作用域选择。
- Modify: `tests/danmaku-settings-panel.test.tsx`，覆盖新页签与控件静态渲染。
- Modify: `src/renderer/styles.css`，治理页签、标签、统计卡片和窄屏布局样式。
- Modify: `tests/renderer-build-config.test.ts`，加入治理面板的窄屏 CSS 断言（若现有测试结构适合）。

## Task 1: 建立治理配置模型和失败测试

**Files:**
- Modify: `src/renderer/danmaku/danmaku-settings.ts`
- Test: `tests/danmaku-settings.test.ts`

- [ ] **Step 1: 写默认配置与解析器失败测试**

在 `tests/danmaku-settings.test.ts` 增加断言，要求 `DEFAULT_DANMAKU_SETTINGS.governance` 为：

```ts
{
  enabled: true,
  keywordBlacklist: [],
  duplicateWindowSeconds: 3,
  peakProtectionEnabled: true,
}
```

同时增加以下行为测试：

```ts
expect(parseDanmakuSettings({ governance: {
  enabled: 'yes',
  keywordBlacklist: [' 刷屏 ', '刷屏', '', '  '],
  duplicateWindowSeconds: 0,
  peakProtectionEnabled: false,
} }).governance).toEqual({
  enabled: true,
  keywordBlacklist: ['刷屏'],
  duplicateWindowSeconds: 1,
  peakProtectionEnabled: false,
});

expect(parseDanmakuSettings({ governance: {
  duplicateWindowSeconds: 99,
} }).governance.duplicateWindowSeconds).toBe(10);
```

- [ ] **Step 2: 运行测试确认先失败**

Run: `npm test -- tests/danmaku-settings.test.ts`

Expected: FAIL，因为 `DanmakuSettings` 尚未包含 `governance`，解析器也尚未清洗关键词。

- [ ] **Step 3: 实现最小配置类型和解析器**

在 `src/renderer/danmaku/danmaku-settings.ts` 增加：

```ts
export interface DanmakuGovernanceSettings {
  enabled: boolean;
  keywordBlacklist: string[];
  duplicateWindowSeconds: number;
  peakProtectionEnabled: boolean;
}

export type DanmakuGovernanceOverride = Partial<DanmakuGovernanceSettings>;

export const DEFAULT_DANMAKU_GOVERNANCE: Readonly<DanmakuGovernanceSettings> = {
  enabled: true,
  keywordBlacklist: [],
  duplicateWindowSeconds: 3,
  peakProtectionEnabled: true,
};
```

将 `governance` 加入 `DanmakuSettings` 和默认值。实现 `parseDanmakuGovernanceSettings(value: unknown)`：仅接受最多 50 个关键词，每个关键词取 `trim()` 后丢弃空值，使用 `toLocaleLowerCase('zh-CN')` 去重；将窗口限制在 1–10 秒；非法布尔值回退默认值。`parseDanmakuSettings` 调用该解析器。

- [ ] **Step 4: 运行相关测试确认通过**

Run: `npm test -- tests/danmaku-settings.test.ts`

Expected: PASS，原有显示设置断言和新增治理断言全部通过。

## Task 2: 用 TDD 实现纯治理引擎

**Files:**
- Create: `src/renderer/danmaku/danmaku-governance.ts`
- Test: `tests/danmaku-governance.test.ts`

- [ ] **Step 1: 写治理引擎失败测试**

创建测试消息工厂和固定时间戳，覆盖：

```ts
const settings = {
  enabled: true,
  keywordBlacklist: ['广告'],
  duplicateWindowSeconds: 3,
  peakProtectionEnabled: false,
};

const first = applyDanmakuGovernance(
  [message('1', '请看广告'), message('2', '正常消息')],
  settings,
  createDanmakuGovernanceRuntime(),
  10_000,
);
expect(first.accepted.map((item) => item.id)).toEqual(['2']);
expect(first.stats.filtered).toBe(1);

const duplicate = applyDanmakuGovernance(
  [message('3', '正常消息')],
  settings,
  first.runtime,
  12_999,
);
expect(duplicate.accepted).toEqual([]);
expect(duplicate.stats.duplicates).toBe(1);

const afterWindow = applyDanmakuGovernance(
  [message('4', '正常消息')],
  settings,
  duplicate.runtime,
  13_000,
);
expect(afterWindow.accepted).toHaveLength(1);
```

再用 10 条消息批次构造近 3 秒输入，验证 `getDanmakuPeakLevel` 的三个边界和高峰放行上限：正常不额外丢弃、拥挤最多放行 20 条/秒、爆发最多放行 10 条/秒。验证结果的 `filtered`、`duplicates`、`rateLimited`、`recentRate` 和 `peakRate` 分类互斥且可累加。

- [ ] **Step 2: 运行测试确认先失败**

Run: `npm test -- tests/danmaku-governance.test.ts`

Expected: FAIL，模块和导出函数不存在。

- [ ] **Step 3: 实现纯函数和运行时类型**

在 `src/renderer/danmaku/danmaku-governance.ts` 实现以下公开接口：

```ts
export type DanmakuPeakLevel = 'normal' | 'crowded' | 'burst';

export interface DanmakuGovernanceStats {
  level: DanmakuPeakLevel;
  recentRate: number;
  peakRate: number;
  filtered: number;
  duplicates: number;
  rateLimited: number;
  queueOverflow: number;
  upstreamDropped: number;
}

export interface DanmakuGovernanceRuntime {
  inputTimestamps: number[];
  acceptedTimestamps: number[];
  lastComparableText?: string;
  lastComparableAt?: number;
  peakRate: number;
  stats: DanmakuGovernanceStats;
}

export interface DanmakuGovernanceResult {
  accepted: DanmakuMessage[];
  runtime: DanmakuGovernanceRuntime;
  stats: DanmakuGovernanceStats;
}

export function createDanmakuGovernanceRuntime(): DanmakuGovernanceRuntime;
export function getDanmakuPeakLevel(rate: number): DanmakuPeakLevel;
export function applyDanmakuGovernance(
  messages: DanmakuMessage[],
  settings: DanmakuGovernanceSettings,
  runtime: DanmakuGovernanceRuntime,
  now: number,
): DanmakuGovernanceResult;
```

保留 60 秒输入时间戳和 1 秒放行时间戳；每次调用先删除窗口外的时间戳，再按关键词、重复、高峰顺序处理。关闭治理时直接返回全部消息，并只更新速率窗口，不增加分类丢弃数。关闭高峰保护时仍执行关键词和重复规则。

- [ ] **Step 4: 运行单元测试确认通过**

Run: `npm test -- tests/danmaku-governance.test.ts tests/danmaku-settings.test.ts`

Expected: PASS。

## Task 3: 将治理状态接入弹幕 store

**Files:**
- Modify: `src/renderer/store/danmaku-store.ts`
- Test: `tests/danmaku-store.test.ts`

- [ ] **Step 1: 写 store 集成失败测试**

为 `syncRoom` 增加治理配置参数，并加入以下场景：

```ts
store.getState().syncRoom('63136', true, {
  enabled: true,
  keywordBlacklist: ['广告'],
  duplicateWindowSeconds: 3,
  peakProtectionEnabled: false,
});
store.getState().handleEvent(messages('63136', [
  ['1', '广告'],
  ['2', '正常'],
  ['3', '正常'],
]));
expect(store.getState().rooms['63136'].pending.map((item) => item.id)).toEqual(['2']);
expect(store.getState().rooms['63136'].governanceStats.filtered).toBe(1);
expect(store.getState().rooms['63136'].governanceStats.duplicates).toBe(1);
```

再验证治理统计和 `dropped` 分开累计，配置更新会清理旧 pending 和治理运行时，禁用后会清空 pending、统计和状态。

- [ ] **Step 2: 运行 store 测试确认先失败**

Run: `npm test -- tests/danmaku-store.test.ts`

Expected: FAIL，因为 `governanceStats` 和第三个 `syncRoom` 参数尚不存在。

- [ ] **Step 3: 实现 store 接入**

在 `DanmakuRoomView` 增加：

```ts
governanceStats: DanmakuGovernanceStats;
```

在内部 Map 保存每个房间的 `DanmakuGovernanceRuntime` 和治理配置签名。`syncRoom(roomId, enabled, governance = DEFAULT_DANMAKU_GOVERNANCE)` 在有效配置签名变化时清空 pending、运行时和治理统计，但保留 status；禁用或移除房间时删除内部状态。

`handleEvent` 对每个消息批次调用 `applyDanmakuGovernance`，把结果加入 pending，再执行现有 300 条上限。队列淘汰增加 `governanceStats.queueOverflow`，上游 `event.dropped` 增加 `governanceStats.upstreamDropped`，两者仍同时计入兼容字段 `dropped`。新增 `clearGovernanceStats(roomId)` action，仅清除统计和时间窗口，不影响连接状态或 pending。

- [ ] **Step 4: 更新既有 350 条消息测试并运行**

将原有队列测试改为显式关闭高峰保护，继续验证 300 条上限；新增高峰保护测试验证限流统计。运行：

`npm test -- tests/danmaku-store.test.ts tests/danmaku-governance.test.ts`

Expected: PASS。

## Task 4: 持久化全局设置和房间覆盖

**Files:**
- Modify: `src/renderer/store/workspace-persistence.ts`
- Modify: `src/renderer/store/workspace-store.ts`
- Test: `tests/workspace-persistence.test.ts`
- Test: `tests/workspace-store.test.ts`

- [ ] **Step 1: 写版本 4 和 actions 失败测试**

在持久化测试中构造 `schemaVersion: 3` 快照，断言加载后 `danmakuGovernanceOverrides` 为空，并构造版本 4 快照验证合法覆盖保留、非法覆盖清洗。

在 workspace store 测试中写：

```ts
store.getState().setDanmakuGovernance({ keywordBlacklist: ['广告'] });
store.getState().setRoomDanmakuGovernanceOverride('63136', {
  duplicateWindowSeconds: 5,
});
expect(store.getState().danmakuSettings.governance.keywordBlacklist).toEqual(['广告']);
expect(store.getState().danmakuGovernanceOverrides['63136']).toEqual({
  duplicateWindowSeconds: 5,
});
store.getState().clearRoomDanmakuGovernanceOverride('63136');
expect(store.getState().danmakuGovernanceOverrides['63136']).toBeUndefined();
```

- [ ] **Step 2: 运行测试确认先失败**

Run: `npm test -- tests/workspace-persistence.test.ts tests/workspace-store.test.ts`

Expected: FAIL，因为快照仍为版本 3，state 没有覆盖字段和 actions。

- [ ] **Step 3: 实现版本 4 迁移和 workspace actions**

在 `WorkspaceSnapshot` 增加：

```ts
danmakuGovernanceOverrides: Record<string, DanmakuGovernanceOverride>;
```

将 `WORKSPACE_SCHEMA_VERSION` 改为 4，允许读取版本 1、2、3、4。旧版本使用空覆盖；版本 4 只接受存在于 `roomLibrary` 的 roomId，复用治理解析器清洗每个 Partial 覆盖。保存快照时写入版本 4 字段。

在 `WorkspaceState` 增加：

```ts
danmakuGovernanceOverrides: Record<string, DanmakuGovernanceOverride>;
setDanmakuGovernance(patch: Partial<DanmakuGovernanceSettings>): void;
setRoomDanmakuGovernanceOverride(roomId: string, patch: DanmakuGovernanceOverride): void;
clearRoomDanmakuGovernanceOverride(roomId: string): void;
```

全局 action 通过 `parseDanmakuSettings` 合并并持久化；房间 action 只接受当前 roomLibrary 中的房间，并删除空覆盖。删除房间时同步删除对应覆盖。

- [ ] **Step 4: 运行持久化和 workspace 测试确认通过**

Run: `npm test -- tests/workspace-persistence.test.ts tests/workspace-store.test.ts`

Expected: PASS，且既有房间、收藏、分组、布局和弹幕显示设置断言不回归。

## Task 5: 将有效配置同步到 DanmakuProvider

**Files:**
- Modify: `src/renderer/store/danmaku-context.tsx`
- Test: `tests/danmaku-settings.test.ts`

- [ ] **Step 1: 写有效配置合并失败测试**

在 `tests/danmaku-settings.test.ts` 增加 `resolveDanmakuGovernance` 的测试，断言房间覆盖只替换指定字段，其余字段沿用全局值：

```ts
expect(resolveDanmakuGovernance(
  {
    enabled: true,
    keywordBlacklist: ['广告'],
    duplicateWindowSeconds: 3,
    peakProtectionEnabled: true,
  },
  { duplicateWindowSeconds: 5 },
)).toEqual({
  enabled: true,
  keywordBlacklist: ['广告'],
  duplicateWindowSeconds: 5,
  peakProtectionEnabled: true,
});
```

同时断言空覆盖等于全局配置，非法覆盖在合并前回退到解析器默认值。

- [ ] **Step 2: 运行测试确认先失败**

Run: `npm test -- tests/danmaku-settings.test.ts`

Expected: FAIL，因为 `resolveDanmakuGovernance` 尚未导出。

- [ ] **Step 3: 实现 provider 配置解析和同步**

在 `src/renderer/danmaku/danmaku-settings.ts` 增加 `resolveDanmakuGovernance(global, override)`，先解析全局配置，再对 Partial 覆盖逐字段解析并合并。provider 读取 `danmakuSettings.governance` 与 `danmakuGovernanceOverrides`，对每个房间调用该函数后传入 `syncRoom(roomId, shouldTrack, effectiveGovernance)`。将治理字段加入 effect 依赖，配置切换时重新同步但不重启 source 连接。新增 `useDanmakuGovernanceStats(roomIdsKey)` selector，按稳定的 room ID 列表汇总统计，供全局统计页使用。

- [ ] **Step 4: 运行配置、provider 和全弹幕测试**

Run: `npm test -- tests/danmaku-settings.test.ts tests/danmaku-store.test.ts tests/renderer-danmaku-source.test.ts`

Expected: PASS。

## Task 6: 用静态渲染测试扩展三页签设置面板

**Files:**
- Modify: `src/renderer/components/DanmakuSettingsPanel.tsx`
- Modify: `tests/danmaku-settings-panel.test.tsx`
- Modify: `src/renderer/styles.css`

- [ ] **Step 1: 写面板失败测试**

在现有静态渲染测试中断言默认打开面板包含：

```ts
for (const label of ['显示', '治理', '统计', '全局默认', '关键词黑名单', '重复抑制窗口', '高峰保护', '过滤', '限流']) {
  expect(html).toContain(label);
}
expect(html).toContain('data-tab="display"');
expect(html).toContain('data-tab="governance"');
expect(html).toContain('data-tab="stats"');
```

保留现有七项显示设置、三个 range 和三个重置按钮断言。

- [ ] **Step 2: 运行面板测试确认先失败**

Run: `npm test -- tests/danmaku-settings-panel.test.tsx`

Expected: FAIL，因为当前面板没有页签、治理控件和统计内容。

- [ ] **Step 3: 实现页签和治理交互**

在组件中使用本地 `activeTab` 与 `selectedScope` 状态。作用域为 `global` 或房间 ID；房间下拉选项来自 `useWorkspace(state => state.rooms)`。治理页读取全局设置或对应覆盖，关键词以标签显示，输入框支持 Enter 和逗号提交，按钮支持删除标签；改动立即调用对应 workspace action。房间作用域显示“继承全局”按钮，清除覆盖后重新显示有效全局值。

统计页从 `useDanmakuRoom` 读取所选房间统计；全局作用域使用 `useDanmakuGovernanceStats(roomIdsKey)` 求和并取最大峰值。没有房间时显示空状态。清零按钮调用 `clearGovernanceStats`，不修改持久化设置。

页签按钮使用 `aria-selected`，面板保持 `role="dialog"` 和已有 Escape/外部点击关闭行为。

- [ ] **Step 4: 添加最小样式并运行面板测试**

在 `src/renderer/styles.css` 增加 `.danmaku-settings-tabs`、`.danmaku-governance-form`、`.danmaku-keyword-list`、`.danmaku-stat-grid` 和 `.danmaku-stat-card`，使用现有颜色变量和 8px 内圆角。移动端将面板宽度限制为视口减 24px，统计卡片改为两列。

Run: `npm test -- tests/danmaku-settings-panel.test.tsx tests/renderer-build-config.test.ts`

Expected: PASS。

## Task 7: 完成跨模块回归测试和浏览器验证

**Files:**
- Modify: `tests/danmaku-store.test.ts`
- Modify: `tests/workspace-store.test.ts`
- Modify: `tests/workspace-persistence.test.ts`
- Modify: `tests/danmaku-settings-panel.test.tsx`

- [ ] **Step 1: 运行弹幕相关测试集合**

Run: `npm test -- tests/danmaku-*.test.ts tests/danmaku-*.test.tsx tests/workspace-persistence.test.ts tests/workspace-store.test.ts`

Expected: 0 failed tests。

- [ ] **Step 2: 运行全量测试、类型检查和生产构建**

Run:

```text
npm test
npm run typecheck
npm run build
```

Expected：三个命令退出码均为 0；Vitest 报告 0 failed，TypeScript 无诊断，Vite/Electron 三个构建产物生成成功。

- [ ] **Step 3: 启动开发页面进行桌面和窄屏检查**

Run: `npm run dev -- --host 127.0.0.1`

用浏览器打开终端输出的地址，验证：

1. 设置面板可以在显示、治理、统计间切换。
2. 全局规则修改后，两个在线房间都使用新规则。
3. 指定房间覆盖只影响该房间，清除覆盖后恢复全局值。
4. 连续相同文本在 3 秒内只出现一次；关键词命中不显示。
5. 快速注入大量弹幕时，状态切换到拥挤或爆发，弹幕仍持续滚动，统计数增加且 pending 不超过 300。
6. 视口宽度 1280px 和 390px 时，面板页签、关键词标签、统计卡片不横向溢出；浏览器控制台无新增错误。

- [ ] **Step 4: 检查最终差异并保持提交范围**

Run: `git diff --check; git status --short`

确认只包含本功能相关源文件、测试、设计和计划文档；不修改播放源、签名、token、cookie 或用户已有的无关改动。

## 计划自检

- 设计中的全局默认、房间覆盖、普通包含匹配、3 秒重复窗口、自适应三级限流、近 60 秒统计和三页签均有对应任务。
- 所有新增生产接口先在 Task 1–6 的失败测试中定义，再实现。
- 快照版本 4、旧版本迁移和非法配置回退均有测试步骤。
- 300 条队列保护、上游 dropped 分离、禁用清理和连续飘屏回归均有覆盖。
