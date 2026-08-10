# Main Room Resize Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a constrained, persistent resize divider for the primary live room while secondary rooms redistribute automatically and primary-room changes swap visual slots without remounting playback.

**Architecture:** Keep `primary-two` as the persisted layout ID and rename only its UI copy. Put size-dependent layout math in the domain layout engine, placement-order normalization in a small store helper, and pointer/keyboard behavior in a focused divider component. `WorkspaceGrid` will keep every keyed `RoomTile` as a direct grid child and assign positions through CSS Grid so moving rooms does not replace player or danmaku nodes.

**Tech Stack:** TypeScript 7, React 19, Zustand, CSS Grid, Vitest, Playwright, Vite, Electron

---

## File Map

- Create `src/renderer/store/room-placement.ts`: normalize, move, swap, and repair visual room placement order.
- Create `src/renderer/components/PrimaryRoomDivider.tsx`: pointer capture, animation-frame preview, snapping, keyboard input, cancel, and reset.
- Create `tests/room-placement.test.ts`: pure placement-order behavior.
- Create `tests/primary-room-divider.test.tsx`: ratio stepping, snapping, clamping, and separator markup.
- Create `scripts/primary-room-resize-e2e.mjs`: browser-level drag, swap, node-identity, overlap, and narrow-layout checks.
- Modify `src/domain/layout-engine.ts`: primary-focus plan, 1-9 room slot rules, ratio availability, and effective-ratio calculation.
- Modify `src/renderer/store/workspace-persistence.ts`: save and repair `roomPlacementOrder` and `primaryRoomRatio` without changing schema version 3.
- Modify `src/renderer/store/workspace-store.ts`: own placement and ratio state and update it during room lifecycle actions.
- Modify `src/renderer/components/WorkspaceGrid.tsx`: measure the grid, calculate the focus plan, render the divider, and apply preview CSS variables.
- Modify `src/renderer/components/RoomTile.tsx`: expose a stable room selector and keep primary controls visible during a resize.
- Modify `src/renderer/ui-model.ts`: rename `primary-two` to “主画面布局”.
- Modify `src/renderer/styles.css`: desktop left/right focus grid, narrow top/bottom focus grid, divider states, and reduced-motion behavior.
- Modify `tests/layout-engine.test.ts`, `tests/workspace-persistence.test.ts`, `tests/workspace-store.test.ts`, `tests/ui-model.test.ts`, and `tests/app-smoke.test.tsx`: regression coverage.
- Modify `package.json`: add the focused Playwright command.

### Task 1: Primary-focus layout engine

**Files:**
- Modify: `src/domain/layout-engine.ts:1-110`
- Test: `tests/layout-engine.test.ts:1-65`

- [ ] **Step 1: Write failing tests for the 1-9 room geometry and ratio constraints**

Add these imports and cases to `tests/layout-engine.test.ts`:

```ts
import {
  calculateLayout,
  calculatePrimaryFocusLayout,
  getRecommendedLayoutId,
  resolveLayoutId,
} from '../src/domain/layout-engine';

describe('calculatePrimaryFocusLayout', () => {
  it.each([
    [1, 1, 0],
    [2, 1, 1],
    [3, 1, 2],
    [4, 1, 3],
    [5, 2, 2],
    [6, 2, 3],
    [7, 2, 3],
    [8, 2, 4],
    [9, 2, 4],
  ])('uses the expected secondary grid for %i rooms', (count, columns, rows) => {
    const roomIds = Array.from({ length: count }, (_, index) => `room-${index + 1}`);
    const plan = calculatePrimaryFocusLayout(roomIds, roomIds[0], 0.6, {
      width: 1920,
      height: 1080,
    });

    expect(plan.secondaryColumns).toBe(columns);
    expect(plan.secondaryRows).toBe(rows);
    expect(plan.slots).toHaveLength(count);
    expect(new Set(plan.slots.map((slot) => slot.roomId)).size).toBe(count);
  });

  it('places the primary room across every secondary row without changing room order', () => {
    const plan = calculatePrimaryFocusLayout(['a', 'b', 'c', 'd'], 'c', 0.6, {
      width: 1440,
      height: 900,
    });

    expect(plan.orderedRoomIds).toEqual(['a', 'b', 'c', 'd']);
    expect(plan.slots).toEqual([
      { roomId: 'c', row: 1, column: 1, rowSpan: 3, columnSpan: 1 },
      { roomId: 'a', row: 1, column: 3, rowSpan: 1, columnSpan: 1 },
      { roomId: 'b', row: 2, column: 3, rowSpan: 1, columnSpan: 1 },
      { roomId: 'd', row: 3, column: 3, rowSpan: 1, columnSpan: 1 },
    ]);
  });

  it('filters snap ratios that would shrink secondary tiles below 240 by 135', () => {
    const plan = calculatePrimaryFocusLayout(
      ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i'],
      'a',
      0.67,
      { width: 1180, height: 780 },
    );

    expect(plan.availableRatios).toEqual([0.5]);
    expect(plan.effectiveRatio).toBe(0.5);
  });

  it('uses a top-bottom focus grid below the 820px breakpoint', () => {
    const plan = calculatePrimaryFocusLayout(['a', 'b', 'c'], 'a', 0.6, {
      width: 390,
      height: 844,
    });

    expect(plan.orientation).toBe('vertical');
    expect(plan.slots).toEqual([
      { roomId: 'a', row: 1, column: 1, rowSpan: 1, columnSpan: 1 },
      { roomId: 'b', row: 3, column: 1, rowSpan: 1, columnSpan: 1 },
      { roomId: 'c', row: 4, column: 1, rowSpan: 1, columnSpan: 1 },
    ]);
  });

  it('fills the workspace and omits resize choices for one room', () => {
    const plan = calculatePrimaryFocusLayout(['a'], 'a', 0.6, {
      width: 1440,
      height: 900,
    });

    expect(plan.effectiveRatio).toBe(1);
    expect(plan.availableRatios).toEqual([]);
    expect(plan.slots).toEqual([
      { roomId: 'a', row: 1, column: 1, rowSpan: 1, columnSpan: 1 },
    ]);
  });
});
```

Replace the existing three-room `primary-two` expectation with the new single-track primary slot:

```ts
it('uses a primary slot when the primary-two layout has three rooms', () => {
  expect(calculateLayout(['a', 'b', 'c'], 'primary-two', 'b')).toEqual([
    { roomId: 'b', row: 1, column: 1, rowSpan: 2, columnSpan: 1 },
    { roomId: 'a', row: 1, column: 3, rowSpan: 1, columnSpan: 1 },
    { roomId: 'c', row: 2, column: 3, rowSpan: 1, columnSpan: 1 },
  ]);
});
```

- [ ] **Step 2: Run the layout tests and confirm the missing API failure**

Run: `npm test -- tests/layout-engine.test.ts`

Expected: FAIL because `calculatePrimaryFocusLayout` is not exported.

- [ ] **Step 3: Add the focus-plan types and pure calculation**

Add after `LayoutSlot` in `src/domain/layout-engine.ts`:

```ts
export type PrimaryRoomRatio = 0.5 | 0.6 | 0.67;
export type PrimaryLayoutOrientation = 'horizontal' | 'vertical';

export interface WorkspaceSize {
  width: number;
  height: number;
}

export interface PrimaryFocusLayoutPlan {
  kind: 'primary-focus';
  primaryRoomId: string;
  orderedRoomIds: string[];
  secondaryColumns: 1 | 2;
  secondaryRows: number;
  preferredRatio: PrimaryRoomRatio;
  effectiveRatio: number;
  availableRatios: PrimaryRoomRatio[];
  orientation: PrimaryLayoutOrientation;
  slots: LayoutSlot[];
}

export const PRIMARY_ROOM_RATIOS: readonly PrimaryRoomRatio[] = [0.5, 0.6, 0.67];
export const DEFAULT_PRIMARY_ROOM_RATIO: PrimaryRoomRatio = 0.6;
export const PRIMARY_ROOM_RATIO_MIN = 0.42;
export const PRIMARY_ROOM_RATIO_MAX = 0.7;

const PRIMARY_DIVIDER_SIZE = 8;
const PRIMARY_GRID_GAP = 8;
const SECONDARY_TARGET_WIDTH = 240;
const SECONDARY_TARGET_HEIGHT = 135;
const PRIMARY_VERTICAL_BREAKPOINT = 820;

function clampRatio(value: number): number {
  return Math.min(PRIMARY_ROOM_RATIO_MAX, Math.max(PRIMARY_ROOM_RATIO_MIN, value));
}

function secondaryGrid(secondaryCount: number): { columns: 1 | 2; rows: number } {
  if (secondaryCount <= 0) return { columns: 1, rows: 0 };
  const columns: 1 | 2 = secondaryCount <= 3 ? 1 : 2;
  return { columns, rows: Math.ceil(secondaryCount / columns) };
}

function ratioFits(
  ratio: PrimaryRoomRatio,
  size: WorkspaceSize,
  orientation: PrimaryLayoutOrientation,
  columns: 1 | 2,
  rows: number,
): boolean {
  if (size.width <= 0 || size.height <= 0 || rows === 0) return true;
  if (orientation === 'horizontal') {
    const secondaryWidth = (
      size.width * (1 - ratio) - PRIMARY_DIVIDER_SIZE - PRIMARY_GRID_GAP * (columns + 1)
    ) / columns;
    const secondaryHeight = (
      size.height - PRIMARY_GRID_GAP * (rows - 1)
    ) / rows;
    return secondaryWidth >= SECONDARY_TARGET_WIDTH && secondaryHeight >= SECONDARY_TARGET_HEIGHT;
  }
  const secondaryWidth = (size.width - PRIMARY_GRID_GAP * (columns - 1)) / columns;
  const secondaryHeight = (
    size.height * (1 - ratio) - PRIMARY_DIVIDER_SIZE - PRIMARY_GRID_GAP * (rows + 1)
  ) / rows;
  return secondaryWidth >= SECONDARY_TARGET_WIDTH && secondaryHeight >= SECONDARY_TARGET_HEIGHT;
}

function closestRatio(value: number, ratios: readonly PrimaryRoomRatio[]): PrimaryRoomRatio {
  return ratios.reduce((closest, ratio) => (
    Math.abs(ratio - value) < Math.abs(closest - value) ? ratio : closest
  ));
}

function fallbackEffectiveRatio(
  preferredRatio: PrimaryRoomRatio,
  size: WorkspaceSize,
  orientation: PrimaryLayoutOrientation,
  columns: 1 | 2,
  rows: number,
): number {
  if (size.width <= 0 || size.height <= 0 || rows === 0) return preferredRatio;
  const targetMaximum = orientation === 'horizontal'
    ? 1 - (
        PRIMARY_DIVIDER_SIZE
        + columns * SECONDARY_TARGET_WIDTH
        + PRIMARY_GRID_GAP * (columns + 1)
      ) / size.width
    : 1 - (
        PRIMARY_DIVIDER_SIZE
        + rows * SECONDARY_TARGET_HEIGHT
        + PRIMARY_GRID_GAP * (rows + 1)
      ) / size.height;
  return clampRatio(Math.min(preferredRatio, targetMaximum));
}

export function calculatePrimaryFocusLayout(
  roomIds: string[],
  primaryRoomId: string | undefined,
  preferredRatio: PrimaryRoomRatio,
  size: WorkspaceSize,
): PrimaryFocusLayoutPlan {
  const orderedRoomIds = [...roomIds];
  const primary = primaryRoomId && orderedRoomIds.includes(primaryRoomId)
    ? primaryRoomId
    : orderedRoomIds[0] ?? '';
  const secondaryRoomIds = orderedRoomIds.filter((roomId) => roomId !== primary);
  const { columns, rows } = secondaryGrid(secondaryRoomIds.length);
  const orientation: PrimaryLayoutOrientation = size.width > 0 && size.width <= PRIMARY_VERTICAL_BREAKPOINT
    ? 'vertical'
    : 'horizontal';
  const availableRatios = rows === 0
    ? []
    : PRIMARY_ROOM_RATIOS.filter((ratio) => ratioFits(
        ratio,
        size,
        orientation,
        columns,
        rows,
      ));
  const effectiveRatio = rows === 0
    ? 1
    : availableRatios.length > 0
      ? closestRatio(preferredRatio, availableRatios)
      : fallbackEffectiveRatio(preferredRatio, size, orientation, columns, rows);
  const primarySlot: LayoutSlot = orientation === 'horizontal'
    ? { roomId: primary, row: 1, column: 1, rowSpan: Math.max(1, rows), columnSpan: 1 }
    : { roomId: primary, row: 1, column: 1, rowSpan: 1, columnSpan: columns };
  const secondarySlots = secondaryRoomIds.map((roomId, index): LayoutSlot => (
    orientation === 'horizontal'
      ? {
          roomId,
          row: Math.floor(index / columns) + 1,
          column: (index % columns) + 3,
          rowSpan: 1,
          columnSpan: 1,
        }
      : {
          roomId,
          row: Math.floor(index / columns) + 3,
          column: (index % columns) + 1,
          rowSpan: 1,
          columnSpan: 1,
        }
  ));

  return {
    kind: 'primary-focus',
    primaryRoomId: primary,
    orderedRoomIds,
    secondaryColumns: columns,
    secondaryRows: rows,
    preferredRatio,
    effectiveRatio,
    availableRatios,
    orientation,
    slots: primary ? [primarySlot, ...secondarySlots] : [],
  };
}
```

Replace `primaryTwoSlots` with:

```ts
function primaryTwoSlots(roomIds: string[], primaryRoomId?: string): LayoutSlot[] {
  return calculatePrimaryFocusLayout(
    roomIds,
    primaryRoomId,
    DEFAULT_PRIMARY_ROOM_RATIO,
    { width: 0, height: 0 },
  ).slots;
}
```

- [ ] **Step 4: Run the focused tests and confirm they pass**

Run: `npm test -- tests/layout-engine.test.ts`

Expected: PASS, including the existing layout recommendation cases.

- [ ] **Step 5: Commit the layout engine**

```powershell
git add src/domain/layout-engine.ts tests/layout-engine.test.ts
git commit -m "实现主画面布局计算"
```

### Task 2: Visual placement order and persistence

**Files:**
- Create: `src/renderer/store/room-placement.ts`
- Create: `tests/room-placement.test.ts`
- Modify: `src/renderer/store/workspace-persistence.ts:1-55,190-249`
- Test: `tests/workspace-persistence.test.ts:1-210`

- [ ] **Step 1: Write failing placement-order tests**

Create `tests/room-placement.test.ts`:

```ts
import { describe, expect, it } from 'vitest';
import {
  moveRoomPlacement,
  nextPrimaryAfterRemoval,
  normalizeRoomPlacementOrder,
  swapPrimaryRoomPlacement,
} from '../src/renderer/store/room-placement';

describe('room placement order', () => {
  it('removes duplicates and dangling ids before appending missing active rooms', () => {
    expect(normalizeRoomPlacementOrder(['a', 'b', 'c'], ['c', 'missing', 'c']))
      .toEqual(['c', 'a', 'b']);
  });

  it('moves a room relative to its target without changing unrelated ids', () => {
    expect(moveRoomPlacement(['c', 'a', 'b', 'd'], 'b', 'a'))
      .toEqual(['c', 'b', 'a', 'd']);
  });

  it('swaps only the current and requested primary slots', () => {
    expect(swapPrimaryRoomPlacement(['a', 'b', 'c', 'd'], 'a', 'c'))
      .toEqual(['c', 'b', 'a', 'd']);
    expect(swapPrimaryRoomPlacement(['c', 'b', 'a', 'd'], 'c', 'b'))
      .toEqual(['b', 'c', 'a', 'd']);
  });

  it('selects the following visual slot when the primary room is removed', () => {
    expect(nextPrimaryAfterRemoval(['a', 'b', 'c'], 'b')).toBe('c');
    expect(nextPrimaryAfterRemoval(['a', 'b', 'c'], 'c')).toBe('b');
    expect(nextPrimaryAfterRemoval(['a'], 'a')).toBeUndefined();
  });
});
```

- [ ] **Step 2: Run the placement tests and confirm the missing-module failure**

Run: `npm test -- tests/room-placement.test.ts`

Expected: FAIL because `src/renderer/store/room-placement.ts` does not exist.

- [ ] **Step 3: Implement the pure placement helpers**

Create `src/renderer/store/room-placement.ts`:

```ts
export function normalizeRoomPlacementOrder(
  activeRoomIds: readonly string[],
  requestedOrder: readonly string[] | undefined,
): string[] {
  const activeIds = new Set(activeRoomIds);
  const normalized: string[] = [];
  for (const roomId of requestedOrder ?? []) {
    if (!activeIds.has(roomId) || normalized.includes(roomId)) continue;
    normalized.push(roomId);
  }
  for (const roomId of activeRoomIds) {
    if (!normalized.includes(roomId)) normalized.push(roomId);
  }
  return normalized;
}

export function moveRoomPlacement(
  roomIds: readonly string[],
  sourceRoomId: string,
  targetRoomId: string,
): string[] {
  const sourceIndex = roomIds.indexOf(sourceRoomId);
  const targetIndex = roomIds.indexOf(targetRoomId);
  if (sourceIndex < 0 || targetIndex < 0 || sourceIndex === targetIndex) return [...roomIds];
  const next = [...roomIds];
  const [source] = next.splice(sourceIndex, 1);
  next.splice(targetIndex, 0, source);
  return next;
}

export function swapPrimaryRoomPlacement(
  roomIds: readonly string[],
  currentPrimaryRoomId: string | undefined,
  targetPrimaryRoomId: string,
): string[] {
  const currentIndex = currentPrimaryRoomId === undefined ? -1 : roomIds.indexOf(currentPrimaryRoomId);
  const targetIndex = roomIds.indexOf(targetPrimaryRoomId);
  if (targetIndex < 0 || currentIndex < 0 || currentIndex === targetIndex) return [...roomIds];
  const next = [...roomIds];
  [next[currentIndex], next[targetIndex]] = [next[targetIndex], next[currentIndex]];
  return next;
}

export function nextPrimaryAfterRemoval(
  roomIds: readonly string[],
  removedRoomId: string,
): string | undefined {
  const removedIndex = roomIds.indexOf(removedRoomId);
  const remaining = roomIds.filter((roomId) => roomId !== removedRoomId);
  if (removedIndex < 0) return remaining[0];
  return remaining[removedIndex] ?? remaining[removedIndex - 1];
}
```

- [ ] **Step 4: Add failing persistence tests for defaults and invalid values**

Add `roomPlacementOrder: ['63136']` and `primaryRoomRatio: 0.6` to the `snapshot` fixture and legacy migration expectations in `tests/workspace-persistence.test.ts`. Add these cases:

```ts
it('repairs placement order and invalid primary ratio from a version 3 snapshot', () => {
  const storage = createMemoryStorage();
  const secondRoom = { ...persistedRoom, roomId: '270888', anchorName: '林深' };
  storage.setItem(WORKSPACE_STORAGE_KEY, JSON.stringify({
    ...snapshot,
    roomLibrary: { '63136': persistedRoom, '270888': secondRoom },
    activeRoomIds: ['63136', '270888'],
    roomPlacementOrder: ['270888', 'missing', '270888'],
    primaryRoomRatio: 0.63,
  }));

  expect(loadWorkspaceSnapshot(storage)).toEqual(expect.objectContaining({
    roomPlacementOrder: ['270888', '63136'],
    primaryRoomRatio: 0.6,
  }));
});

it('accepts each supported primary-room ratio', () => {
  const storage = createMemoryStorage();
  for (const primaryRoomRatio of [0.5, 0.6, 0.67] as const) {
    saveWorkspaceSnapshot(storage, { ...snapshot, primaryRoomRatio });
    expect(loadWorkspaceSnapshot(storage)?.primaryRoomRatio).toBe(primaryRoomRatio);
  }
});
```

- [ ] **Step 5: Run persistence tests and confirm the new fields fail**

Run: `npm test -- tests/room-placement.test.ts tests/workspace-persistence.test.ts`

Expected: placement tests PASS; persistence tests FAIL because `WorkspaceSnapshot` and the loader do not expose the new fields.

- [ ] **Step 6: Persist and repair the two fields without a schema bump**

In `src/renderer/store/workspace-persistence.ts`, import the ratio type/default and normalization helper:

```ts
import {
  DEFAULT_PRIMARY_ROOM_RATIO,
  PRIMARY_ROOM_RATIOS,
  type LayoutId,
  type PrimaryRoomRatio,
} from '../../domain/layout-engine';
import { normalizeRoomPlacementOrder } from './room-placement';
```

Add these required snapshot fields after `primaryRoomId`:

```ts
roomPlacementOrder: string[];
primaryRoomRatio: PrimaryRoomRatio;
```

Add this parser after `parseLayoutId`:

```ts
function parsePrimaryRoomRatio(value: unknown): PrimaryRoomRatio {
  return PRIMARY_ROOM_RATIOS.includes(value as PrimaryRoomRatio)
    ? value as PrimaryRoomRatio
    : DEFAULT_PRIMARY_ROOM_RATIO;
}
```

After `migrated` has been validated, calculate and return the repaired fields:

```ts
const requestedPlacementOrder = parsed.schemaVersion === WORKSPACE_SCHEMA_VERSION
  && Array.isArray(parsed.roomPlacementOrder)
  ? parsed.roomPlacementOrder.filter((value): value is string => typeof value === 'string')
  : undefined;
const roomPlacementOrder = normalizeRoomPlacementOrder(
  migrated.activeRoomIds,
  requestedPlacementOrder,
);
```

Add these properties to the returned snapshot:

```ts
roomPlacementOrder,
primaryRoomRatio: parsed.schemaVersion === WORKSPACE_SCHEMA_VERSION
  ? parsePrimaryRoomRatio(parsed.primaryRoomRatio)
  : DEFAULT_PRIMARY_ROOM_RATIO,
```

Keep `WORKSPACE_SCHEMA_VERSION` at `3`. The two fields are repaired on read, so existing version 3 installations remain compatible.

- [ ] **Step 7: Run the placement and persistence tests**

Run: `npm test -- tests/room-placement.test.ts tests/workspace-persistence.test.ts`

Expected: PASS.

- [ ] **Step 8: Commit placement and persistence**

```powershell
git add src/renderer/store/room-placement.ts src/renderer/store/workspace-persistence.ts tests/room-placement.test.ts tests/workspace-persistence.test.ts
git commit -m "持久化主画面槽位与比例"
```

### Task 3: Workspace state and primary-slot swapping

**Files:**
- Modify: `src/renderer/store/workspace-store.ts:1-92,120-269,424-462`
- Test: `tests/workspace-store.test.ts:57-359`

- [ ] **Step 1: Write failing store tests for lifecycle synchronization**

Add these cases to `tests/workspace-store.test.ts`:

```ts
it('swaps only primary and target visual slots across consecutive changes', () => {
  const store = createWorkspaceStore(createMockDouyuAdapter(), {
    initialRooms: [candidate('a'), candidate('b'), candidate('c'), candidate('d')],
  });
  store.getState().setAudioRoom('b');
  store.getState().setQuality('c', 'high');
  store.getState().toggleDanmaku('c');
  const primarySession = store.getState().rooms.find((room) => room.roomId === 'c');

  store.getState().setPrimaryRoom('c');
  expect(store.getState().roomPlacementOrder).toEqual(['c', 'b', 'a', 'd']);
  expect(store.getState().primaryRoomId).toBe('c');
  expect(store.getState().audioRoomId).toBe('b');
  expect(store.getState().rooms.find((room) => room.roomId === 'c')).toBe(primarySession);
  expect(primarySession).toEqual(expect.objectContaining({ quality: 'high', danmakuEnabled: false }));

  store.getState().setPrimaryRoom('b');
  expect(store.getState().roomPlacementOrder).toEqual(['b', 'c', 'a', 'd']);
  expect(store.getState().primaryRoomId).toBe('b');
});

it('synchronizes placement order during add, sidebar reorder, and removal', () => {
  const store = createWorkspaceStore(createMockDouyuAdapter(), {
    initialRooms: [candidate('a'), candidate('b'), candidate('c')],
  });
  store.getState().setPrimaryRoom('c');
  store.getState().addRoom(candidate('d'));
  expect(store.getState().roomPlacementOrder).toEqual(['c', 'b', 'a', 'd']);

  store.getState().reorderRooms('d', 'b');
  expect(store.getState().roomPlacementOrder).toEqual(['c', 'd', 'b', 'a']);

  store.getState().removeRoom('c');
  expect(store.getState().roomPlacementOrder).toEqual(['d', 'b', 'a']);
  expect(store.getState().primaryRoomId).toBe('d');
});

it('keeps surviving visual slots while switching groups and appends new rooms', () => {
  const store = createWorkspaceStore(createMockDouyuAdapter(), {
    ...deterministicOptions,
    initialRooms: [candidate('a'), candidate('b'), candidate('c')],
  });
  store.getState().setPrimaryRoom('c');
  const groupId = store.getState().createGroup('赛事')!;
  store.getState().addRoomToGroup(groupId, 'b');
  store.getState().addRoomToGroup(groupId, 'c');
  store.getState().switchGroup(groupId);

  expect(store.getState().roomPlacementOrder).toEqual(['c', 'b']);
  expect(store.getState().primaryRoomId).toBe('c');
});

it('persists the selected primary ratio and restores it', () => {
  const storage = createMemoryStorage();
  const first = createWorkspaceStore(createMockDouyuAdapter(), {
    storage,
    initialRooms: [candidate('a'), candidate('b')],
  });
  first.getState().setPrimaryRoomRatio(0.67);

  const restored = createWorkspaceStore(createMockDouyuAdapter(), { storage });
  expect(restored.getState().primaryRoomRatio).toBe(0.67);
});
```

- [ ] **Step 2: Run store tests and confirm the missing state/action failure**

Run: `npm test -- tests/workspace-store.test.ts`

Expected: FAIL because `roomPlacementOrder`, `primaryRoomRatio`, and `setPrimaryRoomRatio` are absent.

- [ ] **Step 3: Add the state contract, initialization, and persistence fields**

Import the ratio type/default and placement helpers in `workspace-store.ts`:

```ts
import {
  DEFAULT_PRIMARY_ROOM_RATIO,
  type LayoutId,
  type PrimaryRoomRatio,
} from '../../domain/layout-engine';
import {
  moveRoomPlacement,
  nextPrimaryAfterRemoval,
  normalizeRoomPlacementOrder,
  swapPrimaryRoomPlacement,
} from './room-placement';
```

Add to `WorkspaceState`:

```ts
roomPlacementOrder: string[];
primaryRoomRatio: PrimaryRoomRatio;
setPrimaryRoomRatio: (ratio: PrimaryRoomRatio) => void;
```

Calculate the initial order after `initialSessions`:

```ts
const initialPlacementOrder = normalizeRoomPlacementOrder(
  initialSessions.map((room) => room.roomId),
  persisted?.roomPlacementOrder,
);
```

Add to the initial state:

```ts
roomPlacementOrder: initialPlacementOrder,
primaryRoomRatio: persisted?.primaryRoomRatio ?? DEFAULT_PRIMARY_ROOM_RATIO,
```

Add to the persisted snapshot:

```ts
roomPlacementOrder: state.roomPlacementOrder,
primaryRoomRatio: state.primaryRoomRatio,
```

- [ ] **Step 4: Synchronize each room mutation and implement primary swapping**

Add room append behavior to the `addRoom` state update:

```ts
roomPlacementOrder: [...state.roomPlacementOrder, session.roomId],
```

Replace the body of the `removeRoom` state updater with:

```ts
const rooms = state.rooms.filter((room) => room.roomId !== roomId);
const nextPrimaryRoomId = nextPrimaryAfterRemoval(state.roomPlacementOrder, roomId);
const roomPlacementOrder = state.roomPlacementOrder.filter((id) => id !== roomId);
return {
  rooms,
  roomPlacementOrder,
  activeGroupId: undefined,
  primaryRoomId: state.primaryRoomId === roomId ? nextPrimaryRoomId : state.primaryRoomId,
  audioRoomId: state.audioRoomId === roomId ? rooms[0]?.roomId : state.audioRoomId,
};
```

In `moveRoom`, capture the target room ID before moving and return:

```ts
const targetRoomId = state.rooms[to].roomId;
const rooms = [...state.rooms];
const [moved] = rooms.splice(from, 1);
rooms.splice(to, 0, moved);
return {
  rooms,
  roomPlacementOrder: moveRoomPlacement(state.roomPlacementOrder, roomId, targetRoomId),
  activeGroupId: undefined,
};
```

In `reorderRooms`, return:

```ts
return {
  rooms,
  roomPlacementOrder: moveRoomPlacement(
    state.roomPlacementOrder,
    sourceRoomId,
    targetRoomId,
  ),
  activeGroupId: undefined,
};
```

Replace `setPrimaryRoom` and add the ratio action:

```ts
setPrimaryRoom(roomId) {
  if (!get().rooms.some((room) => room.roomId === roomId)) return;
  set((state) => ({
    primaryRoomId: roomId,
    roomPlacementOrder: swapPrimaryRoomPlacement(
      state.roomPlacementOrder,
      state.primaryRoomId,
      roomId,
    ),
  }));
  persist();
},

setPrimaryRoomRatio(primaryRoomRatio) {
  set({ primaryRoomRatio });
  persist();
},
```

Replace the `switchGroup` state update with:

```ts
const roomPlacementOrder = normalizeRoomPlacementOrder(
  rooms.map((room) => room.roomId),
  state.roomPlacementOrder,
);
const primaryRoomId = state.primaryRoomId && roomPlacementOrder.includes(state.primaryRoomId)
  ? state.primaryRoomId
  : roomPlacementOrder[0];
set({
  rooms,
  roomPlacementOrder,
  activeGroupId: groupId,
  primaryRoomId,
  audioRoomId: rooms[0]?.roomId,
});
```

- [ ] **Step 5: Run store and persistence tests**

Run: `npm test -- tests/workspace-store.test.ts tests/workspace-persistence.test.ts tests/room-placement.test.ts`

Expected: PASS.

- [ ] **Step 6: Commit workspace state behavior**

```powershell
git add src/renderer/store/workspace-store.ts tests/workspace-store.test.ts
git commit -m "实现主直播间槽位对调"
```

### Task 4: Accessible resize divider

**Files:**
- Create: `src/renderer/components/PrimaryRoomDivider.tsx`
- Create: `tests/primary-room-divider.test.tsx`

- [ ] **Step 1: Write failing divider utility and markup tests**

Create `tests/primary-room-divider.test.tsx`:

```tsx
import { renderToStaticMarkup } from 'react-dom/server';
import { describe, expect, it, vi } from 'vitest';
import {
  PrimaryRoomDivider,
  clampPrimaryRoomPreview,
  snapPrimaryRoomRatio,
  stepPrimaryRoomRatio,
} from '../src/renderer/components/PrimaryRoomDivider';

describe('PrimaryRoomDivider', () => {
  it('clamps free drag preview to the hard range', () => {
    expect(clampPrimaryRoomPreview(0.2)).toBe(0.42);
    expect(clampPrimaryRoomPreview(0.64)).toBe(0.64);
    expect(clampPrimaryRoomPreview(0.9)).toBe(0.7);
  });

  it('snaps to the nearest available recommendation', () => {
    expect(snapPrimaryRoomRatio(0.64, [0.5, 0.6, 0.67])).toBe(0.67);
    expect(snapPrimaryRoomRatio(0.64, [0.5, 0.6])).toBe(0.6);
    expect(snapPrimaryRoomRatio(0.64, [])).toBe(0.6);
  });

  it('steps through available ratios for keyboard input', () => {
    expect(stepPrimaryRoomRatio(0.6, [0.5, 0.6, 0.67], -1)).toBe(0.5);
    expect(stepPrimaryRoomRatio(0.6, [0.5, 0.6, 0.67], 1)).toBe(0.67);
    expect(stepPrimaryRoomRatio(0.67, [0.5, 0.6, 0.67], 1)).toBe(0.67);
  });

  it('renders an accessible horizontal separator', () => {
    const html = renderToStaticMarkup(
      <PrimaryRoomDivider
        orientation="horizontal"
        value={0.6}
        availableRatios={[0.5, 0.6, 0.67]}
        onPreviewChange={vi.fn()}
        onCommit={vi.fn()}
        onDragStateChange={vi.fn()}
      />,
    );

    expect(html).toContain('role="separator"');
    expect(html).toContain('aria-orientation="vertical"');
    expect(html).toContain('aria-valuemin="42"');
    expect(html).toContain('aria-valuemax="70"');
    expect(html).toContain('aria-valuenow="60"');
  });
});
```

- [ ] **Step 2: Run the divider test and confirm the missing-module failure**

Run: `npm test -- tests/primary-room-divider.test.tsx`

Expected: FAIL because `PrimaryRoomDivider.tsx` does not exist.

- [ ] **Step 3: Implement pointer, keyboard, cancel, reset, and ARIA behavior**

Create `src/renderer/components/PrimaryRoomDivider.tsx`:

```tsx
import { useEffect, useRef, useState, type KeyboardEvent, type PointerEvent } from 'react';
import {
  DEFAULT_PRIMARY_ROOM_RATIO,
  PRIMARY_ROOM_RATIO_MAX,
  PRIMARY_ROOM_RATIO_MIN,
  PRIMARY_ROOM_RATIOS,
  type PrimaryLayoutOrientation,
  type PrimaryRoomRatio,
} from '../../domain/layout-engine';

interface PrimaryRoomDividerProps {
  orientation: PrimaryLayoutOrientation;
  value: number;
  availableRatios: PrimaryRoomRatio[];
  onPreviewChange: (ratio: number | undefined) => void;
  onCommit: (ratio: PrimaryRoomRatio) => void;
  onDragStateChange: (dragging: boolean) => void;
}

export function clampPrimaryRoomPreview(value: number): number {
  return Math.min(PRIMARY_ROOM_RATIO_MAX, Math.max(PRIMARY_ROOM_RATIO_MIN, value));
}

export function snapPrimaryRoomRatio(
  value: number,
  availableRatios: readonly PrimaryRoomRatio[],
): PrimaryRoomRatio {
  const ratios = availableRatios.length > 0 ? availableRatios : PRIMARY_ROOM_RATIOS;
  return ratios.reduce((closest, ratio) => (
    Math.abs(ratio - value) < Math.abs(closest - value) ? ratio : closest
  ));
}

export function stepPrimaryRoomRatio(
  value: number,
  availableRatios: readonly PrimaryRoomRatio[],
  direction: -1 | 1,
): PrimaryRoomRatio {
  const ratios = availableRatios.length > 0 ? [...availableRatios] : [...PRIMARY_ROOM_RATIOS];
  const current = snapPrimaryRoomRatio(value, ratios);
  const currentIndex = ratios.indexOf(current);
  const nextIndex = Math.min(ratios.length - 1, Math.max(0, currentIndex + direction));
  return ratios[nextIndex];
}

export function PrimaryRoomDivider({
  orientation,
  value,
  availableRatios,
  onPreviewChange,
  onCommit,
  onDragStateChange,
}: PrimaryRoomDividerProps) {
  const [previewRatio, setPreviewRatio] = useState<number>();
  const previewRef = useRef<number | undefined>(undefined);
  const frameRef = useRef<number | undefined>(undefined);
  const pointerIdRef = useRef<number | undefined>(undefined);

  const cancelFrame = () => {
    if (frameRef.current !== undefined) cancelAnimationFrame(frameRef.current);
    frameRef.current = undefined;
  };

  const finishPreview = () => {
    cancelFrame();
    pointerIdRef.current = undefined;
    previewRef.current = undefined;
    setPreviewRatio(undefined);
    onPreviewChange(undefined);
    onDragStateChange(false);
  };

  const ratioFromPointer = (event: PointerEvent<HTMLDivElement>): number => {
    const bounds = event.currentTarget.parentElement?.getBoundingClientRect();
    if (!bounds || bounds.width <= 0 || bounds.height <= 0) return value;
    const ratio = orientation === 'horizontal'
      ? (event.clientX - bounds.left) / bounds.width
      : (event.clientY - bounds.top) / bounds.height;
    return clampPrimaryRoomPreview(ratio);
  };

  const publishPreview = (ratio: number) => {
    previewRef.current = ratio;
    cancelFrame();
    frameRef.current = requestAnimationFrame(() => {
      frameRef.current = undefined;
      setPreviewRatio(ratio);
      onPreviewChange(ratio);
    });
  };

  const handlePointerDown = (event: PointerEvent<HTMLDivElement>) => {
    if (event.button !== 0) return;
    event.preventDefault();
    pointerIdRef.current = event.pointerId;
    event.currentTarget.setPointerCapture(event.pointerId);
    onDragStateChange(true);
    publishPreview(ratioFromPointer(event));
  };

  const handlePointerMove = (event: PointerEvent<HTMLDivElement>) => {
    if (pointerIdRef.current !== event.pointerId) return;
    publishPreview(ratioFromPointer(event));
  };

  const handlePointerUp = (event: PointerEvent<HTMLDivElement>) => {
    if (pointerIdRef.current !== event.pointerId) return;
    const nextRatio = snapPrimaryRoomRatio(ratioFromPointer(event), availableRatios);
    if (event.currentTarget.hasPointerCapture(event.pointerId)) {
      event.currentTarget.releasePointerCapture(event.pointerId);
    }
    finishPreview();
    onCommit(nextRatio);
  };

  const handleKeyDown = (event: KeyboardEvent<HTMLDivElement>) => {
    if (event.key === 'Escape' && pointerIdRef.current !== undefined) {
      event.preventDefault();
      if (event.currentTarget.hasPointerCapture(pointerIdRef.current)) {
        event.currentTarget.releasePointerCapture(pointerIdRef.current);
      }
      finishPreview();
      return;
    }
    const smaller = event.key === 'ArrowLeft' || event.key === 'ArrowUp';
    const larger = event.key === 'ArrowRight' || event.key === 'ArrowDown';
    if (smaller || larger) {
      event.preventDefault();
      onCommit(stepPrimaryRoomRatio(value, availableRatios, smaller ? -1 : 1));
      return;
    }
    const ratios = availableRatios.length > 0 ? availableRatios : [...PRIMARY_ROOM_RATIOS];
    if (event.key === 'Home') {
      event.preventDefault();
      onCommit(ratios[0]);
    }
    if (event.key === 'End') {
      event.preventDefault();
      onCommit(ratios[ratios.length - 1]);
    }
  };

  useEffect(() => () => {
    cancelFrame();
    onPreviewChange(undefined);
    onDragStateChange(false);
  }, [orientation]);

  const renderedValue = previewRatio ?? value;
  return (
    <div
      className={`primary-room-divider is-${orientation}`}
      role="separator"
      tabIndex={0}
      aria-label="调整主画面大小"
      aria-orientation={orientation === 'horizontal' ? 'vertical' : 'horizontal'}
      aria-valuemin={Math.round(PRIMARY_ROOM_RATIO_MIN * 100)}
      aria-valuemax={Math.round(PRIMARY_ROOM_RATIO_MAX * 100)}
      aria-valuenow={Math.round(renderedValue * 100)}
      title="调整主画面大小"
      onPointerDown={handlePointerDown}
      onPointerMove={handlePointerMove}
      onPointerUp={handlePointerUp}
      onPointerCancel={finishPreview}
      onKeyDown={handleKeyDown}
      onDoubleClick={() => onCommit(snapPrimaryRoomRatio(
        DEFAULT_PRIMARY_ROOM_RATIO,
        availableRatios,
      ))}
    >
      <span aria-hidden="true" />
    </div>
  );
}
```

- [ ] **Step 4: Run divider tests and type checking**

Run: `npm test -- tests/primary-room-divider.test.tsx; npm run typecheck`

Expected: both commands PASS.

- [ ] **Step 5: Commit the divider component**

```powershell
git add src/renderer/components/PrimaryRoomDivider.tsx tests/primary-room-divider.test.tsx
git commit -m "实现主画面尺寸分隔线"
```

### Task 5: Grid integration, stable tiles, and responsive styling

**Files:**
- Modify: `src/renderer/components/WorkspaceGrid.tsx:1-37`
- Modify: `src/renderer/components/RoomTile.tsx:11-15,24-100`
- Modify: `src/renderer/ui-model.ts:13-22`
- Modify: `src/renderer/styles.css:131-138,308-313,339-398`
- Test: `tests/ui-model.test.ts`
- Test: `tests/app-smoke.test.tsx:48-78`
- Create: `scripts/primary-room-resize-e2e.mjs`
- Modify: `package.json:scripts`

- [ ] **Step 1: Add failing UI-copy and stable-selector smoke expectations**

In `tests/ui-model.test.ts`, add this expectation to `exposes all approved layout choices with stable ids and labels`:

```ts
expect(getLayoutOption('primary-two')).toEqual(expect.objectContaining({
  label: '主画面布局',
  shortLabel: '主画面',
}));
```

Add to the seeded-room smoke test in `tests/app-smoke.test.tsx`:

```ts
expect(html).toContain('data-room-id="63136"');
expect(html).toContain('data-room-id="270888"');
```

- [ ] **Step 2: Run the UI tests and confirm the copy/selector failures**

Run: `npm test -- tests/ui-model.test.ts tests/app-smoke.test.tsx`

Expected: FAIL because the old label is still “主画面 + 两侧” and tiles have no `data-room-id` attribute.

- [ ] **Step 3: Rename the layout and add stable tile identity plus resize locking**

Replace the `primary-two` entry in `LAYOUT_OPTIONS`:

```ts
{ id: 'primary-two', label: '主画面布局', shortLabel: '主画面', hint: '拖动分隔线调整主画面比例' },
```

Extend `RoomTileProps`:

```ts
interface RoomTileProps {
  room: RoomSession;
  slot: LayoutSlot;
  index: number;
  controlsLocked?: boolean;
}
```

Change the component signature and control lock logic:

```ts
export function RoomTile({ room, slot, index, controlsLocked = false }: RoomTileProps) {
```

```ts
hideCleanupRef.current = scheduleControlsHide({
  locked: controlsLocked || focusWithin || menuOpen,
  onHide: () => setControlsVisible(false),
});
```

Update that callback dependency list to `[controlsLocked, focusWithin, menuOpen]`. Add this effect after `showControls`:

```ts
useEffect(() => {
  if (controlsLocked) setControlsVisible(true);
}, [controlsLocked]);
```

Add the stable selector to the `<article>`:

```tsx
data-room-id={room.roomId}
```

- [ ] **Step 4: Replace `WorkspaceGrid` with measured focus-layout integration**

Replace `src/renderer/components/WorkspaceGrid.tsx` with:

```tsx
import { Plus, Sparkles } from 'lucide-react';
import { useEffect, useMemo, useRef, useState, type CSSProperties } from 'react';
import {
  calculateLayout,
  calculatePrimaryFocusLayout,
  resolveLayoutId,
  type WorkspaceSize,
} from '../../domain/layout-engine';
import { normalizeRoomPlacementOrder } from '../store/room-placement';
import { useWorkspace } from '../store/workspace-context';
import { PrimaryRoomDivider } from './PrimaryRoomDivider';
import { RoomTile } from './RoomTile';

interface WorkspaceGridProps {
  onAddRoom: () => void;
}

const UNMEASURED_SIZE: WorkspaceSize = { width: 0, height: 0 };

export function WorkspaceGrid({ onAddRoom }: WorkspaceGridProps) {
  const rooms = useWorkspace((state) => state.rooms);
  const layoutId = useWorkspace((state) => state.layoutId);
  const primaryRoomId = useWorkspace((state) => state.primaryRoomId);
  const roomPlacementOrder = useWorkspace((state) => state.roomPlacementOrder);
  const primaryRoomRatio = useWorkspace((state) => state.primaryRoomRatio);
  const setPrimaryRoomRatio = useWorkspace((state) => state.setPrimaryRoomRatio);
  const gridRef = useRef<HTMLDivElement>(null);
  const [workspaceSize, setWorkspaceSize] = useState<WorkspaceSize>(UNMEASURED_SIZE);
  const [previewRatio, setPreviewRatio] = useState<number>();
  const [resizing, setResizing] = useState(false);
  const resolvedLayoutId = resolveLayoutId(layoutId, rooms.length);
  const activeRoomIds = rooms.map((room) => room.roomId);
  const orderedRoomIds = normalizeRoomPlacementOrder(activeRoomIds, roomPlacementOrder);
  const focusPlan = resolvedLayoutId === 'primary-two' && rooms.length > 0
    ? calculatePrimaryFocusLayout(
        orderedRoomIds,
        primaryRoomId,
        primaryRoomRatio,
        workspaceSize,
      )
    : undefined;
  const slots = focusPlan?.slots
    ?? calculateLayout(activeRoomIds, resolvedLayoutId, primaryRoomId);
  const slotsByRoomId = useMemo(
    () => new Map(slots.map((slot) => [slot.roomId, slot])),
    [slots],
  );
  const activeRatio = previewRatio ?? focusPlan?.effectiveRatio ?? primaryRoomRatio;
  const gridStyle = {
    '--room-count': rooms.length,
    '--primary-ratio': `${activeRatio * 100}%`,
    '--secondary-columns': focusPlan?.secondaryColumns ?? 1,
    '--secondary-rows': Math.max(1, focusPlan?.secondaryRows ?? 1),
  } as CSSProperties;

  useEffect(() => {
    const element = gridRef.current;
    if (!element || typeof ResizeObserver === 'undefined') return undefined;
    const updateSize = () => {
      const bounds = element.getBoundingClientRect();
      setWorkspaceSize({ width: bounds.width, height: bounds.height });
    };
    updateSize();
    const observer = new ResizeObserver(updateSize);
    observer.observe(element);
    return () => observer.disconnect();
  }, [resolvedLayoutId]);

  useEffect(() => {
    setPreviewRatio(undefined);
    setResizing(false);
  }, [primaryRoomId, resolvedLayoutId, rooms.length]);

  return (
    <main className="workspace-main">
      {rooms.length ? (
        <div
          ref={gridRef}
          className={`workspace-grid layout-${resolvedLayoutId} ${focusPlan && focusPlan.secondaryRows > 0 ? 'has-primary-divider' : ''} ${resizing ? 'is-resizing' : ''}`}
          style={gridStyle}
        >
          {rooms.map((room, index) => {
            const slot = slotsByRoomId.get(room.roomId);
            return slot ? (
              <RoomTile
                room={room}
                slot={slot}
                index={index}
                controlsLocked={resizing && room.roomId === primaryRoomId}
                key={room.roomId}
              />
            ) : null;
          })}
          {focusPlan && focusPlan.secondaryRows > 0 ? (
            <PrimaryRoomDivider
              orientation={focusPlan.orientation}
              value={activeRatio}
              availableRatios={focusPlan.availableRatios}
              onPreviewChange={setPreviewRatio}
              onCommit={setPrimaryRoomRatio}
              onDragStateChange={setResizing}
            />
          ) : null}
        </div>
      ) : (
        <div className="workspace-empty">
          <div className="empty-illustration"><Sparkles size={25} /></div>
          <h3>把直播间放进同一张画布</h3>
          <p>用房间号或主播名字添加第一路信号，弹幕会叠加在对应画面上。</p>
          <button className="button button-primary" type="button" onClick={onAddRoom}><Plus size={16} />添加第一路直播</button>
        </div>
      )}
    </main>
  );
}
```

- [ ] **Step 5: Add the focus grid and divider styles**

Replace the existing `.workspace-grid.layout-primary-two` rule and add the divider rules after it:

```css
.workspace-grid.layout-primary-two { grid-template-columns: minmax(0, 1fr); grid-template-rows: minmax(0, 1fr); }
.workspace-grid.layout-primary-two.has-primary-divider {
  grid-template-columns: var(--primary-ratio) 8px repeat(var(--secondary-columns), minmax(0, 1fr));
  grid-template-rows: repeat(var(--secondary-rows), minmax(135px, 1fr));
  transition: grid-template-columns 140ms ease;
}
.workspace-grid.layout-primary-two.has-primary-divider.is-resizing { cursor: col-resize; transition: none; user-select: none; }
.primary-room-divider { z-index: 8; display: grid; place-items: center; min-width: 0; min-height: 0; outline: 0; touch-action: none; }
.primary-room-divider.is-horizontal { grid-column: 2; grid-row: 1 / -1; cursor: col-resize; }
.primary-room-divider.is-vertical { grid-column: 1 / -1; grid-row: 2; cursor: row-resize; }
.primary-room-divider span { width: 2px; height: 42px; border-radius: 1px; background: #5d6875; transition: background 120ms ease, box-shadow 120ms ease; }
.primary-room-divider.is-vertical span { width: 42px; height: 2px; }
.primary-room-divider:hover span, .primary-room-divider:focus-visible span, .workspace-grid.is-resizing .primary-room-divider span { background: var(--orange); box-shadow: 0 0 0 2px rgba(255, 107, 53, 0.18); }
```

In the `max-width: 820px` section, remove `.layout-primary-two` from the selector that switches layouts to flex. Add these rules after that selector:

```css
.workspace-grid.layout-primary-two.has-primary-divider {
  display: grid;
  height: calc(100vh - 56px);
  min-height: calc(100vh - 56px);
  grid-template-columns: repeat(var(--secondary-columns), minmax(0, 1fr));
  grid-template-rows: var(--primary-ratio) 8px repeat(var(--secondary-rows), minmax(0, 1fr));
  transition: grid-template-rows 140ms ease;
}
.workspace-grid.layout-primary-two.has-primary-divider.is-resizing { cursor: row-resize; transition: none; }
.workspace-grid.layout-primary-two .room-tile { min-height: 0; flex: none; }
```

Extend the reduced-motion rule:

```css
@media (prefers-reduced-motion: reduce) {
  .room-tile .tile-topbar,
  .room-tile .tile-bottom-bar,
  .workspace-grid.layout-primary-two,
  .primary-room-divider span { transition: none; }
}
```

- [ ] **Step 6: Run focused tests and type checking**

Run: `npm test -- tests/ui-model.test.ts tests/app-smoke.test.tsx tests/primary-room-divider.test.tsx tests/layout-engine.test.ts; npm run typecheck`

Expected: PASS with no TypeScript diagnostics.

- [ ] **Step 7: Commit the renderer integration**

```powershell
git add src/renderer/components/WorkspaceGrid.tsx src/renderer/components/RoomTile.tsx src/renderer/ui-model.ts src/renderer/styles.css tests/ui-model.test.ts tests/app-smoke.test.tsx
git commit -m "接入主画面拖动布局"
```

### Task 6: Playwright interaction and node-preservation regression

**Files:**
- Create: `scripts/primary-room-resize-e2e.mjs`
- Modify: `package.json:scripts`

- [ ] **Step 1: Add the browser validation command**

Add this script to `package.json`:

```json
"test:primary-resize": "node scripts/primary-room-resize-e2e.mjs"
```

- [ ] **Step 2: Create the deterministic Playwright validation**

Create `scripts/primary-room-resize-e2e.mjs`:

```js
import { spawn } from 'node:child_process';
import { createServer } from 'node:net';
import { resolve } from 'node:path';
import { chromium } from 'playwright';

const projectRoot = resolve(import.meta.dirname, '..');

function assert(condition, message) {
  if (!condition) throw new Error(message);
}

async function reservePort() {
  const server = createServer();
  await new Promise((resolveListen, rejectListen) => {
    server.once('error', rejectListen);
    server.listen(0, '127.0.0.1', resolveListen);
  });
  const address = server.address();
  const port = typeof address === 'object' && address ? address.port : 0;
  await new Promise((resolveClose) => server.close(resolveClose));
  return port;
}

async function waitForServer(url) {
  const deadline = Date.now() + 20_000;
  while (Date.now() < deadline) {
    try {
      const response = await fetch(url);
      if (response.ok) return;
    } catch {
      await new Promise((resolveDelay) => setTimeout(resolveDelay, 150));
    }
  }
  throw new Error('VITE_SERVER_TIMEOUT');
}

async function addRoom(page, roomId) {
  await page.locator('button[aria-label="添加直播间"]').first().click();
  await page.locator('#room-search').fill(roomId);
  await page.locator('.room-search-form button[type="submit"]').click();
  await page.locator('.search-result').filter({ hasText: roomId }).first().click();
  await page.locator(`.room-tile[data-room-id="${roomId}"]`).waitFor({ state: 'visible' });
}

async function overlapCount(page) {
  return page.locator('.room-tile').evaluateAll((tiles) => {
    const rectangles = tiles.map((tile) => tile.getBoundingClientRect());
    let overlaps = 0;
    for (let leftIndex = 0; leftIndex < rectangles.length; leftIndex += 1) {
      for (let rightIndex = leftIndex + 1; rightIndex < rectangles.length; rightIndex += 1) {
        const left = rectangles[leftIndex];
        const right = rectangles[rightIndex];
        if (left.left < right.right && left.right > right.left && left.top < right.bottom && left.bottom > right.top) {
          overlaps += 1;
        }
      }
    }
    return overlaps;
  });
}

const port = await reservePort();
const url = `http://127.0.0.1:${port}`;
const viteProcess = spawn(
  process.execPath,
  [resolve(projectRoot, 'node_modules/vite/bin/vite.js'), '--host', '127.0.0.1', '--port', String(port), '--strictPort'],
  { cwd: projectRoot, stdio: 'ignore' },
);
let browser;

try {
  await waitForServer(url);
  browser = await chromium.launch({ headless: true });
  const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
  const consoleErrors = [];
  page.on('console', (message) => {
    if (message.type() === 'error') consoleErrors.push(message.text());
  });
  await page.addInitScript(() => localStorage.clear());
  await page.goto(url);
  assert(await page.locator('.primary-room-divider').count() === 0, 'DIVIDER_VISIBLE_OUTSIDE_PRIMARY_LAYOUT');
  await page.locator('.layout-menu-trigger').click();
  await page.locator('.layout-option').filter({ hasText: '主画面布局' }).click();

  const divider = page.locator('.primary-room-divider');
  await divider.waitFor({ state: 'visible' });
  const gridBounds = await page.locator('.workspace-grid').boundingBox();
  const dividerBounds = await divider.boundingBox();
  assert(gridBounds && dividerBounds, 'RESIZE_BOUNDS_UNAVAILABLE');
  await page.mouse.move(dividerBounds.x + dividerBounds.width / 2, dividerBounds.y + dividerBounds.height / 2);
  await page.mouse.down();
  await page.mouse.move(gridBounds.x + gridBounds.width * 0.64, dividerBounds.y + dividerBounds.height / 2, { steps: 8 });
  await page.waitForTimeout(3_200);
  assert(
    await page.locator('.room-tile.is-primary').evaluate((tile) => tile.classList.contains('controls-visible')),
    'PRIMARY_CONTROLS_HIDDEN_DURING_RESIZE',
  );
  await page.mouse.up();
  assert(await divider.getAttribute('aria-valuenow') === '67', 'RESIZE_DID_NOT_SNAP_TO_67');

  await divider.focus();
  await page.keyboard.press('Home');
  assert(await divider.getAttribute('aria-valuenow') === '50', 'HOME_DID_NOT_SELECT_MINIMUM');
  await page.keyboard.press('End');
  assert(await divider.getAttribute('aria-valuenow') === '67', 'END_DID_NOT_SELECT_MAXIMUM');
  await divider.dblclick();
  assert(await divider.getAttribute('aria-valuenow') === '60', 'DOUBLE_CLICK_DID_NOT_RESET');
  await page.mouse.move(dividerBounds.x + dividerBounds.width / 2, dividerBounds.y + dividerBounds.height / 2);
  await page.mouse.down();
  await page.mouse.move(gridBounds.x + gridBounds.width * 0.5, dividerBounds.y + dividerBounds.height / 2, { steps: 4 });
  await page.keyboard.press('Escape');
  await page.mouse.up();
  assert(await divider.getAttribute('aria-valuenow') === '60', 'ESCAPE_DID_NOT_CANCEL_PREVIEW');

  await page.evaluate(() => {
    globalThis.__primaryResizeNodes = new Map(
      [...document.querySelectorAll('.room-tile')].map((tile) => [tile.getAttribute('data-room-id'), {
        tile,
        playback: tile.querySelector('.signal-scene, .live-video-surface, .playback-state-surface'),
        danmaku: tile.querySelector('.danmaku-overlay'),
      }]),
    );
  });
  const targetTile = page.locator('.room-tile[data-room-id="270888"]');
  await targetTile.hover();
  await targetTile.getByRole('button', { name: '设 林深 为主画面' }).click();
  await page.locator('.room-tile[data-room-id="270888"].is-primary').waitFor({ state: 'visible' });
  const nodesPreserved = await page.evaluate(() => (
    [...document.querySelectorAll('.room-tile')].every((tile) => {
      const previous = globalThis.__primaryResizeNodes.get(tile.getAttribute('data-room-id'));
      return previous?.tile === tile
        && previous.playback === tile.querySelector('.signal-scene, .live-video-surface, .playback-state-surface')
        && previous.danmaku === tile.querySelector('.danmaku-overlay');
    })
  ));
  assert(nodesPreserved, 'ROOM_TILE_NODE_REPLACED');
  assert(await overlapCount(page) === 0, 'DESKTOP_TILE_OVERLAP');

  const thirdTile = page.locator('.room-tile[data-room-id="385729"]');
  await thirdTile.hover();
  await thirdTile.getByRole('button', { name: '白昼 更多操作' }).click();
  await thirdTile.getByRole('menuitem', { name: '移除房间' }).click();
  await page.waitForFunction(() => document.querySelectorAll('.room-tile').length === 2);
  assert(await overlapCount(page) === 0, 'TWO_ROOM_TILE_OVERLAP');

  const firstTile = page.locator('.room-tile[data-room-id="63136"]');
  await firstTile.hover();
  await firstTile.getByRole('button', { name: '星河 更多操作' }).click();
  await firstTile.getByRole('menuitem', { name: '移除房间' }).click();
  await page.waitForFunction(() => document.querySelectorAll('.room-tile').length === 1);
  assert(await page.locator('.primary-room-divider').count() === 0, 'SINGLE_ROOM_DIVIDER_VISIBLE');

  await page.setViewportSize({ width: 1920, height: 1080 });
  for (const roomId of ['63136', '385729', '4', '5', '6', '7', '8', '9']) {
    await addRoom(page, roomId);
  }
  assert(await page.locator('.room-tile').count() === 9, 'NINE_ROOM_COUNT_MISMATCH');
  assert(await overlapCount(page) === 0, 'NINE_ROOM_TILE_OVERLAP');

  await page.setViewportSize({ width: 390, height: 844 });
  await page.waitForFunction(() => (
    document.querySelector('.primary-room-divider')?.getAttribute('aria-orientation') === 'horizontal'
  ));
  assert(await overlapCount(page) === 0, 'NARROW_TILE_OVERLAP');
  assert(consoleErrors.length === 0, `CONSOLE_ERRORS:${consoleErrors.join('|')}`);
  process.stdout.write('PRIMARY_ROOM_RESIZE_E2E_PASS\n');
} finally {
  if (browser) await browser.close();
  viteProcess.kill();
}
```

- [ ] **Step 3: Run Playwright validation**

Run: `npm run test:primary-resize`

Expected: prints `PRIMARY_ROOM_RESIZE_E2E_PASS` and exits with code 0. The script must fail if pointer or keyboard adjustment is wrong, Escape does not cancel, reset does not return to 60%, a room tile DOM node changes identity, 2-room or 9-room layouts overlap, narrow mode keeps a vertical divider, or the browser console reports an error.

- [ ] **Step 4: Commit the interaction regression**

```powershell
git add package.json scripts/primary-room-resize-e2e.mjs
git commit -m "验证主画面拖动与无重挂载"
```

### Task 7: Full verification and documentation consistency

**Files:**
- Verify: `docs/superpowers/specs/2026-08-11-primary-room-resize-design.md`
- Verify: all files changed in Tasks 1-6

- [ ] **Step 1: Run the full automated test suite**

Run: `npm test`

Expected: all Vitest files PASS with no unhandled errors.

- [ ] **Step 2: Run type checking and production builds**

Run: `npm run typecheck; npm run build`

Expected: both commands exit with code 0; renderer, preload, and main bundles build successfully.

- [ ] **Step 3: Run focused Playwright interaction coverage again after the build**

Run: `npm run test:primary-resize`

Expected: `PRIMARY_ROOM_RESIZE_E2E_PASS`.

- [ ] **Step 4: Inspect the final diff for scope and accidental text corruption**

Run:

```powershell
git diff --check
git status --short
git diff --stat
rg -n "TBD|TODO|implement later|fill in details|主画面 \+ 两侧" src tests scripts package.json
```

Expected: `git diff --check` prints nothing; status lists only the planned feature files; the placeholder/old-copy scan prints nothing.

- [ ] **Step 5: Compare implementation against every acceptance criterion**

Confirm from the automated output and one desktop Electron run:

```text
1. Only primary-two with two or more rooms renders a divider.
2. Drag preview stays within 42%-70% and release snaps to 50%, 60%, or 67% when available.
3. Secondary rooms use all remaining grid space without overlap.
4. Every primary change swaps exactly two roomPlacementOrder positions.
5. data-room-id nodes keep identity across the swap.
6. Restart restores primaryRoomRatio and roomPlacementOrder.
7. Arrow keys, Home, End, Escape, and double-click operate the separator.
8. Desktop uses left/right; a 390px viewport uses top/bottom.
```

- [ ] **Step 6: Commit any verification-only correction, then leave the worktree clean**

If verification required a correction, stage only the corrected feature files and commit them:

```powershell
git add src tests scripts package.json
git commit -m "修正主画面拖动验收问题"
```

Run: `git status --short`

Expected: no output.
