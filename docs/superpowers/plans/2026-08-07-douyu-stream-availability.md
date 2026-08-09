# Douyu Stream Availability Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a compliance-gated stream availability check that reports why a Douyu room cannot supply an independent public playback URL, removes fake rooms from Electron startup, and preserves the browser demo.

**Architecture:** Extend `DouyuAdapter` with a typed availability query. The production HTTP adapter reads only public `betard` room metadata and returns a normal `blocked` result; typed IPC carries that result to the renderer. The renderer stores availability per room and renders checking, blocked, and error states without calling signed playback APIs or constructing guessed URLs.

**Tech Stack:** TypeScript, Electron 43, React 19, Zustand, Vite, Vitest, Node 24 built-in `fetch`.

---

## Scope and Safety Gates

- Source design: `docs/superpowers/specs/2026-08-07-douyu-stream-availability-design.md`.
- Never call `www.douyu.com/lapi/live/getH5Play` from production code, tests, smoke scripts, or Electron DevTools.
- Never generate signatures, execute Douyu page scripts, embed the official page, or guess CDN URLs.
- `blocked` is a successful business result. Network and response-shape failures remain typed errors.
- The workspace has no `.git` directory. Do not initialize Git; version-control steps are intentionally absent.

## File Structure

- Modify `src/domain/douyu-adapter.ts`: stream availability domain types and required adapter method.
- Modify `src/infrastructure/mock-douyu-adapter.ts`: browser-demo `available` response using `mock:` URLs.
- Modify `src/infrastructure/douyu-http-adapter.ts`: public `betard` capability probe.
- Create `tests/fixtures/douyu-betard-api.json`: sanitized room and `multirates` fixture.
- Modify `tests/douyu-http-adapter.test.ts` and `tests/mock-douyu-adapter.test.ts`: adapter contracts.
- Modify `src/shared/ipc-contract.ts`, `src/main/ipc-handlers.ts`, `src/preload/bridge.ts`: typed white-listed IPC.
- Modify `tests/ipc-contract.test.ts`, `tests/ipc-handlers.test.ts`, `tests/preload-bridge.test.ts`: IPC and preload tests.
- Modify `src/infrastructure/renderer-douyu-adapter.ts` and `tests/renderer-douyu-adapter.test.ts`: Electron bridge and browser fallback.
- Modify `src/renderer/store/workspace-store.ts` and `tests/workspace-store.test.ts`: per-room availability lifecycle.
- Create `src/renderer/components/RoomPlaybackSurface.tsx` and `tests/room-playback-surface.test.tsx`: checking, blocked, error, and demo surfaces.
- Create `src/renderer/runtime-mode.ts` and `tests/runtime-mode.test.ts`: pure Electron/demo startup split.
- Modify `src/renderer/components/RoomTile.tsx`, `src/renderer/components/AddRoomDialog.tsx`, `src/renderer/components/WorkspaceGrid.tsx`, `src/renderer/ui-model.ts`, and `src/renderer/styles.css`: blocked controls and truthful copy.
- Modify `src/renderer/main.tsx` and `tests/app-smoke.test.tsx`: empty Electron startup and retained browser demo.

### Task 1: Domain Contract and Browser Mock

**Files:**
- Modify: `src/domain/douyu-adapter.ts`
- Modify: `src/infrastructure/mock-douyu-adapter.ts`
- Modify: `tests/mock-douyu-adapter.test.ts`

- [x] **Step 1: Write failing tests for the Mock availability contract**

Add this behavior to `tests/mock-douyu-adapter.test.ts`:

```ts
it('provides explicit demo-only stream variants', async () => {
  const adapter = createMockDouyuAdapter();

  const availability = await adapter.getStreamAvailability('63136');

  expect(availability).toEqual(expect.objectContaining({
    kind: 'available',
    roomId: '63136',
  }));
  if (availability.kind !== 'available') throw new Error('expected available demo stream');
  expect(availability.variants.map((variant) => variant.quality)).toEqual([
    'auto', 'original', 'super', 'high', 'standard',
  ]);
  expect(availability.variants.every((variant) => variant.playbackUrl.startsWith('mock:'))).toBe(true);
});
```

- [x] **Step 2: Run the focused test and verify RED**

Run: `npm test -- tests/mock-douyu-adapter.test.ts`

Expected: FAIL because `getStreamAvailability` does not exist.

- [x] **Step 3: Add the domain types and required adapter method**

Append to `src/domain/douyu-adapter.ts`:

```ts
export interface ObservedStreamQuality {
  id: string;
  label: string;
  providerType: number;
}

export interface StreamVariant {
  id: string;
  label: string;
  quality: StreamQuality;
  playbackUrl: string;
  container: 'hls' | 'flv';
}

export type StreamBlockReason =
  | 'ROOM_OFFLINE'
  | 'NO_PUBLIC_SOURCE'
  | 'SIGNATURE_REQUIRED';

export type StreamAvailability =
  | {
      kind: 'available';
      roomId: string;
      variants: StreamVariant[];
      checkedAt: string;
    }
  | {
      kind: 'blocked';
      roomId: string;
      reason: StreamBlockReason;
      observedQualities: ObservedStreamQuality[];
      checkedAt: string;
    };

export interface DouyuAdapter {
  search(input: RoomInput): Promise<RoomCandidate[]>;
  getStreamAvailability(roomId: string): Promise<StreamAvailability>;
}
```

- [x] **Step 4: Add deterministic demo variants to the Mock adapter**

Inside `createMockDouyuAdapter`, implement:

```ts
async getStreamAvailability(roomId) {
  const qualities: Array<{ quality: StreamQuality; label: string }> = [
    { quality: 'auto', label: '自动' },
    { quality: 'original', label: '原画' },
    { quality: 'super', label: '超清' },
    { quality: 'high', label: '高清' },
    { quality: 'standard', label: '标清' },
  ];
  return {
    kind: 'available',
    roomId,
    checkedAt: new Date().toISOString(),
    variants: qualities.map(({ quality, label }) => ({
      id: `mock-${quality}`,
      label,
      quality,
      playbackUrl: `mock://${roomId}/${quality}`,
      container: 'hls' as const,
    })),
  };
}
```

- [x] **Step 5: Update inline test adapters to satisfy the required interface**

Where a test constructs `{ search: async ... }`, spread `createMockDouyuAdapter()` first and override only `search`:

```ts
const adapter: DouyuAdapter = {
  ...createMockDouyuAdapter(),
  search: async () => [],
};
```

- [x] **Step 6: Run the focused test and typecheck**

Run: `npm test -- tests/mock-douyu-adapter.test.ts`

Run: `npm run typecheck`

Expected: focused tests and typecheck pass.

### Task 2: Public Betard Availability Probe

**Files:**
- Create: `tests/fixtures/douyu-betard-api.json`
- Modify: `tests/douyu-http-adapter.test.ts`
- Modify: `src/infrastructure/douyu-http-adapter.ts`

- [x] **Step 1: Add a sanitized public response fixture**

Create `tests/fixtures/douyu-betard-api.json`:

```json
{
  "room": {
    "room_id": 63136,
    "show_status": 1,
    "multirates": [
      { "name": "蓝光10M", "type": 0 },
      { "name": "蓝光10M重复", "type": 0 },
      { "name": "超清", "type": 2 },
      { "name": null, "type": 3 }
    ]
  }
}
```

- [x] **Step 2: Write failing tests for live, offline, no-source, protocol, and endpoint safety behavior**

Add a `createDouyuHttpAdapter availability` describe block:

```ts
it('reports signed-only qualities without returning playback URLs', async () => {
  const fetchBetard = vi.fn(async (_url: string, _init?: RequestInit) =>
    Response.json(betardFixture),
  );
  const adapter = createDouyuHttpAdapter({
    fetch: fetchBetard,
    now: () => new Date('2026-08-07T00:00:00.000Z'),
  });

  await expect(adapter.getStreamAvailability('63136')).resolves.toEqual({
    kind: 'blocked',
    roomId: '63136',
    reason: 'SIGNATURE_REQUIRED',
    observedQualities: [
      { id: 'douyu-0', label: '蓝光10M', providerType: 0 },
      { id: 'douyu-2', label: '超清', providerType: 2 },
    ],
    checkedAt: '2026-08-07T00:00:00.000Z',
  });
  expect(fetchBetard.mock.calls[0][0]).toBe('https://www.douyu.com/betard/63136');
  expect(fetchBetard.mock.calls.flat().join(' ')).not.toContain('getH5Play');
});
```

Add separate tests with complete response shapes:

```ts
function adapterFor(payload: unknown): DouyuAdapter {
  return createDouyuHttpAdapter({
    fetch: async () => Response.json(payload),
    now: () => new Date('2026-08-07T00:00:00.000Z'),
  });
}

await expect(adapterFor({ room: { room_id: 63136, show_status: 2, multirates: [] } })
  .getStreamAvailability('63136')).resolves.toMatchObject({
    kind: 'blocked', reason: 'ROOM_OFFLINE', observedQualities: [],
  });

await expect(adapterFor({ room: { room_id: 63136, show_status: 1, multirates: [] } })
  .getStreamAvailability('63136')).resolves.toMatchObject({
    kind: 'blocked', reason: 'NO_PUBLIC_SOURCE', observedQualities: [],
  });

await expect(adapterFor({ room: { show_status: 1 } })
  .getStreamAvailability('63136')).rejects.toMatchObject({ code: 'PROTOCOL_CHANGED' });
```

- [x] **Step 3: Run the focused test and verify RED**

Run: `npm test -- tests/douyu-http-adapter.test.ts`

Expected: existing room/search tests pass; availability tests fail because the method and `now` option are missing.

- [x] **Step 4: Implement the public probe and quality parser**

Add to `DouyuHttpAdapterOptions`:

```ts
now?: () => Date;
```

Add helpers:

```ts
const BETARD_API_BASE_URL = 'https://www.douyu.com/betard/';

function observedQualities(value: unknown): ObservedStreamQuality[] {
  if (!Array.isArray(value)) return [];
  const qualities = new Map<number, ObservedStreamQuality>();
  for (const item of value) {
    if (!isRecord(item) || typeof item.name !== 'string' || typeof item.type !== 'number') continue;
    if (!item.name.trim() || !Number.isInteger(item.type) || qualities.has(item.type)) continue;
    qualities.set(item.type, {
      id: `douyu-${item.type}`,
      label: item.name.trim(),
      providerType: item.type,
    });
  }
  return [...qualities.values()];
}
```

Implement on the returned adapter:

```ts
async getStreamAvailability(roomId) {
  const payload = await requestJson(
    `${BETARD_API_BASE_URL}${encodeURIComponent(roomId)}`,
    fetchImpl,
    timeoutMs,
  );
  if (!isRecord(payload) || !isRecord(payload.room)) {
    throw new DouyuAdapterError('PROTOCOL_CHANGED');
  }
  const mappedRoomId = scalarString(payload.room.room_id);
  const showStatus = scalarString(payload.room.show_status);
  if (!mappedRoomId || !showStatus || !Array.isArray(payload.room.multirates)) {
    throw new DouyuAdapterError('PROTOCOL_CHANGED');
  }
  const qualities = observedQualities(payload.room.multirates);
  return {
    kind: 'blocked',
    roomId: mappedRoomId,
    reason: showStatus !== '1'
      ? 'ROOM_OFFLINE'
      : qualities.length
        ? 'SIGNATURE_REQUIRED'
        : 'NO_PUBLIC_SOURCE',
    observedQualities: qualities,
    checkedAt: now().toISOString(),
  };
}
```

- [x] **Step 5: Run focused tests and typecheck**

Run: `npm test -- tests/douyu-http-adapter.test.ts tests/mock-douyu-adapter.test.ts`

Run: `npm run typecheck`

Expected: adapter tests and typecheck pass; no test URL contains `getH5Play`.

### Task 3: IPC, Main Handler, and Preload Allowlist

**Files:**
- Modify: `src/shared/ipc-contract.ts`
- Modify: `src/main/ipc-handlers.ts`
- Modify: `src/preload/bridge.ts`
- Modify: `tests/ipc-contract.test.ts`
- Modify: `tests/ipc-handlers.test.ts`
- Modify: `tests/preload-bridge.test.ts`

- [x] **Step 1: Write failing contract tests**

Add assertions:

```ts
expect(IPC_CHANNELS.getStreamAvailability).toBe('playback.getAvailability');
expect(isValidGetStreamAvailabilityRequest({ roomId: '63136' })).toBe(true);
expect(isValidGetStreamAvailabilityRequest({ roomId: '' })).toBe(false);
expect(isValidGetStreamAvailabilityRequest({ roomId: 'abc' })).toBe(false);
expect(isValidGetStreamAvailabilityRequest({ roomId: '1'.repeat(21) })).toBe(false);
```

- [x] **Step 2: Write failing handler and preload tests**

Handler test:

```ts
const result = await handlers.get(IPC_CHANNELS.getStreamAvailability)?.(
  {}, { roomId: '63136' },
);
expect(result).toEqual(expect.objectContaining({
  ok: true,
  data: expect.objectContaining({ kind: 'available', roomId: '63136' }),
}));
```

Preload test:

```ts
await api.getStreamAvailability('63136');
expect(calls).toContainEqual({
  channel: IPC_CHANNELS.getStreamAvailability,
  payload: { roomId: '63136' },
});
expect(Object.keys(api)).toEqual(['searchRooms', 'getStreamAvailability', 'ping']);
expect('invoke' in api).toBe(false);
```

- [x] **Step 3: Run focused tests and verify RED**

Run: `npm test -- tests/ipc-contract.test.ts tests/ipc-handlers.test.ts tests/preload-bridge.test.ts`

Expected: FAIL because channel, validator, handler, and bridge method are missing.

- [x] **Step 4: Add the shared contract**

Add:

```ts
getStreamAvailability: 'playback.getAvailability',

export interface GetStreamAvailabilityRequest { roomId: string; }
export type GetStreamAvailabilityResult = IpcResult<StreamAvailability>;

export function isValidGetStreamAvailabilityRequest(
  value: unknown,
): value is GetStreamAvailabilityRequest {
  if (!value || typeof value !== 'object' || !('roomId' in value)) return false;
  const roomId = (value as { roomId?: unknown }).roomId;
  return typeof roomId === 'string' && /^\d{1,20}$/.test(roomId);
}
```

Add `invalidRoomIdError()` with code `INVALID_INPUT`, message `请输入有效的直播间号`, and `retryable: false`.

- [x] **Step 5: Register the main handler**

```ts
ipcMain.handle(
  IPC_CHANNELS.getStreamAvailability,
  async (_event, request): Promise<GetStreamAvailabilityResult> => {
    if (!isValidGetStreamAvailabilityRequest(request)) return invalidRoomIdError();
    try {
      return ok(await adapter.getStreamAvailability(request.roomId));
    } catch (error) {
      return failed(error);
    }
  },
);
```

- [x] **Step 6: Extend only the preload allowlist**

Add to `AppApi` and `createAppApi`:

```ts
getStreamAvailability(roomId: string): Promise<GetStreamAvailabilityResult>;

getStreamAvailability(roomId) {
  return ipcRenderer.invoke(
    IPC_CHANNELS.getStreamAvailability,
    { roomId },
  ) as Promise<GetStreamAvailabilityResult>;
},
```

- [x] **Step 7: Run focused and full contract tests**

Run: `npm test -- tests/ipc-contract.test.ts tests/ipc-handlers.test.ts tests/preload-bridge.test.ts`

Run: `npm run typecheck`

Expected: all focused tests and typecheck pass; AppApi exposes exactly three methods.

### Task 4: Renderer Adapter Bridge

**Files:**
- Modify: `src/infrastructure/renderer-douyu-adapter.ts`
- Modify: `tests/renderer-douyu-adapter.test.ts`

- [x] **Step 1: Write failing Electron success and error tests**

```ts
const availability = {
  kind: 'blocked' as const,
  roomId: '63136',
  reason: 'SIGNATURE_REQUIRED' as const,
  observedQualities: [],
  checkedAt: '2026-08-07T00:00:00.000Z',
};
const getStreamAvailability = vi.fn(async () => ok(availability));
installAppApi({ searchRooms, getStreamAvailability, ping: vi.fn() });

await expect(adapter.getStreamAvailability('63136')).resolves.toEqual(availability);
expect(getStreamAvailability).toHaveBeenCalledWith('63136');
```

Add an IPC error test that expects the fixed user message to be thrown. Update all existing `AppApi` fixtures with `getStreamAvailability`.

- [x] **Step 2: Run focused tests and verify RED**

Run: `npm test -- tests/renderer-douyu-adapter.test.ts`

Expected: FAIL because the renderer adapter has no availability method.

- [x] **Step 3: Forward the typed preload method**

Add to the Electron adapter object:

```ts
async getStreamAvailability(roomId) {
  const result = await appApi.getStreamAvailability(roomId);
  if (!result.ok) throw new Error(result.error.message);
  return result.data;
},
```

Do not catch this error and do not fall back to the Mock adapter after Electron bridge detection.

- [x] **Step 4: Run focused tests and typecheck**

Run: `npm test -- tests/renderer-douyu-adapter.test.ts`

Run: `npm run typecheck`

Expected: focused tests and typecheck pass.

### Task 5: Per-Room Store Lifecycle

**Files:**
- Modify: `src/renderer/store/workspace-store.ts`
- Modify: `tests/workspace-store.test.ts`

- [x] **Step 1: Write a failing checking-to-blocked test**

Use a deferred promise so the intermediate state is observable:

```ts
function blockedAvailability(roomId: string): StreamAvailability {
  return {
    kind: 'blocked',
    roomId,
    reason: 'SIGNATURE_REQUIRED',
    observedQualities: [
      { id: 'douyu-0', label: '蓝光10M', providerType: 0 },
    ],
    checkedAt: '2026-08-07T00:00:00.000Z',
  };
}

let resolveAvailability!: (value: StreamAvailability) => void;
const availabilityPromise = new Promise<StreamAvailability>((resolve) => {
  resolveAvailability = resolve;
});
const adapter: DouyuAdapter = {
  ...createMockDouyuAdapter(),
  getStreamAvailability: async () => availabilityPromise,
};
const store = createWorkspaceStore(adapter);

expect(store.getState().addRoom(candidate('63136'))).toBe('added');
expect(store.getState().rooms[0].playbackAvailabilityStatus).toBe('checking');

resolveAvailability(blockedAvailability('63136'));
await availabilityPromise;
await Promise.resolve();
expect(store.getState().rooms[0]).toEqual(expect.objectContaining({
  playbackAvailabilityStatus: 'blocked',
  streamAvailability: expect.objectContaining({ kind: 'blocked' }),
}));
```

- [x] **Step 2: Write failing isolation, stale-result, and error tests**

Add separate tests proving:

- a rejected availability promise sets only the matching room to `error` with a user-facing message;
- removing a room before its promise resolves does not recreate it;
- refreshing one room does not replace another room object;
- `refreshStreamAvailability(roomId)` ignores unknown room IDs.

- [x] **Step 3: Run focused tests and verify RED**

Run: `npm test -- tests/workspace-store.test.ts`

Expected: existing store tests pass; new tests fail because availability fields and refresh action are missing.

- [x] **Step 4: Add room availability state and refresh action**

Add:

```ts
export type PlaybackAvailabilityStatus = 'checking' | 'available' | 'blocked' | 'error';

export interface RoomSession extends RoomCandidate {
  status: RoomStatus;
  quality: StreamQuality;
  danmakuEnabled: boolean;
  playbackAvailabilityStatus: PlaybackAvailabilityStatus;
  streamAvailability?: StreamAvailability;
  playbackError?: string;
}
```

`toSession` sets `playbackAvailabilityStatus: 'checking'`. Add to `WorkspaceState`:

```ts
refreshStreamAvailability: (roomId: string) => Promise<void>;
```

Implement:

```ts
async refreshStreamAvailability(roomId) {
  if (!get().rooms.some((room) => room.roomId === roomId)) return;
  set((state) => ({
    rooms: state.rooms.map((room) => room.roomId === roomId
      ? { ...room, playbackAvailabilityStatus: 'checking', playbackError: undefined }
      : room),
  }));
  try {
    const availability = await adapter.getStreamAvailability(roomId);
    if (!get().rooms.some((room) => room.roomId === roomId)) return;
    set((state) => ({
      rooms: state.rooms.map((room) => room.roomId === roomId
        ? {
            ...room,
            playbackAvailabilityStatus: availability.kind,
            streamAvailability: availability,
            playbackError: undefined,
          }
        : room),
    }));
  } catch (error) {
    if (!get().rooms.some((room) => room.roomId === roomId)) return;
    set((state) => ({
      rooms: state.rooms.map((room) => room.roomId === roomId
        ? {
            ...room,
            playbackAvailabilityStatus: 'error',
            playbackError: error instanceof Error ? error.message : '播放能力检查失败',
          }
        : room),
    }));
  }
}
```

After `addRoom` updates state, call `void get().refreshStreamAvailability(candidate.roomId)`.

- [x] **Step 5: Refresh initial rooms after store construction**

Change `createWorkspaceStore` from an immediate `return createStore(...)` to:

```ts
const store = createStore<WorkspaceState>(/* existing initializer */);
for (const room of initialSessions) {
  void store.getState().refreshStreamAvailability(room.roomId);
}
return store;
```

- [x] **Step 6: Run focused tests and typecheck**

Run: `npm test -- tests/workspace-store.test.ts`

Run: `npm run typecheck`

Expected: store tests and typecheck pass; no stale result recreates a removed room.

### Task 6: Truthful Electron UI and Retained Browser Demo

**Files:**
- Create: `src/renderer/components/RoomPlaybackSurface.tsx`
- Create: `tests/room-playback-surface.test.tsx`
- Modify: `src/renderer/components/RoomTile.tsx`
- Modify: `src/renderer/components/AddRoomDialog.tsx`
- Modify: `src/renderer/components/WorkspaceGrid.tsx`
- Modify: `src/renderer/ui-model.ts`
- Modify: `tests/ui-model.test.ts`
- Modify: `src/renderer/store/workspace-store.ts`
- Modify: `src/renderer/store/workspace-context.tsx`
- Create: `src/renderer/runtime-mode.ts`
- Modify: `src/renderer/main.tsx`
- Modify: `src/renderer/styles.css`
- Modify: `tests/app-smoke.test.tsx`
- Create: `tests/runtime-mode.test.ts`

- [x] **Step 1: Write failing playback presentation tests**

Add to `tests/ui-model.test.ts`:

```ts
const baseRoom: RoomSession = {
  roomId: '63136',
  anchorName: '示例主播',
  title: '示例直播间',
  category: 'CS2',
  online: true,
  viewerLabel: '1 万',
  status: 'playing',
  quality: 'auto',
  danmakuEnabled: true,
  playbackAvailabilityStatus: 'checking',
};
const checkingRoom = baseRoom;
const signatureBlockedRoom: RoomSession = {
  ...baseRoom,
  playbackAvailabilityStatus: 'blocked',
  streamAvailability: {
    kind: 'blocked',
    roomId: '63136',
    reason: 'SIGNATURE_REQUIRED',
    observedQualities: [
      { id: 'douyu-0', label: '蓝光10M', providerType: 0 },
    ],
    checkedAt: '2026-08-07T00:00:00.000Z',
  },
};
const errorRoom: RoomSession = {
  ...baseRoom,
  playbackAvailabilityStatus: 'error',
  playbackError: '无法连接斗鱼，请检查网络后重试',
};
const demoRoom: RoomSession = {
  ...baseRoom,
  playbackAvailabilityStatus: 'available',
  streamAvailability: {
    kind: 'available',
    roomId: '63136',
    variants: [{
      id: 'mock-auto',
      label: '自动',
      quality: 'auto',
      playbackUrl: 'mock://63136/auto',
      container: 'hls',
    }],
    checkedAt: '2026-08-07T00:00:00.000Z',
  },
};

expect(getPlaybackPresentation(checkingRoom)).toEqual(expect.objectContaining({
  title: '正在检查播放源',
  qualityDisabled: true,
  audioDisabled: true,
}));
expect(getPlaybackPresentation(signatureBlockedRoom)).toEqual(expect.objectContaining({
  title: '暂无合规播放源',
  detail: '斗鱼当前只提供需签名的播放接口',
  qualityLabels: ['蓝光10M'],
}));
expect(getPlaybackPresentation(errorRoom)).toEqual(expect.objectContaining({
  title: '播放能力检查失败',
  canRetry: true,
}));
```

- [x] **Step 2: Write failing static surface tests**

Create `tests/room-playback-surface.test.tsx` and render the component directly:

```ts
const blockedHtml = renderToStaticMarkup(
  <RoomPlaybackSurface room={signatureBlockedRoom} demoMode={false} onRetry={() => {}} />,
);
expect(blockedHtml).toContain('暂无合规播放源');
expect(blockedHtml).toContain('斗鱼当前只提供需签名的播放接口');
expect(blockedHtml).not.toContain('模拟画面');

const demoHtml = renderToStaticMarkup(
  <RoomPlaybackSurface room={demoRoom} demoMode onRetry={() => {}} />,
);
expect(demoHtml).toContain('模拟画面');
```

- [x] **Step 3: Write a failing runtime seed test**

Create `tests/runtime-mode.test.ts` for the pure helper in `src/renderer/runtime-mode.ts`:

```ts
expect(getInitialRoomsForRuntime(true)).toEqual([]);
expect(getInitialRoomsForRuntime(false)).toEqual(MOCK_ROOM_CANDIDATES.slice(0, 3));
```

- [x] **Step 4: Run focused tests and verify RED**

Run: `npm test -- tests/ui-model.test.ts tests/room-playback-surface.test.tsx tests/app-smoke.test.tsx`

Expected: FAIL because presentation helper, surface component, and runtime seed helper are missing.

- [x] **Step 5: Implement the presentation helper**

Add `getPlaybackPresentation(room)` to `ui-model.ts`. It must return fixed titles/details for all four statuses, derive blocked quality labels from `observedQualities`, and set:

```ts
{
  qualityDisabled: room.playbackAvailabilityStatus !== 'available',
  audioDisabled: room.playbackAvailabilityStatus !== 'available',
  canRetry: room.playbackAvailabilityStatus === 'error',
}
```

- [x] **Step 6: Implement `RoomPlaybackSurface` and update `RoomTile`**

`RoomPlaybackSurface` receives `room`, `demoMode`, and `onRetry`. It renders the existing `.signal-scene` only when `demoMode` and availability is `available`; all other statuses render a stable `.playback-state-surface` with Lucide `LoaderCircle`, `CircleSlash2`, or `AlertCircle` icons and a retry button for errors.

In `RoomTile`:

- call `refreshStreamAvailability(room.roomId)` from the retry button;
- disable the audio button when presentation says `audioDisabled`;
- render observed blocked quality labels in a disabled select;
- retain existing `QUALITY_OPTIONS` only for browser demo availability;
- never render a real `<video>` element in this milestone.

- [x] **Step 7: Add runtime mode to the provider and truthful copy**

Add `demoMode: boolean` to `WorkspaceOptions` and `WorkspaceState`, pass it through `WorkspaceProvider`, and use it in `AddRoomDialog`, `WorkspaceGrid`, and `RoomTile`.

Use these production strings:

```text
房间信息来自斗鱼公开接口，播放源按合规规则检查。
布局切换只移动房间位置，不会重新检查播放能力。
```

Keep the existing mock-stage strings only when `demoMode` is true.

- [x] **Step 8: Make Electron startup empty and browser startup seeded**

Create `src/renderer/runtime-mode.ts` with:

```ts
export function getInitialRoomsForRuntime(electronMode: boolean): RoomCandidate[] {
  return electronMode ? [] : MOCK_ROOM_CANDIDATES.slice(0, 3);
}
```

In `main.tsx`, detect Electron from `typeof window !== 'undefined' && Boolean(window.appApi)`, then pass `demoMode={!electronMode}` and `initialRooms={getInitialRoomsForRuntime(electronMode)}`.

- [x] **Step 9: Add restrained UI styles**

Add fixed-size styles for `.playback-state-surface`, `.playback-state-icon`, `.playback-state-copy`, and its retry button. Preserve the room tile dimensions, use existing neutral/coral tokens, and avoid layout shifts between checking, blocked, and error states.

- [x] **Step 10: Run focused tests and typecheck**

Run: `npm test -- tests/ui-model.test.ts tests/room-playback-surface.test.tsx tests/app-smoke.test.tsx tests/workspace-store.test.ts`

Run: `npm run typecheck`

Expected: focused tests pass; browser smoke still contains seeded anchors and “模拟画面”.

### Task 7: Full Verification, Electron Evidence, and Notion

**Files:**
- Update: `docs/superpowers/plans/2026-08-07-douyu-stream-availability.md` checkboxes
- Update: Notion project development design page after evidence exists

- [x] **Step 1: Run complete automated verification**

Run: `npm test`

Run: `npm run typecheck`

Run: `npm run build`

Run: `node --check dist/main/main.js`

Run: `node --check dist/preload/preload.cjs`

Expected: every command exits 0 with no failed tests.

- [x] **Step 2: Inspect the production bundle safety boundary**

Run:

```powershell
rg -n "betard|getH5Play|homeH5Enc|mock://" dist/main/main.js
```

Expected: `betard` is present; `getH5Play`, `homeH5Enc`, and `mock://` are absent from the production main bundle.

- [x] **Step 3: Validate the browser demo with Playwright**

Start or reuse the Vite server. At desktop and 390×844 viewports verify:

- three demo rooms appear;
- “模拟画面” remains visible;
- add-room, layout, sidebar, quality, and danmaku controls still work;
- console has no application error or warning;
- text and controls do not overlap.

- [x] **Step 4: Validate the rebuilt Electron app**

Restart Electron after `npm run build`. Verify:

- startup workspace is empty;
- searching and adding room `63136` shows current real metadata;
- the room transitions from “正在检查播放源” to “暂无合规播放源”;
- the detail says the endpoint requires signing;
- the disabled quality menu shows the current public `multirates` label;
- no “模拟画面” appears;
- audio is disabled and retry works after a simulated network failure.

In DevTools run:

```js
Object.keys(window.appApi)
await window.appApi.getStreamAvailability('63136')
```

Expected keys: `searchRooms`, `getStreamAvailability`, `ping`. Expected result: `ok: true`, `data.kind: 'blocked'`, no `playbackUrl` field.

- [x] **Step 5: Update Notion and re-fetch the page**

Append a milestone section containing implementation files, test/build counts, browser and Electron evidence, the observed quality label, and the explicit boundary that video playback remains blocked by the lack of a compliant public direct URL. Re-fetch and assert the section, evidence, and boundary are present.

## Self-Review

- Spec coverage: tasks cover domain models, public probe, IPC, preload, renderer adapter, store isolation, truthful UI, Electron/demo startup split, safety checks, E2E, and Notion.
- Placeholder scan: every change step includes exact files, behavior, code shape, commands, and expected evidence.
- Type consistency: `StreamAvailability`, `getStreamAvailability`, `playback.getAvailability`, `playbackAvailabilityStatus`, and `demoMode` use the same names across all tasks.
- Scope: no task implements a player, signed API, page script execution, URL guessing, or embedded Douyu content.
