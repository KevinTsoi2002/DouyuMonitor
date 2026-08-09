# 直播间资料库与紧凑播放器实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** 增加主播头像、历史、收藏和可切换自定义分组，并压缩应用顶部和直播画布界面，在鼠标静止 3 秒后隐藏播放器控件。

**Architecture:** 将可持久化的直播间资料库作为房间元数据和播放偏好的唯一来源，当前画面只保存房间号顺序并生成运行时会话。历史、收藏和分组保存房间号引用；分组切换由 store 原子替换当前会话。界面拆分为头像、资料库视图、分组管理和播放器控件可见性模块。

**Tech Stack:** TypeScript、React 19、Zustand、Vitest、Vite、Electron、Playwright

---

## 文件结构

- 修改 `src/domain/douyu-adapter.ts`：为房间候选增加可选头像地址。
- 修改 `src/infrastructure/douyu-http-adapter.ts`：从两个斗鱼公开接口解析并校验头像。
- 修改 `tests/fixtures/douyu-room-api.json`、`tests/fixtures/douyu-search-api.json`：补充头像字段。
- 修改 `tests/douyu-http-adapter.test.ts`：覆盖合法和非法头像。
- 新建 `src/renderer/store/room-library.ts`：定义资料库、历史、分组类型和纯数据操作。
- 新建 `tests/room-library.test.ts`：覆盖历史、收藏引用、组内顺序和容量。
- 修改 `src/renderer/store/workspace-persistence.ts`：升级快照并迁移 v1/v2。
- 修改 `tests/workspace-persistence.test.ts`：覆盖新快照和旧数据迁移。
- 修改 `src/renderer/store/workspace-store.ts`：接入资料库并增加收藏、历史、分组动作。
- 修改 `tests/workspace-store.test.ts`：覆盖快速添加、分组切换和焦点修复。
- 新建 `src/renderer/components/RoomAvatar.tsx`：渲染头像及首字回退。
- 新建 `tests/room-avatar.test.tsx`：覆盖图片和回退结构。
- 新建 `src/renderer/components/RoomLibraryView.tsx`：渲染收藏与历史快速添加列表。
- 新建 `src/renderer/components/GroupManagerDialog.tsx`：管理分组名称、成员和顺序。
- 修改 `src/renderer/components/RoomSidebar.tsx`：组合分组标签、资料库入口和当前房间列表。
- 修改 `src/renderer/components/AddRoomDialog.tsx`：搜索结果使用主播头像。
- 新建 `tests/room-sidebar.test.tsx`：覆盖列表文案、状态颜色和资料库入口。
- 新建 `src/renderer/player-controls-visibility.ts`：集中 3 秒自动隐藏计时规则。
- 新建 `tests/player-controls-visibility.test.ts`：用假计时器覆盖隐藏与锁定。
- 修改 `src/renderer/components/RoomTile.tsx`：接入鼠标、焦点、菜单和触摸控制。
- 修改 `tests/app-smoke.test.tsx`：覆盖紧凑顶部与已删除文案。
- 修改 `src/renderer/components/AppHeader.tsx`、`src/renderer/components/WorkspaceGrid.tsx`：删除中央状态与画布说明。
- 修改 `src/renderer/App.tsx`：接入分组管理弹窗的打开状态。
- 修改 `src/renderer/styles.css`：实现紧凑布局、头像、分组、资料库、弹窗和控件淡出。

## Task 1：解析主播头像

**Files:**
- Modify: `src/domain/douyu-adapter.ts`
- Modify: `src/infrastructure/douyu-http-adapter.ts`
- Modify: `tests/fixtures/douyu-room-api.json`
- Modify: `tests/fixtures/douyu-search-api.json`
- Test: `tests/douyu-http-adapter.test.ts`

- [x] **Step 1: 在公开接口夹具中加入头像**

房间接口使用：

```json
"avatar": "https://apic.douyucdn.cn/upload/avatar/example_big.jpg"
```

搜索结果使用：

```json
"avatar": "https://apic.douyucdn.cn/upload/avatar/search_big.jpg"
```

- [x] **Step 2: 写头像映射红灯测试**

在两个现有映射断言中分别加入：

```ts
avatarUrl: 'https://apic.douyucdn.cn/upload/avatar/example_big.jpg',
```

和：

```ts
avatarUrl: 'https://apic.douyucdn.cn/upload/avatar/search_big.jpg',
```

再加入非法协议测试：

```ts
it('drops non-http avatar protocols', async () => {
  const adapter = createDouyuHttpAdapter({
    fetch: async () => Response.json({
      error: 0,
      data: {
        room_id: '1', room_name: '房间', owner_name: '主播',
        cate_name: '综合', room_status: '1', online: 1,
        avatar: 'javascript:alert(1)',
      },
    }),
  });

  await expect(adapter.search({ type: 'room-id', value: '1' }))
    .resolves.toEqual([expect.not.objectContaining({ avatarUrl: expect.anything() })]);
});
```

- [x] **Step 3: 运行测试确认因头像字段缺失而失败**

Run: `npx vitest run tests/douyu-http-adapter.test.ts`

Expected: 两个映射断言缺少 `avatarUrl`。

- [x] **Step 4: 增加类型和安全解析函数**

在 `RoomCandidate` 中加入：

```ts
avatarUrl?: string;
```

在 HTTP adapter 中加入：

```ts
function httpUrl(value: unknown): string | undefined {
  const text = scalarString(value);
  if (!text) return undefined;
  try {
    const url = new URL(text);
    return url.protocol === 'http:' || url.protocol === 'https:' ? url.toString() : undefined;
  } catch {
    return undefined;
  }
}
```

两个候选映射对象只在 `httpUrl(...)` 有值时展开 `avatarUrl`：

```ts
const avatarUrl = httpUrl(payload.data.avatar);
return { roomId, anchorName, title, category, online, viewerLabel, ...(avatarUrl ? { avatarUrl } : {}) };
```

- [x] **Step 5: 运行头像测试**

Run: `npx vitest run tests/douyu-http-adapter.test.ts`

Expected: 文件内全部测试通过。

## Task 2：建立资料库纯数据模型

**Files:**
- Create: `src/renderer/store/room-library.ts`
- Test: `tests/room-library.test.ts`

- [x] **Step 1: 写历史和分组红灯测试**

```ts
import { describe, expect, it } from 'vitest';
import { addHistoryEntry, addRoomIdToGroup, toggleRoomId } from '../src/renderer/store/room-library';

describe('room library helpers', () => {
  it('deduplicates history and keeps the newest fifty rooms', () => {
    let history = [];
    for (let index = 0; index < 51; index += 1) {
      history = addHistoryEntry(history, String(index), `2026-08-10T00:00:${String(index).padStart(2, '0')}Z`);
    }
    history = addHistoryEntry(history, '10', '2026-08-10T01:00:00Z');
    expect(history).toHaveLength(50);
    expect(history[0]).toEqual({ roomId: '10', addedAt: '2026-08-10T01:00:00Z' });
    expect(new Set(history.map((item) => item.roomId)).size).toBe(50);
  });

  it('toggles favorite ids without duplicates', () => {
    expect(toggleRoomId(['1'], '1')).toEqual([]);
    expect(toggleRoomId([], '1')).toEqual(['1']);
  });

  it('keeps group order and enforces nine rooms', () => {
    let ids: string[] = [];
    for (let index = 1; index <= 9; index += 1) {
      expect(addRoomIdToGroup(ids, String(index))).toMatchObject({ result: 'added' });
      ids = addRoomIdToGroup(ids, String(index)).roomIds;
    }
    expect(addRoomIdToGroup(ids, '10')).toMatchObject({ result: 'limit', roomIds: ids });
    expect(addRoomIdToGroup(ids, '9')).toMatchObject({ result: 'duplicate', roomIds: ids });
  });
});
```

- [x] **Step 2: 运行测试确认模块不存在**

Run: `npx vitest run tests/room-library.test.ts`

Expected: FAIL，无法导入 `room-library`。

- [x] **Step 3: 实现资料库类型与纯函数**

```ts
import type { RoomCandidate, StreamQuality } from '../../domain/douyu-adapter';

export const MAX_HISTORY_ROOMS = 50;
export const MAX_GROUP_ROOMS = 9;

export interface LibraryRoom extends RoomCandidate {
  quality: StreamQuality;
  danmakuEnabled: boolean;
  volume: number;
}
export interface RoomHistoryEntry { roomId: string; addedAt: string }
export interface RoomGroup { id: string; name: string; roomIds: string[]; createdAt: string }
export type RoomLibrary = Record<string, LibraryRoom>;

export function addHistoryEntry(history: RoomHistoryEntry[], roomId: string, addedAt: string): RoomHistoryEntry[] {
  return [{ roomId, addedAt }, ...history.filter((item) => item.roomId !== roomId)]
    .slice(0, MAX_HISTORY_ROOMS);
}

export function toggleRoomId(roomIds: string[], roomId: string): string[] {
  return roomIds.includes(roomId) ? roomIds.filter((id) => id !== roomId) : [roomId, ...roomIds];
}

export function addRoomIdToGroup(roomIds: string[], roomId: string) {
  if (roomIds.includes(roomId)) return { result: 'duplicate' as const, roomIds };
  if (roomIds.length >= MAX_GROUP_ROOMS) return { result: 'limit' as const, roomIds };
  return { result: 'added' as const, roomIds: [...roomIds, roomId] };
}
```

- [x] **Step 4: 运行纯函数测试**

Run: `npx vitest run tests/room-library.test.ts`

Expected: 3 tests passed。

## Task 3：升级持久化快照并迁移旧数据

**Files:**
- Modify: `src/renderer/store/workspace-persistence.ts`
- Modify: `tests/workspace-persistence.test.ts`

- [x] **Step 1: 写 v3 round-trip 与 v2 迁移红灯测试**

新快照断言包含：

```ts
schemaVersion: 3,
roomLibrary: { '63136': expect.objectContaining({ roomId: '63136' }) },
activeRoomIds: ['63136'],
history: [{ roomId: '63136', addedAt: '2026-08-10T00:00:00.000Z' }],
favoriteRoomIds: ['63136'],
groups: [{ id: 'group-1', name: '赛事', roomIds: ['63136'], createdAt: '2026-08-10T00:00:00.000Z' }],
activeGroupId: 'group-1',
```

迁移断言要求旧 `rooms` 转换为 `roomLibrary` 和 `activeRoomIds`，历史、收藏、分组为空。

- [x] **Step 2: 运行测试确认版本和字段不匹配**

Run: `npx vitest run tests/workspace-persistence.test.ts`

Expected: FAIL，当前版本为 2 且没有资料库字段。

- [x] **Step 3: 定义 v3 快照**

```ts
export const WORKSPACE_SCHEMA_VERSION = 3;

export interface WorkspaceSnapshot {
  schemaVersion: 3;
  roomLibrary: Record<string, PersistedRoom>;
  activeRoomIds: string[];
  history: RoomHistoryEntry[];
  favoriteRoomIds: string[];
  groups: RoomGroup[];
  activeGroupId?: string;
  layoutId: LayoutId;
  primaryRoomId?: string;
  audioRoomId?: string;
  globalDanmakuEnabled: boolean;
  globalMuted: boolean;
  danmakuSettings: DanmakuSettings;
  sidebarOpen?: boolean;
}
```

`PersistedRoom` 改为 `LibraryRoom` 的类型别名，避免资料库与持久化模块互相导入。`parseRoom` 保留合法 `avatarUrl`。解析时以 `roomLibrary` 为准，过滤不存在的活动房间、收藏、历史和组成员，组成员去重并截取 9 项；历史按输入顺序去重并截取 50 项。v1/v2 的 `rooms` 迁移为资料库与活动房间号。

- [x] **Step 4: 运行持久化测试**

Run: `npx vitest run tests/workspace-persistence.test.ts`

Expected: 文件内全部测试通过，并确认序列化文本不含 `playbackUrl` 或 `token`。

## Task 4：在 Workspace Store 中接入资料库、收藏、历史和分组

**Files:**
- Modify: `src/renderer/store/workspace-store.ts`
- Modify: `tests/workspace-store.test.ts`

- [x] **Step 1: 为测试注入稳定时间和分组 ID**

扩展 options：

```ts
now?: () => Date;
createGroupId?: () => string;
```

测试使用：

```ts
const options = {
  now: () => new Date('2026-08-10T00:00:00.000Z'),
  createGroupId: () => 'group-1',
};
```

- [x] **Step 2: 写收藏、历史和分组切换红灯测试**

```ts
it('records successful additions and toggles favorites', () => {
  const store = createWorkspaceStore(createMockDouyuAdapter(), options);
  store.getState().addRoom(candidate('1'));
  store.getState().toggleFavorite('1');
  expect(store.getState().history).toEqual([{ roomId: '1', addedAt: '2026-08-10T00:00:00.000Z' }]);
  expect(store.getState().favoriteRoomIds).toEqual(['1']);
});

it('replaces active sessions when switching groups', () => {
  const store = createWorkspaceStore(createMockDouyuAdapter(), { ...options, initialRooms: [candidate('1'), candidate('2')] });
  const groupId = store.getState().createGroup('赛事');
  store.getState().addRoomToGroup(groupId!, '2');
  store.getState().switchGroup(groupId!);
  expect(store.getState().rooms.map((room) => room.roomId)).toEqual(['2']);
  expect(store.getState()).toEqual(expect.objectContaining({ activeGroupId: groupId, primaryRoomId: '2', audioRoomId: '2' }));
});

it('clears the active group after a temporary room change', () => {
  const store = createWorkspaceStore(createMockDouyuAdapter(), { ...options, initialRooms: [candidate('1'), candidate('2')] });
  const groupId = store.getState().createGroup('赛事')!;
  store.getState().addRoomToGroup(groupId, '1');
  store.getState().switchGroup(groupId);
  store.getState().addRoom(candidate('2'));
  expect(store.getState().activeGroupId).toBeUndefined();
});
```

- [x] **Step 3: 运行 store 测试确认动作不存在**

Run: `npx vitest run tests/workspace-store.test.ts`

Expected: FAIL，缺少资料库状态和动作。

- [x] **Step 4: 扩展状态与动作接口**

```ts
roomLibrary: RoomLibrary;
history: RoomHistoryEntry[];
favoriteRoomIds: string[];
groups: RoomGroup[];
activeGroupId?: string;
toggleFavorite: (roomId: string) => void;
createGroup: (name: string) => string | undefined;
renameGroup: (groupId: string, name: string) => boolean;
deleteGroup: (groupId: string) => void;
addRoomToGroup: (groupId: string, roomId: string) => 'added' | 'duplicate' | 'limit' | 'missing';
removeRoomFromGroup: (groupId: string, roomId: string) => void;
moveGroupRoom: (groupId: string, roomId: string, delta: -1 | 1) => void;
switchGroup: (groupId: string) => void;
```

- [x] **Step 5: 实现统一写入和分组原子切换**

增加内部 `persist()`，只保存 `roomLibrary` 与 `activeRoomIds`。`addRoom` 成功后合并候选资料、保存默认播放偏好、更新历史并清除 `activeGroupId`。房间清晰度、音量和弹幕变化同时更新运行时会话和 `roomLibrary`。

`switchGroup` 使用以下顺序：

```ts
const group = get().groups.find((item) => item.id === groupId);
if (!group) return;
const rooms = group.roomIds.flatMap((roomId) => {
  const room = get().roomLibrary[roomId];
  return room ? [toSession(room)] : [];
});
for (const room of get().rooms) registry.remove(room.roomId);
for (const room of rooms) registry.add({ roomId: room.roomId, anchorName: room.anchorName });
set({ rooms, activeGroupId: groupId, primaryRoomId: rooms[0]?.roomId, audioRoomId: rooms[0]?.roomId });
persist();
for (const room of rooms.filter((item) => item.online)) void get().refreshStreamAvailability(room.roomId);
```

- [x] **Step 6: 运行 store 与持久化测试**

Run: `npx vitest run tests/workspace-store.test.ts tests/workspace-persistence.test.ts tests/room-library.test.ts`

Expected: 全部通过。

## Task 5：渲染主播头像并改造搜索结果

**Files:**
- Create: `src/renderer/components/RoomAvatar.tsx`
- Modify: `src/renderer/components/AddRoomDialog.tsx`
- Test: `tests/room-avatar.test.tsx`
- Modify: `tests/app-smoke.test.tsx`

- [x] **Step 1: 写头像 SSR 红灯测试**

```tsx
it('renders an image with an accessible anchor name', () => {
  const html = renderToStaticMarkup(<RoomAvatar anchorName="示例主播" avatarUrl="https://example.com/avatar.jpg" />);
  expect(html).toContain('src="https://example.com/avatar.jpg"');
  expect(html).toContain('alt="示例主播"');
});

it('renders initials when no avatar is available', () => {
  const html = renderToStaticMarkup(<RoomAvatar anchorName="示例主播" />);
  expect(html).toContain('示例');
  expect(html).not.toContain('<img');
});
```

- [x] **Step 2: 运行测试确认组件不存在**

Run: `npx vitest run tests/room-avatar.test.tsx`

Expected: FAIL，无法导入组件。

- [x] **Step 3: 实现带加载失败回退的头像组件**

组件用 `useState(Boolean(avatarUrl))` 控制图片；`onError={() => setImageVisible(false)}` 后渲染 `getRoomInitials(anchorName)`。图片使用 `referrerPolicy="no-referrer"` 和 `draggable={false}`。

- [x] **Step 4: 搜索结果改用 RoomAvatar**

将 `AddRoomDialog` 中的首字头像替换为：

```tsx
<RoomAvatar anchorName={room.anchorName} avatarUrl={room.avatarUrl} size="small" />
```

- [x] **Step 5: 运行头像和应用烟雾测试**

Run: `npx vitest run tests/room-avatar.test.tsx tests/app-smoke.test.tsx`

Expected: 全部通过。

## Task 6：实现直播间列表、收藏、历史和分组管理界面

**Files:**
- Create: `src/renderer/components/RoomLibraryView.tsx`
- Create: `src/renderer/components/GroupManagerDialog.tsx`
- Modify: `src/renderer/components/RoomSidebar.tsx`
- Modify: `src/renderer/App.tsx`
- Test: `tests/room-sidebar.test.tsx`
- Modify: `tests/app-smoke.test.tsx`

- [x] **Step 1: 写侧栏结构红灯测试**

通过 `WorkspaceProvider` 与 `renderToStaticMarkup` 渲染 `RoomSidebar`，断言：

```ts
expect(html).toContain('直播间列表');
expect(html).toContain('收藏');
expect(html).toContain('历史');
expect(html).toContain('管理分组');
expect(html).not.toContain('监看列表');
expect(html).not.toContain('路信号');
expect(html).not.toContain('点击主播设为主画面');
expect(html).not.toContain('12.8 万');
```

离线房间断言包含 `status-dot-offline`，在线房间包含 `status-dot-live`。

- [x] **Step 2: 运行侧栏测试确认旧文案和结构失败**

Run: `npx vitest run tests/room-sidebar.test.tsx tests/app-smoke.test.tsx`

Expected: FAIL，仍渲染旧标题、摘要和观看人数。

- [x] **Step 3: 实现资料库快速添加视图**

`RoomLibraryView` 接收 `mode: 'favorites' | 'history'`，从 `roomLibrary` 解析房间。收藏保持 `favoriteRoomIds` 顺序，历史按 `history` 顺序。每行显示 `RoomAvatar`、主播名、分区、在线状态和添加按钮。已在当前画面的房间禁用添加按钮并显示“已添加”。

- [x] **Step 4: 实现分组管理弹窗**

弹窗左侧显示分组列表和新建输入，右侧显示资料库房间复选框。选中成员调用 `addRoomToGroup` 或 `removeRoomFromGroup`；上移、下移调用 `moveGroupRoom`；重命名和删除均使用现有 icon button 样式与确认区域，不调用浏览器原生 `prompt`。

- [x] **Step 5: 改造 RoomSidebar**

本地视图状态：

```ts
type SidebarView = { type: 'current' } | { type: 'favorites' } | { type: 'history' };
```

前 3 个分组显示为标签，其余进入 `MoreHorizontal` 菜单。点击分组先调用 `switchGroup(group.id)`，再切回 current 视图。当前房间行使用 `RoomAvatar`，状态点由 `room.online`/`room.status` 决定；收藏按钮调用 `toggleFavorite`。删除摘要、观看人数和 footer。

- [x] **Step 6: 在 App 中管理分组弹窗**

`App` 增加 `groupManagerOpen`，将打开回调传给 `RoomSidebar`，并在根节点末尾条件渲染 `GroupManagerDialog`。

- [x] **Step 7: 运行侧栏与 store 测试**

Run: `npx vitest run tests/room-sidebar.test.tsx tests/app-smoke.test.tsx tests/workspace-store.test.ts`

Expected: 全部通过。

## Task 7：压缩顶部栏并删除画布说明

**Files:**
- Modify: `src/renderer/components/AppHeader.tsx`
- Modify: `src/renderer/components/WorkspaceGrid.tsx`
- Modify: `tests/app-smoke.test.tsx`

- [x] **Step 1: 写文案移除红灯测试**

```ts
expect(html).not.toContain('MULTI-VIEW DESK');
expect(html).not.toContain('公开数据模式');
expect(html).not.toContain('/ 9 路');
expect(html).not.toContain('LIVE CANVAS');
expect(html).not.toContain('多视角监看');
expect(html).not.toContain('实时同步');
expect(html).not.toContain('布局切换只移动');
```

- [x] **Step 2: 运行烟雾测试确认旧界面仍存在**

Run: `npx vitest run tests/app-smoke.test.tsx`

Expected: FAIL，命中至少一个旧文案。

- [x] **Step 3: 删除 Header 中央状态与 WorkspaceGrid 标题/脚注**

`AppHeader` 删除 `rooms`、`demoMode` 和 `.header-status`。品牌只保留名称。`WorkspaceGrid` 删除 `demoMode`、`layout`、`.workspace-heading` 和 `.workspace-footnote`，保留空状态与网格。

- [x] **Step 4: 运行烟雾测试**

Run: `npx vitest run tests/app-smoke.test.tsx`

Expected: 全部通过。

## Task 8：实现 3 秒播放器控件自动隐藏

**Files:**
- Create: `src/renderer/player-controls-visibility.ts`
- Create: `tests/player-controls-visibility.test.ts`
- Modify: `src/renderer/components/RoomTile.tsx`
- Modify: `tests/app-smoke.test.tsx`

- [x] **Step 1: 写计时器红灯测试**

```ts
import { afterEach, describe, expect, it, vi } from 'vitest';
import { PLAYER_CONTROLS_HIDE_DELAY_MS, scheduleControlsHide } from '../src/renderer/player-controls-visibility';

afterEach(() => vi.useRealTimers());

it('hides controls after three seconds', () => {
  vi.useFakeTimers();
  const onHide = vi.fn();
  scheduleControlsHide({ locked: false, onHide });
  vi.advanceTimersByTime(PLAYER_CONTROLS_HIDE_DELAY_MS - 1);
  expect(onHide).not.toHaveBeenCalled();
  vi.advanceTimersByTime(1);
  expect(onHide).toHaveBeenCalledOnce();
});

it('does not schedule hiding while interaction is locked', () => {
  vi.useFakeTimers();
  const onHide = vi.fn();
  scheduleControlsHide({ locked: true, onHide });
  vi.runAllTimers();
  expect(onHide).not.toHaveBeenCalled();
});
```

- [x] **Step 2: 运行测试确认模块不存在**

Run: `npx vitest run tests/player-controls-visibility.test.ts`

Expected: FAIL，无法导入模块。

- [x] **Step 3: 实现可取消计时器**

```ts
export const PLAYER_CONTROLS_HIDE_DELAY_MS = 3_000;

export function scheduleControlsHide({ locked, onHide }: { locked: boolean; onHide: () => void }): () => void {
  if (locked) return () => {};
  const timer = globalThis.setTimeout(onHide, PLAYER_CONTROLS_HIDE_DELAY_MS);
  return () => globalThis.clearTimeout(timer);
}
```

- [x] **Step 4: RoomTile 接入交互状态**

新增 `controlsVisible`、`focusWithin` 和 `menuOpen`。`showControls()` 先显示控件并重置计时器。`useEffect` 在 `focusWithin || menuOpen` 时保持可见，否则调用 `scheduleControlsHide`。article 增加：

```tsx
className={`room-tile ${isPrimary ? 'is-primary' : ''} ${controlsVisible ? 'controls-visible' : 'controls-hidden'}`}
onPointerMove={showControls}
onPointerDown={showControls}
onTouchStart={showControls}
onFocusCapture={() => { setFocusWithin(true); setControlsVisible(true); }}
onBlurCapture={(event) => {
  if (!event.currentTarget.contains(event.relatedTarget as Node | null)) setFocusWithin(false);
}}
```

- [x] **Step 5: 运行计时器和烟雾测试**

Run: `npx vitest run tests/player-controls-visibility.test.ts tests/app-smoke.test.tsx`

Expected: 全部通过。

## Task 9：完成样式、响应式布局与全量验证

**Files:**
- Modify: `src/renderer/styles.css`
- Modify: `docs/superpowers/specs/2026-08-10-room-library-and-compact-player-design.md` only if implementation exposes a confirmed design correction

- [x] **Step 1: 添加紧凑顶部和无标题画布样式**

将 `.app-header` 高度改为 44px，两列布局 `1fr auto`；`.app-body` 高度使用 `calc(100vh - 44px)`；`.workspace-main` 缩小到 8px 内边距；`.workspace-grid` 最小高度使用 `calc(100vh - 60px)`。同步调整 820px 和 480px 断点，避免保留 62px 的旧高度计算。

- [x] **Step 2: 添加头像、分组和资料库样式**

头像使用稳定的 32px 圆形尺寸和 `object-fit: cover`。分组标签使用横向滚动容器，文字不压缩；收藏/历史入口使用两段式按钮。弹窗最大宽度不超过 `760px`，桌面双栏、窄屏单栏，不嵌套卡片。

- [x] **Step 3: 添加控件淡出样式**

```css
.tile-topbar, .tile-bottom-bar { transition: opacity 160ms ease, transform 160ms ease; }
.room-tile.controls-hidden .tile-topbar { opacity: 0; transform: translateY(-6px); pointer-events: none; }
.room-tile.controls-hidden .tile-bottom-bar { opacity: 0; transform: translateY(6px); pointer-events: none; }
@media (prefers-reduced-motion: reduce) {
  .tile-topbar, .tile-bottom-bar { transition: none; }
}
```

- [x] **Step 4: 运行格式与死代码扫描**

Run: `rg -n "监看列表|MULTI-VIEW DESK|公开数据模式|LIVE CANVAS|多视角监看|workspace-footnote|sidebar-footer|viewerLabel" src/renderer`

Expected: `viewerLabel` 只允许保留在数据模型或搜索业务中，其他已删除文案和样式选择器无结果。

- [x] **Step 5: 运行完整自动化验证**

Run: `npm test -- --run`

Expected: 0 failed。

Run: `npm run typecheck`

Expected: exit code 0。

Run: `npm run build`

Expected: exit code 0。

- [x] **Step 6: 浏览器桌面验证**

启动 `npm run dev -- --port 60423`，在 1440×900 检查：页面非空、无框架错误覆盖、顶部约 44px、中央状态与画布标题不存在、头像为圆形、离线状态为红色、分组标签切换后画面被完整替换、收藏和历史可以快速添加、鼠标静止 3 秒后上下控件隐藏、再次移动后恢复、菜单打开时不隐藏、控制台无相关 warning/error。

- [x] **Step 7: 浏览器窄屏验证**

在 390×844 检查：侧栏可展开和收起，分组标签可访问，分组管理弹窗为单栏，按钮文字不溢出，直播画面和弹幕不被顶部栏遮挡，控制台无相关 warning/error。

- [x] **Step 8: 更新任务状态并记录剩余限制**

仅在全部命令与浏览器检查通过后标记实施完成。若斗鱼真实房间没有头像字段，界面必须显示首字回退；这不阻塞本地功能验收。

## 完成状态与保留限制

计划步骤已于 2026-08-10 对照源码和自动化验证更新为完成。当前列表只显示状态指示器，分区只在收藏和历史中显示；重复添加会更新历史时间；离线房间恢复在线后会重新检查播放源。

以下限制按产品决定保留：生产环境当前只有一个合规 `auto` 清晰度变体；未执行 9 路两小时压力测试；Vite Mock 数据继续使用主播首字回退，不补真实头像。
