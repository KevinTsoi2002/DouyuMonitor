# 自动推荐布局实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在新增房间后让自动模式按房间数量切换推荐网格，同时保持手动布局选择不被覆盖。

**Architecture:** 使用现有 `layoutId` 作为单一状态来源，新增 `auto` 模式；领域层提供推荐布局和解析函数，store 只保存模式，渲染层把自动模式解析成稳定的 CSS 布局类。快照白名单增加 `auto` 并保留旧具体布局的兼容行为。

**Tech Stack:** TypeScript, React, Zustand vanilla store, Vitest, Electron/Vite, Playwright。

---

### Task 1: 增加自动布局领域规则

**Files:**
- Modify: `src/domain/layout-engine.ts`
- Modify: `tests/layout-engine.test.ts`

- [ ] **Step 1: 写自动推荐的失败测试**

在 `tests/layout-engine.test.ts` 增加：

```ts
import { calculateLayout, getRecommendedLayoutId, resolveLayoutId } from '../src/domain/layout-engine';

it('recommends stable layouts for every supported room count', () => {
  expect([0, 1, 2, 4, 5, 6, 7, 9].map(getRecommendedLayoutId)).toEqual([
    'single', 'single', 'grid-2x2', 'grid-2x2',
    'grid-3x2', 'grid-3x2', 'grid-3x3', 'grid-3x3',
  ]);
});

it('resolves auto layout before calculating slots', () => {
  expect(resolveLayoutId('auto', 5)).toBe('grid-3x2');
  expect(calculateLayout(['a', 'b', 'c', 'd', 'e'], 'auto')).toEqual(
    calculateLayout(['a', 'b', 'c', 'd', 'e'], 'grid-3x2'),
  );
});
```

- [ ] **Step 2: 运行领域测试确认 RED**

运行：`npm test -- --run tests/layout-engine.test.ts`

预期：失败，原因是 `getRecommendedLayoutId` 和 `resolveLayoutId` 尚不存在，且 `auto` 尚未进入布局分支。

- [ ] **Step 3: 写最小实现**

在 `src/domain/layout-engine.ts` 中：

```ts
export type LayoutId = 'auto' | 'single' | 'grid-2x2' | 'grid-3x2' | 'grid-3x3'
  | 'primary-two' | 'split-horizontal' | 'split-vertical' | string;

export function getRecommendedLayoutId(roomCount: number): LayoutId {
  if (roomCount <= 1) return 'single';
  if (roomCount <= 4) return 'grid-2x2';
  if (roomCount <= 6) return 'grid-3x2';
  return 'grid-3x3';
}

export function resolveLayoutId(layoutId: LayoutId, roomCount: number): LayoutId {
  return layoutId === 'auto' ? getRecommendedLayoutId(roomCount) : layoutId;
}
```

在 `calculateLayout` 开头将 `auto` 解析为 `resolvedLayoutId`，再对解析后的 ID 执行原有 switch；未知 ID 仍走原有自适应兜底。

- [ ] **Step 4: 运行领域测试确认 GREEN**

运行：`npm test -- --run tests/layout-engine.test.ts`

预期：全部布局测试通过。

### Task 2: 让 store 和快照理解自动模式

**Files:**
- Modify: `src/renderer/store/workspace-persistence.ts`
- Modify: `src/renderer/store/workspace-store.ts`
- Modify: `tests/workspace-persistence.test.ts`
- Modify: `tests/workspace-store.test.ts`

- [ ] **Step 1: 写快照和 store 的失败测试**

在 `tests/workspace-persistence.test.ts` 增加自动模式快照往返：

```ts
it('round-trips the automatic layout mode', () => {
  const storage = createMemoryStorage();
  saveWorkspaceSnapshot(storage, { ...snapshot, layoutId: 'auto' });
  expect(loadWorkspaceSnapshot(storage)?.layoutId).toBe('auto');
});
```

在 `tests/workspace-store.test.ts` 增加：

```ts
it('starts new workspaces in automatic layout mode and keeps it after adding rooms', () => {
  const store = createWorkspaceStore(createMockDouyuAdapter());
  expect(store.getState().layoutId).toBe('auto');
  store.getState().addRoom(candidate('101'));
  store.getState().addRoom(candidate('202'));
  expect(store.getState().layoutId).toBe('auto');
});

it('preserves a manually locked layout when a room is added', () => {
  const store = createWorkspaceStore(createMockDouyuAdapter(), { initialRooms: [candidate('101')] });
  store.getState().setLayout('split-vertical');
  store.getState().addRoom(candidate('202'));
  expect(store.getState().layoutId).toBe('split-vertical');
});

it('returns to automatic recommendations when the user selects auto', () => {
  const store = createWorkspaceStore(createMockDouyuAdapter(), { initialRooms: [candidate('101')] });
  store.getState().setLayout('primary-two');
  store.getState().setLayout('auto');
  expect(store.getState().layoutId).toBe('auto');
});
```

- [ ] **Step 2: 运行快照和 store 测试确认 RED**

运行：`npm test -- --run tests/workspace-persistence.test.ts tests/workspace-store.test.ts`

预期：自动快照被判为非法，且新 store 仍返回 `single` 或已有具体布局。

- [ ] **Step 3: 写最小实现**

在 `workspace-persistence.ts` 的 `LAYOUT_VALUES` 中加入 `'auto'`。在 `workspace-store.ts` 中将无持久化快照时的默认值改为 `'auto'`；`addRoom` 只追加 session 和焦点字段，不修改 `layoutId`，从而自动模式保持自动、手动模式保持手动。

- [ ] **Step 4: 运行快照和 store 测试确认 GREEN**

运行：`npm test -- --run tests/workspace-persistence.test.ts tests/workspace-store.test.ts`

预期：新增测试和既有持久化、房间上限、焦点、异步播放能力测试全部通过。

### Task 3: 接入布局菜单、网格渲染和样式

**Files:**
- Modify: `src/renderer/ui-model.ts`
- Modify: `src/renderer/components/WorkspaceGrid.tsx`
- Modify: `src/renderer/styles.css`
- Modify: `tests/ui-model.test.ts`
- Modify: `tests/app-smoke.test.tsx`

- [ ] **Step 1: 写 UI 模型和渲染回归测试**

在 `tests/ui-model.test.ts` 中把布局 ID 期望更新为自动选项在首位，并增加：

```ts
it('describes automatic layout mode as a selectable option', () => {
  expect(LAYOUT_OPTIONS[0]).toEqual(expect.objectContaining({
    id: 'auto',
    label: expect.any(String),
    hint: expect.any(String),
  }));
});
```

在 `tests/app-smoke.test.tsx` 增加单房间自动模式的静态渲染断言：

```ts
it('resolves automatic layout to a stable CSS layout class', () => {
  const html = renderToStaticMarkup(
    <WorkspaceProvider adapter={createRendererDouyuAdapter()} demoMode initialRooms={MOCK_ROOM_CANDIDATES.slice(0, 1)}>
      <App />
    </WorkspaceProvider>,
  );
  expect(html).toContain('workspace-grid layout-single');
});
```

- [ ] **Step 2: 运行 UI 测试确认 RED**

运行：`npm test -- --run tests/ui-model.test.ts tests/app-smoke.test.tsx`

预期：布局选项列表首项仍是 `single`，自动 CSS 类断言失败。

- [ ] **Step 3: 写最小 UI 实现**

在 `ui-model.ts` 的 `LAYOUT_OPTIONS` 首位加入：

```ts
{ id: 'auto', label: '自动推荐', shortLabel: '自动', hint: '新增房间时自动适配布局' }
```

在 `WorkspaceGrid.tsx` 使用 `resolveLayoutId(layoutId, rooms.length)` 计算 `resolvedLayoutId`，并将该值传给 `calculateLayout` 和 `layout-${resolvedLayoutId}` CSS 类；菜单仍使用原始 `layoutId` 判断“自动/手动”选中状态。这样自动模式会得到稳定的 `layout-single`、`layout-grid-2x2` 等样式。

在 `styles.css` 为自动解析后的布局保留现有具体布局规则，并在移动端选择器中不引入 `layout-auto` 依赖；删除或新增房间不会因为 CSS 类短暂变化导致尺寸漂移。

- [ ] **Step 4: 运行 UI 测试确认 GREEN**

运行：`npm test -- --run tests/ui-model.test.ts tests/app-smoke.test.tsx`

预期：布局选项、自动 CSS 类和既有应用壳测试全部通过。

### Task 4: 集成验证和文档同步

**Files:**
- Update externally: Notion development design page (`https://app.notion.com/p/3b40a2056c2981b1abc7cd80cb0e231d`)

- [ ] **Step 1: 运行完整自动化检查**

依次运行：

```powershell
npm test
npm run typecheck
npm run build
```

预期：Vitest、TypeScript 和 Electron 三个构建产物均成功生成且无错误。

- [ ] **Step 2: 用 Playwright 验证新增房间流程**

启动开发窗口，在自动模式下顺序新增 1、2、4、5、7、9 个房间，检查布局菜单仍显示“自动”，网格类分别为 `layout-single`、`layout-grid-2x2`、`layout-grid-3x2`、`layout-grid-3x3`，且房间数量与可见 tile 数一致。随后手动选择“纵向分屏”，再新增房间，确认 `layoutId` 和网格类不被自动模式改写；重新选择“自动推荐”后确认恢复当前数量对应的网格。

- [ ] **Step 3: 更新 Notion 开发设计记录**

在既有开发设计页追加“自动推荐布局”小节，记录状态模型、四档推荐规则、手动锁定行为、测试命令和实际验证结果；不记录播放 URL、token、cookie 或其他敏感数据。更新后重新读取页面确认内容已写入。

- [ ] **Step 4: 做最终 diff 和敏感信息检查**

运行：`git diff -- src docs tests`（若当前目录仍无 Git 仓库，则改为逐文件检查 `git` 不可用原因并使用 `rg` 搜索 `wsAuth|token=|cookie|playbackUrl` 仅确认本次新增文档和测试没有泄露敏感值）。

预期：只有自动布局相关文件发生变化，测试、构建和 UI 验证结果与报告一致。
