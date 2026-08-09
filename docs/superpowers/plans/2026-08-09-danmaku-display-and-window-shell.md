# High-Density Danmaku and Window Shell Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the five-message overlay with configurable high-density scrolling danmaku, add a fully collapsible room sidebar, and replace the Electron system frame with secure in-app window controls.

**Architecture:** Keep received danmaku in the existing Zustand room store and move launch/collision decisions into a pure lane scheduler consumed by each `DanmakuOverlay`. Store one validated global settings object and sidebar state in workspace schema v2. Keep window commands behind typed preload methods and a dedicated main-process handler that only operates on the sender's window.

**Tech Stack:** TypeScript 7, React 19, Zustand, CSS animations, Electron 43, Vitest 4, Playwright 1.62, Vite 8

**Design reference:** `docs/superpowers/specs/2026-08-09-danmaku-display-and-window-shell-design.md`

**Repository note:** `D:\DouyuMonitor` is not a Git repository. Replace commit steps with local verification checkpoints; do not initialize Git as part of this plan.

---

## File Map

### Create

- `src/renderer/danmaku/danmaku-settings.ts`: settings types, defaults, range clamping, density profiles, and reset helpers.
- `src/renderer/danmaku/danmaku-lane-scheduler.ts`: pure track generation and collision-safe lane selection.
- `src/renderer/components/DanmakuSettingsPanel.tsx`: compact global settings popover.
- `src/renderer/components/WindowControls.tsx`: Electron-only minimize, maximize/restore, and close controls.
- `src/main/window-controls.ts`: sender-scoped window IPC handlers and maximize-state notifications.
- `tests/danmaku-settings.test.ts`: settings validation and defaults.
- `tests/danmaku-lane-scheduler.test.ts`: track and collision behavior.
- `tests/danmaku-settings-panel.test.tsx`: static markup and accessible labels.
- `tests/window-controls.test.ts`: main-process window control isolation.
- `tests/window-controls-render.test.tsx`: browser/Electron rendering behavior.

### Modify

- `src/renderer/store/workspace-persistence.ts`: schema v2 and v1 migration.
- `src/renderer/store/workspace-store.ts`: global settings and persisted sidebar state/actions.
- `src/renderer/store/workspace-context.tsx`: initial sidebar option.
- `src/renderer/store/danmaku-store.ts`: 300-message intake queue and atomic dequeue; remove the five-visible-message model.
- `src/renderer/store/danmaku-context.tsx`: remove the fixed 333 ms launch loop and expose dequeue.
- `src/renderer/components/DanmakuOverlay.tsx`: per-room launch runtime, measurement, resize handling, animation cleanup.
- `src/renderer/components/AppHeader.tsx`: settings trigger, persistent sidebar toggle, and window controls.
- `src/renderer/components/RoomSidebar.tsx`: explicit open/closed classes and accessibility state.
- `src/renderer/components/WorkspaceGrid.tsx`: left-edge sidebar restore button.
- `src/renderer/App.tsx`: read sidebar state from the workspace store.
- `src/renderer/main.tsx`: pass the viewport-derived initial sidebar state.
- `src/renderer/styles.css`: multi-track animation, settings panel, collapsed sidebar, drag regions, and window controls.
- `src/shared/ipc-contract.ts`: exact window control channels.
- `src/preload/bridge.ts`: typed window methods and maximize event subscription.
- `src/shared/window-api.d.ts`: make `appApi` optional for browser mode.
- `src/main/main-config.ts`: frameless window options.
- `src/main/main.ts`: register window handlers and state notifications.
- Existing tests under `tests/`: update old five-message, persistence, smoke, preload, and main configuration expectations.

---

### Task 1: Define and Validate Global Danmaku Settings

**Files:**
- Create: `src/renderer/danmaku/danmaku-settings.ts`
- Create: `tests/danmaku-settings.test.ts`

- [ ] **Step 1: Write failing tests for defaults, clamping, enum fallback, density profiles, and slider mapping**

```ts
import { describe, expect, it } from 'vitest';
import {
  DEFAULT_DANMAKU_SETTINGS,
  getDanmakuDensityProfile,
  parseDanmakuSettings,
  sliderValueToDuration,
} from '../src/renderer/danmaku/danmaku-settings';

describe('danmaku settings', () => {
  it('uses the approved defaults', () => {
    expect(DEFAULT_DANMAKU_SETTINGS).toEqual({
      durationSeconds: 8,
      fontSize: 24,
      opacity: 0.9,
      region: 'full',
      density: 'normal',
      fontFamily: 'microsoft-yahei',
      rendering: 'native',
    });
  });

  it('clamps numeric values and rejects unknown options field by field', () => {
    expect(parseDanmakuSettings({
      durationSeconds: 99,
      fontSize: 2,
      opacity: -1,
      region: 'middle',
      density: 'massive',
      fontFamily: 'comic-sans',
      rendering: 'advanced',
    })).toEqual({
      ...DEFAULT_DANMAKU_SETTINGS,
      durationSeconds: 15,
      fontSize: 14,
      opacity: 0.3,
      density: 'massive',
      rendering: 'advanced',
    });
  });

  it('maps the visual speed slider from slow-left to fast-right', () => {
    expect(sliderValueToDuration(0)).toBe(15);
    expect(sliderValueToDuration(100)).toBe(4);
  });

  it('returns the approved per-room density limits', () => {
    expect(getDanmakuDensityProfile('massive')).toEqual({ laneRatio: 1, intervalMs: 80 });
    expect(getDanmakuDensityProfile('normal')).toEqual({ laneRatio: 0.7, intervalMs: 180 });
    expect(getDanmakuDensityProfile('reduced')).toEqual({ laneRatio: 0.4, intervalMs: 360 });
  });
});
```

- [ ] **Step 2: Run the test and confirm the module is missing**

Run: `npx vitest run tests/danmaku-settings.test.ts`

Expected: FAIL because `src/renderer/danmaku/danmaku-settings.ts` does not exist.

- [ ] **Step 3: Add the settings type, constants, parser, and mapping helpers**

```ts
export type DanmakuRegion = 'full' | 'top' | 'bottom';
export type DanmakuDensity = 'massive' | 'normal' | 'reduced';
export type DanmakuFontFamily = 'simhei' | 'microsoft-yahei';
export type DanmakuRendering = 'native' | 'advanced';

export interface DanmakuSettings {
  durationSeconds: number;
  fontSize: number;
  opacity: number;
  region: DanmakuRegion;
  density: DanmakuDensity;
  fontFamily: DanmakuFontFamily;
  rendering: DanmakuRendering;
}

export const DEFAULT_DANMAKU_SETTINGS: Readonly<DanmakuSettings> = Object.freeze({
  durationSeconds: 8,
  fontSize: 24,
  opacity: 0.9,
  region: 'full',
  density: 'normal',
  fontFamily: 'microsoft-yahei',
  rendering: 'native',
});

const clamp = (value: unknown, min: number, max: number, fallback: number) => (
  typeof value === 'number' && Number.isFinite(value)
    ? Math.min(max, Math.max(min, value))
    : fallback
);

const oneOf = <T extends string>(value: unknown, allowed: readonly T[], fallback: T): T => (
  typeof value === 'string' && allowed.includes(value as T) ? value as T : fallback
);

export function parseDanmakuSettings(value: unknown): DanmakuSettings {
  const source = value && typeof value === 'object' ? value as Record<string, unknown> : {};
  return {
    durationSeconds: clamp(source.durationSeconds, 4, 15, 8),
    fontSize: clamp(source.fontSize, 14, 36, 24),
    opacity: clamp(source.opacity, 0.3, 1, 0.9),
    region: oneOf(source.region, ['full', 'top', 'bottom'], 'full'),
    density: oneOf(source.density, ['massive', 'normal', 'reduced'], 'normal'),
    fontFamily: oneOf(source.fontFamily, ['simhei', 'microsoft-yahei'], 'microsoft-yahei'),
    rendering: oneOf(source.rendering, ['native', 'advanced'], 'native'),
  };
}

export function sliderValueToDuration(value: number): number {
  const clamped = Math.min(100, Math.max(0, value));
  return Math.round((15 - clamped * 0.11) * 10) / 10;
}

export function durationToSliderValue(duration: number): number {
  return Math.round((15 - Math.min(15, Math.max(4, duration))) / 0.11);
}

export function getDanmakuDensityProfile(density: DanmakuDensity) {
  return density === 'massive'
    ? { laneRatio: 1, intervalMs: 80 }
    : density === 'reduced'
      ? { laneRatio: 0.4, intervalMs: 360 }
      : { laneRatio: 0.7, intervalMs: 180 };
}
```

- [ ] **Step 4: Run the settings tests**

Run: `npx vitest run tests/danmaku-settings.test.ts`

Expected: PASS.

---

### Task 2: Migrate Workspace Persistence to Schema v2

**Files:**
- Modify: `src/renderer/store/workspace-persistence.ts`
- Modify: `src/renderer/store/workspace-store.ts`
- Modify: `src/renderer/store/workspace-context.tsx`
- Modify: `tests/workspace-persistence.test.ts`
- Modify: `tests/workspace-store.test.ts`

- [ ] **Step 1: Add failing migration and state-action tests**

Add assertions that a v1 JSON snapshot loads with `DEFAULT_DANMAKU_SETTINGS`, that invalid v2 fields fall back independently, and that v2 round-trips `sidebarOpen`. Add this store behavior test:

```ts
it('persists global danmaku settings and sidebar state', () => {
  const storage = createMemoryStorage();
  const first = createWorkspaceStore(createMockDouyuAdapter(), {
    storage,
    initialSidebarOpen: true,
  });

  first.getState().setDanmakuSettings({ fontSize: 30, density: 'massive' });
  first.getState().setSidebarOpen(false);

  const restored = createWorkspaceStore(createMockDouyuAdapter(), {
    storage,
    initialSidebarOpen: true,
  });
  expect(restored.getState().danmakuSettings).toEqual(expect.objectContaining({
    fontSize: 30,
    density: 'massive',
  }));
  expect(restored.getState().sidebarOpen).toBe(false);
});
```

- [ ] **Step 2: Run the focused tests and confirm missing v2 fields/actions**

Run: `npx vitest run tests/workspace-persistence.test.ts tests/workspace-store.test.ts`

Expected: FAIL on schema version, `danmakuSettings`, `sidebarOpen`, and missing setters.

- [ ] **Step 3: Implement v1 migration and v2 parsing**

Set `WORKSPACE_SCHEMA_VERSION = 2`, accept input versions 1 and 2, and return a normalized v2 snapshot. Define the new fields as:

```ts
export interface WorkspaceSnapshot {
  schemaVersion: typeof WORKSPACE_SCHEMA_VERSION;
  rooms: PersistedRoom[];
  layoutId: LayoutId;
  primaryRoomId?: string;
  audioRoomId?: string;
  globalDanmakuEnabled: boolean;
  globalMuted: boolean;
  danmakuSettings: DanmakuSettings;
  sidebarOpen?: boolean;
}
```

Use `parseDanmakuSettings(parsed.danmakuSettings)` for both versions. For v1, preserve all existing fields and set `sidebarOpen` to `undefined` so the current viewport default still applies.

- [ ] **Step 4: Add workspace state and partial update actions**

Add these members to `WorkspaceState`:

```ts
danmakuSettings: DanmakuSettings;
sidebarOpen: boolean;
setDanmakuSettings(patch: Partial<DanmakuSettings>): void;
resetDanmakuSetting(key: 'durationSeconds' | 'fontSize' | 'opacity'): void;
setSidebarOpen(open: boolean): void;
```

Initialize from the migrated snapshot and implement updates by merging through `parseDanmakuSettings` before persisting. Persist `schemaVersion: 2`, `danmakuSettings`, and `sidebarOpen` with the existing non-sensitive workspace fields.

- [ ] **Step 5: Run persistence and workspace tests**

Run: `npx vitest run tests/workspace-persistence.test.ts tests/workspace-store.test.ts`

Expected: PASS, including v1 migration and v2 round-trip.

---

### Task 3: Replace the Five-Message Store with a 300-Message Intake Queue

**Files:**
- Modify: `src/renderer/store/danmaku-store.ts`
- Modify: `src/renderer/store/danmaku-context.tsx`
- Modify: `tests/danmaku-store.test.ts`

- [ ] **Step 1: Replace old cap tests with queue and atomic dequeue tests**

```ts
it('keeps the newest three hundred waiting messages', () => {
  const store = createDanmakuStore();
  store.getState().syncRoom('63136', true);
  store.getState().handleEvent(messageBatch('63136', 350));
  expect(store.getState().rooms['63136'].pending).toHaveLength(300);
  expect(store.getState().rooms['63136'].pending[0].id).toBe('50');
  expect(store.getState().rooms['63136'].dropped).toBe(50);
});

it('dequeues only the expected head message', () => {
  const store = createDanmakuStore();
  store.getState().syncRoom('63136', true);
  store.getState().handleEvent(messageBatch('63136', 2));
  expect(store.getState().takePending('63136', '1')).toBe(false);
  expect(store.getState().takePending('63136', '0')).toBe(true);
  expect(store.getState().rooms['63136'].pending.map((message) => message.id)).toEqual(['1']);
});
```

- [ ] **Step 2: Run the store test and confirm it fails on the old cap and actions**

Run: `npx vitest run tests/danmaku-store.test.ts`

Expected: FAIL because the queue stops at 50 and `takePending` does not exist.

- [ ] **Step 3: Simplify room view and store actions**

Set `MAX_PENDING_DANMAKU = 300`. Remove `MAX_VISIBLE_DANMAKU`, `visible`, `tick`, and `expireMessage`. Add:

```ts
takePending(roomId: string, expectedMessageId: string): boolean;
```

Implement it synchronously: verify `pending[0]?.id === expectedMessageId`, remove that head with `slice(1)`, and return whether removal occurred. Keep deduplication and upstream/local dropped counts. Disabling or removing a room clears its queue and dedupe state.

- [ ] **Step 4: Remove the provider's 333 ms interval and expose dequeue**

Replace `useDanmakuExpire` with:

```ts
export function useDanmakuTake(): (roomId: string, messageId: string) => boolean {
  const context = useContext(DanmakuContext);
  return useCallback(
    (roomId, messageId) => context?.store.getState().takePending(roomId, messageId) ?? false,
    [context],
  );
}
```

Leave source subscription and room lifecycle behavior unchanged.

- [ ] **Step 5: Run danmaku store and source integration tests**

Run: `npx vitest run tests/danmaku-store.test.ts tests/renderer-danmaku-source.test.ts tests/danmaku-session-manager.test.ts`

Expected: PASS.

---

### Task 4: Build the Pure Multi-Lane Scheduler

**Files:**
- Create: `src/renderer/danmaku/danmaku-lane-scheduler.ts`
- Create: `tests/danmaku-lane-scheduler.test.ts`

- [ ] **Step 1: Write failing tests for track count, region offsets, safe reuse, and catch-up rejection**

```ts
import { describe, expect, it } from 'vitest';
import {
  calculateLanes,
  selectLane,
  type ActiveDanmakuGeometry,
} from '../src/renderer/danmaku/danmaku-lane-scheduler';

describe('danmaku lane scheduler', () => {
  it('uses all, seventy percent, or forty percent of physical lanes', () => {
    expect(calculateLanes(540, 24, 'full', 'massive')).toHaveLength(16);
    expect(calculateLanes(540, 24, 'full', 'normal')).toHaveLength(12);
    expect(calculateLanes(540, 24, 'full', 'reduced')).toHaveLength(7);
  });

  it('offsets bottom-region tracks below the midpoint', () => {
    expect(calculateLanes(540, 24, 'bottom', 'massive')[0].top).toBe(270);
  });

  it('rejects a lane whose predecessor can be caught before exit', () => {
    const active: ActiveDanmakuGeometry[] = [{
      laneIndex: 0,
      width: 420,
      containerWidth: 800,
      launchedAt: 0,
      durationMs: 15000,
    }];
    expect(selectLane([{ index: 0, top: 0 }], active, {
      width: 80,
      containerWidth: 800,
      launchedAt: 500,
      durationMs: 4000,
      fontSize: 24,
    })).toBeUndefined();
  });
});
```

- [ ] **Step 2: Run the scheduler test and confirm the module is missing**

Run: `npx vitest run tests/danmaku-lane-scheduler.test.ts`

Expected: FAIL because the scheduler module does not exist.

- [ ] **Step 3: Implement lane calculation**

Use `Math.max(1, Math.floor(regionHeight / (fontSize * 1.35)))` for physical lanes. Select `Math.max(1, Math.ceil(physicalCount * laneRatio))` tracks and return stable `{ index, top }` entries. `top` starts at `0` for full/top and `containerHeight / 2` for bottom.

- [ ] **Step 4: Implement collision-safe lane selection**

For each lane, compute the predecessor's current tail position and candidate speed. A lane is safe only when the predecessor tail is at least `Math.max(12, fontSize * 0.5)` pixels past the right edge and the candidate cannot reach the predecessor before it exits. Return the first safe lane, otherwise `undefined`.

Export concrete interfaces for `DanmakuLane`, `ActiveDanmakuGeometry`, and `DanmakuLaunchGeometry`. Keep the module free of React, DOM, timers, and global mutable state.

- [ ] **Step 5: Run scheduler tests**

Run: `npx vitest run tests/danmaku-lane-scheduler.test.ts`

Expected: PASS for region, density, long-message, and catch-up cases.

---

### Task 5: Render Continuous High-Density Danmaku

**Files:**
- Modify: `src/renderer/components/DanmakuOverlay.tsx`
- Modify: `src/renderer/styles.css`
- Modify: `tests/danmaku-overlay.test.tsx`
- Modify: `tests/room-playback-surface.test.tsx`
- Modify: `tests/room-playback-live.test.tsx`

- [ ] **Step 1: Update static rendering tests for absolute tracks and style variables**

Test an exported presentational `DanmakuLines` using launched messages instead of raw pending messages:

```ts
expect(html).toContain('class="danmaku-line danmaku-rendering-native"');
expect(html).toContain('--danmaku-top:0px');
expect(html).toContain('--danmaku-duration:8000ms');
expect(html).toContain('User：');
expect(html).not.toContain('<script>');
```

Add a case with at least 12 launched messages and assert all 12 IDs render. This prevents reintroducing a five-message cap.

- [ ] **Step 2: Run overlay tests and confirm the old component shape fails**

Run: `npx vitest run tests/danmaku-overlay.test.tsx tests/room-playback-surface.test.tsx tests/room-playback-live.test.tsx`

Expected: FAIL because `DanmakuLines` still accepts `DanmakuMessage[]` and uses `danmaku-lift`.

- [ ] **Step 3: Implement the overlay runtime**

Define a local launch record:

```ts
interface LaunchedDanmaku {
  message: DanmakuMessage;
  laneIndex: number;
  top: number;
  width: number;
  containerWidth: number;
  launchedAt: number;
  durationMs: number;
  fontSize: number;
}
```

`DanmakuOverlay` must:

1. Read `room.pending[0]`, global `danmakuSettings`, and `useDanmakuTake()`.
2. Measure its full overlay with `ResizeObserver`, with a window `resize` fallback.
3. On a per-room timer using the selected density interval, measure the next message, call `calculateLanes` and `selectLane`, atomically dequeue the expected ID, then append a `LaunchedDanmaku`.
4. Remove active entries on `animationend` and with a `durationMs + 1000` fallback timer.
5. Clear active entries when `enabled` becomes false or the component unmounts.

Measure text with a temporary hidden span using the selected font family, font size, and nickname weight. If DOM measurement returns zero or throws, use `Array.from(nickname + '：' + text).length * fontSize`.

- [ ] **Step 4: Replace the old vertical CSS**

Use a full-video overlay and absolute lines:

```css
.danmaku-overlay {
  position: absolute;
  inset: 0;
  z-index: 3;
  overflow: hidden;
  pointer-events: none;
}

.danmaku-line {
  position: absolute;
  top: var(--danmaku-top);
  left: 100%;
  width: max-content;
  max-width: none;
  color: rgba(255,255,255,var(--danmaku-opacity));
  font-family: var(--danmaku-font-family);
  font-size: var(--danmaku-font-size);
  line-height: 1.35;
  white-space: nowrap;
  will-change: transform;
  animation: danmaku-scroll var(--danmaku-duration) linear forwards;
}

@keyframes danmaku-scroll {
  from { transform: translate3d(0, 0, 0); }
  to { transform: translate3d(var(--danmaku-travel), 0, 0); }
}
```

Set `--danmaku-travel` on each launched line to the negative pixel sum of its measured tile width and message width. This keeps every animation inside its own tile instead of using viewport width.

- [ ] **Step 5: Run all danmaku tests**

Run: `npx vitest run tests/danmaku-settings.test.ts tests/danmaku-lane-scheduler.test.ts tests/danmaku-store.test.ts tests/danmaku-overlay.test.tsx tests/room-playback-surface.test.tsx tests/room-playback-live.test.tsx`

Expected: PASS and no assertion refers to a five-message cap.

---

### Task 6: Add the Global Danmaku Settings Panel

**Files:**
- Create: `src/renderer/components/DanmakuSettingsPanel.tsx`
- Modify: `src/renderer/components/AppHeader.tsx`
- Modify: `src/renderer/styles.css`
- Create: `tests/danmaku-settings-panel.test.tsx`
- Modify: `tests/app-smoke.test.tsx`

- [ ] **Step 1: Write failing static markup tests for every approved control**

Render the panel through a `WorkspaceProvider` and assert the labels `弹幕速度`, `弹幕大小`, `不透明度`, `弹幕区域`, `弹幕数量`, `弹幕字体`, and `字体渲染`. Assert three range inputs, three reset buttons, and pressed states for `全屏`, `正常`, `微软雅黑`, and `原生`.

- [ ] **Step 2: Run the panel and smoke tests**

Run: `npx vitest run tests/danmaku-settings-panel.test.tsx tests/app-smoke.test.tsx`

Expected: FAIL because the settings trigger and panel do not exist.

- [ ] **Step 3: Implement the controlled panel**

`DanmakuSettingsPanel` accepts `open` and `onClose`, reads settings/actions from `useWorkspace`, and renders:

```tsx
<label className="danmaku-setting-slider">
  <span>弹幕速度</span>
  <input
    type="range"
    min="0"
    max="100"
    value={durationToSliderValue(settings.durationSeconds)}
    onChange={(event) => setDanmakuSettings({
      durationSeconds: sliderValueToDuration(Number(event.currentTarget.value)),
    })}
  />
  <button type="button" onClick={() => resetDanmakuSetting('durationSeconds')}>重置</button>
</label>
```

Repeat with direct ranges for size and opacity. Render option groups as buttons with `aria-pressed`. Add document listeners only while open; Escape and pointer-down outside call `onClose`.

- [ ] **Step 4: Add the header trigger and styles**

Place a `Settings2` icon button immediately after the global danmaku toggle. The trigger uses `aria-expanded`, `aria-controls="danmaku-settings-panel"`, and an accessible label. Style a fixed-width compact popover aligned to the right edge, with 8 px or smaller radius and mobile width constrained by the viewport.

- [ ] **Step 5: Run settings UI tests**

Run: `npx vitest run tests/danmaku-settings-panel.test.tsx tests/app-smoke.test.tsx`

Expected: PASS.

---

### Task 7: Make the Room Sidebar Fully Collapsible

**Files:**
- Modify: `src/renderer/App.tsx`
- Modify: `src/renderer/main.tsx`
- Modify: `src/renderer/store/workspace-context.tsx`
- Modify: `src/renderer/components/AppHeader.tsx`
- Modify: `src/renderer/components/RoomSidebar.tsx`
- Modify: `src/renderer/components/WorkspaceGrid.tsx`
- Modify: `src/renderer/styles.css`
- Modify: `tests/app-smoke.test.tsx`
- Modify: `tests/workspace-store.test.ts`

- [ ] **Step 1: Add failing tests for persisted state and accessible controls**

Keep `getInitialSidebarOpen(390) === false` and `getInitialSidebarOpen(1440) === true`. Add static markup assertions for `aria-expanded`, `room-sidebar is-open`, and the absence of a duplicate canvas-edge restore button. The store persistence test from Task 2 covers restart behavior.

- [ ] **Step 2: Run focused tests and confirm desktop collapse behavior is absent**

Run: `npx vitest run tests/app-smoke.test.tsx tests/workspace-store.test.ts`

Expected: FAIL because `App` owns non-persisted local state and the desktop toggle is mobile-only.

- [ ] **Step 3: Move sidebar state into the workspace store**

Add `initialSidebarOpen?: boolean` to `WorkspaceProviderProps` and pass it to `createWorkspaceStore`. In `main.tsx`, pass `getInitialSidebarOpen(window.innerWidth)`. In `App`, select `sidebarOpen` and `setSidebarOpen` from the store instead of calling `useState`.

- [ ] **Step 4: Add desktop collapse classes and keep one header control**

`RoomSidebar` renders `room-sidebar is-open` or `room-sidebar is-closed` and sets `aria-hidden={!isOpen}`. The header toggle is no longer `mobile-only`, always reports `aria-expanded`, and is the only sidebar open/close control.

- [ ] **Step 5: Add responsive CSS**

Desktop `.room-sidebar.is-closed` uses `width: 0`, `flex-basis: 0`, `border: 0`, `overflow: hidden`, and `visibility: hidden`. Keep the existing transform-based mobile overlay under 820 px. Do not render a duplicate restore control inside `.workspace-main`.

- [ ] **Step 6: Run sidebar tests**

Run: `npx vitest run tests/app-smoke.test.tsx tests/workspace-store.test.ts tests/workspace-persistence.test.ts`

Expected: PASS.

---

### Task 8: Add Sender-Scoped Electron Window IPC

**Files:**
- Modify: `src/shared/ipc-contract.ts`
- Modify: `src/preload/bridge.ts`
- Create: `src/main/window-controls.ts`
- Modify: `src/main/main.ts`
- Modify: `src/main/main-config.ts`
- Modify: `src/shared/window-api.d.ts`
- Modify: `tests/ipc-contract.test.ts`
- Modify: `tests/preload-bridge.test.ts`
- Create: `tests/window-controls.test.ts`
- Modify: `tests/main-config.test.ts`

- [ ] **Step 1: Write failing IPC and secure-config tests**

Add exact channels:

```ts
expect(IPC_CHANNELS.windowMinimize).toBe('window.minimize');
expect(IPC_CHANNELS.windowToggleMaximize).toBe('window.toggleMaximize');
expect(IPC_CHANNELS.windowClose).toBe('window.close');
expect(IPC_CHANNELS.windowMaximizedChanged).toBe('window.maximizedChanged');
```

Assert `createBrowserWindowOptions()` returns `frame: false` and `autoHideMenuBar: true` while retaining the existing sandbox settings.

- [ ] **Step 2: Write a failing sender-isolation test**

```ts
it('operates only on the window resolved from the IPC sender', async () => {
  const target = {
    minimize: vi.fn(),
    maximize: vi.fn(),
    unmaximize: vi.fn(),
    close: vi.fn(),
    isMaximized: vi.fn(() => false),
  };
  const { ipcMain, handlers } = createFakeIpcMain();
  registerWindowControlHandlers(ipcMain, (sender) => sender.id === 7 ? target : undefined);

  await handlers.get(IPC_CHANNELS.windowMinimize)?.({ sender: { id: 7 } });
  await handlers.get(IPC_CHANNELS.windowClose)?.({ sender: { id: 8 } });

  expect(target.minimize).toHaveBeenCalledOnce();
  expect(target.close).not.toHaveBeenCalled();
});
```

- [ ] **Step 3: Run IPC tests and confirm channels/module are missing**

Run: `npx vitest run tests/ipc-contract.test.ts tests/preload-bridge.test.ts tests/window-controls.test.ts tests/main-config.test.ts`

Expected: FAIL on missing channels, methods, module, and frameless options.

- [ ] **Step 4: Implement typed preload methods**

Extend `AppApi` with `minimizeWindow`, `toggleMaximizeWindow`, `closeWindow`, and `onMaximizedChanged`. Each command invokes one fixed channel without a payload. The event subscription accepts only boolean payloads and removes the exact wrapper on unsubscribe. Update the existing `Object.keys(api)` whitelist expectation.

- [ ] **Step 5: Implement sender-scoped main handlers**

Export a testable function:

```ts
export function registerWindowControlHandlers(
  ipcMain: WindowIpcMainLike,
  resolveWindow: (sender: WindowIpcSender) => WindowControlTarget | undefined,
): void {
  ipcMain.handle(IPC_CHANNELS.windowMinimize, async (event) => {
    resolveWindow(event.sender)?.minimize();
  });
  ipcMain.handle(IPC_CHANNELS.windowToggleMaximize, async (event) => {
    const window = resolveWindow(event.sender);
    if (!window) return;
    if (window.isMaximized()) window.unmaximize(); else window.maximize();
  });
  ipcMain.handle(IPC_CHANNELS.windowClose, async (event) => {
    resolveWindow(event.sender)?.close();
  });
}
```

Add `wireMaximizedNotifications(window)` to send booleans on `maximize` and `unmaximize`.

- [ ] **Step 6: Wire Electron without weakening security**

In `bootstrap`, register the handlers with `BrowserWindow.fromWebContents`. In `createMainWindow`, wire maximize notifications. Add `frame: false` and `autoHideMenuBar: true` to the typed secure options. Keep context isolation, disabled Node integration, sandboxing, and the external-window deny handler unchanged.

- [ ] **Step 7: Run all window boundary tests**

Run: `npx vitest run tests/ipc-contract.test.ts tests/preload-bridge.test.ts tests/window-controls.test.ts tests/main-config.test.ts`

Expected: PASS.

---

### Task 9: Render Window Controls and Drag Regions

**Files:**
- Create: `src/renderer/components/WindowControls.tsx`
- Modify: `src/renderer/components/AppHeader.tsx`
- Modify: `src/renderer/styles.css`
- Create: `tests/window-controls-render.test.tsx`
- Modify: `tests/app-smoke.test.tsx`

- [ ] **Step 1: Write failing browser/Electron render tests**

Export `WindowControls` with optional `api` and `initialMaximized` props for test injection. Assert it returns empty markup when no API exists, renders buttons labelled `最小化`, `最大化`, and `关闭` with a fake API, and uses `还原` when `initialMaximized` is true. Playwright covers the runtime maximize event because server-side static rendering does not run effects.

- [ ] **Step 2: Run render tests and confirm the component is missing**

Run: `npx vitest run tests/window-controls-render.test.tsx tests/app-smoke.test.tsx`

Expected: FAIL because `WindowControls` does not exist.

- [ ] **Step 3: Implement window controls**

Use Lucide `Minus`, `Square`, `Copy`, and `X` icons. Initialize state from `initialMaximized ?? false`, subscribe to `onMaximizedChanged` in `useEffect`, and clean up on unmount. Call only the typed API methods. Do not render the group when `api` is undefined.

- [ ] **Step 4: Add drag/no-drag CSS**

Set the top header to a drag region and all interactive descendants to no-drag:

```css
.app-header { -webkit-app-region: drag; }
.app-header button,
.app-header input,
.app-header select,
.app-header [role='menu'] { -webkit-app-region: no-drag; }
```

Give all three controls stable square dimensions. Use the standard Windows close hover color and keep the controls aligned at the far right without overlapping the settings popover.

- [ ] **Step 5: Run renderer window tests**

Run: `npx vitest run tests/window-controls-render.test.tsx tests/app-smoke.test.tsx`

Expected: PASS.

---

### Task 10: Full Verification and Windows Packaging

**Files:**
- Modify only if verification exposes an in-scope defect.
- Inspect: `release/DouyuMonitor-Setup-0.1.0-x64.exe`

- [ ] **Step 1: Run the full automated test suite**

Run: `npm test`

Expected: all test files and tests pass with zero unhandled errors.

- [ ] **Step 2: Run static and production build checks**

Run: `npm run typecheck`

Expected: exit code 0 and no TypeScript diagnostics.

Run: `npm run build`

Expected: main, preload, and renderer builds complete successfully.

- [ ] **Step 3: Start the renderer and run Playwright UI checks**

Run: `npm run dev`

Use the printed localhost URL. In browser mode verify:

1. The settings gear opens a compact panel with all seven approved controls.
2. Sliders and option buttons update all visible room overlays.
3. A burst renders more than five simultaneous `.danmaku-line` elements and they move right-to-left.
4. Lines stay within their selected full/top/bottom region.
5. Sidebar collapse releases its width and the left-edge button restores it.
6. At 390 px width, the settings panel fits and the sidebar remains an overlay.
7. No controls overlap at 1440x900, 1024x768, and 390x844.
8. Browser mode does not show Electron window controls.

- [ ] **Step 4: Run Electron runtime checks**

Run: `npm start`

Verify the system title bar and menu are absent, the empty header area drags the window, interactive header controls remain clickable, maximize changes the icon/label to restore, and minimize/restore/close affect only the app window.

- [ ] **Step 5: Exercise the nine-room burst boundary**

In demo/browser mode, add nine rooms and inject 300 messages per room through the existing mock source or a temporary Playwright page hook. Confirm each room queue stays at or below 300, settings remain responsive, active nodes return to zero after animations, and no uncaught console errors appear. Remove any temporary hook before the final build.

- [ ] **Step 6: Build the Windows installer**

Run: `npm run dist:win`

Expected: `release/DouyuMonitor-Setup-0.1.0-x64.exe` exists and electron-builder exits with code 0. Record its SHA-256 with:

```powershell
Get-FileHash -Algorithm SHA256 'release\DouyuMonitor-Setup-0.1.0-x64.exe'
```

- [ ] **Step 7: Inspect the final change scope**

Run: `Get-ChildItem 'src','tests','docs\superpowers' -Recurse -File | Where-Object LastWriteTime -gt (Get-Date).AddHours(-6) | Select-Object FullName`

Expected: only files listed in this plan plus generated build artifacts changed. Confirm no playback URL, token, Cookie, sidecar output, or credential appears in test logs or new source.
