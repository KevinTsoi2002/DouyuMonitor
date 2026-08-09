# Douyu Real Room Search Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the Electron main-process mock room search with a real, read-only Douyu room metadata adapter for room IDs and anchor names.

**Architecture:** Keep the existing `DouyuAdapter -> IPC -> preload -> renderer` contract. Add a fetch-injected HTTP adapter in `src/infrastructure`, parse only the fields required by `RoomCandidate`, classify upstream failures without leaking response details, and keep the mock only for browser-only Vite development. The adapter must not require login, cookies, signatures, or access-control workarounds.

**Tech Stack:** TypeScript, Node 24 built-in `fetch`, Electron 43, Vitest, existing Vite main bundle.

---

## Validated Boundary

- On 2026-08-07, `GET https://open.douyucdn.cn/api/RoomApi/room/63136` returned HTTP 200 with room metadata and no authentication.
- On 2026-08-07, `GET https://www.douyu.com/japi/search/api/searchShow?kw=<query>&page=1&pageSize=20` returned HTTP 200 with anchor search results and no authentication.
- This milestone covers room metadata only. It does not claim stream URL, quality, signature, playback, or danmaku support.
- The workspace has no `.git` directory. Do not initialize Git or add commit steps without user authorization.

## File Structure

- Create `src/infrastructure/douyu-http-adapter.ts`: endpoint construction, HTTP request boundary, response parsing, deduplication, and `RoomCandidate` mapping.
- Create `tests/douyu-http-adapter.test.ts`: contract tests against sanitized response fixtures and fake fetch functions.
- Create `tests/fixtures/douyu-room-api.json`: minimal sanitized room response matching the validated public endpoint shape.
- Create `tests/fixtures/douyu-search-api.json`: minimal sanitized search response with duplicate and malformed entries for parser coverage.
- Modify `src/domain/douyu-adapter.ts`: stable adapter error codes and typed error class.
- Modify `src/shared/ipc-contract.ts`: map adapter error codes to sanitized IPC errors.
- Modify `tests/ipc-contract.test.ts`: prove error mapping and secret redaction.
- Modify `src/main/main.ts`: inject the production HTTP adapter instead of the mock adapter.

### Task 1: Adapter Error Contract

**Files:**
- Modify: `src/domain/douyu-adapter.ts`
- Modify: `src/shared/ipc-contract.ts`
- Modify: `tests/ipc-contract.test.ts`

- [x] **Step 1: Write failing IPC mapping tests**

```ts
it.each([
  ['ROOM_NOT_FOUND', '未找到对应直播间', false],
  ['NETWORK_UNAVAILABLE', '无法连接斗鱼，请检查网络后重试', true],
  ['PROTOCOL_CHANGED', '斗鱼接口响应异常，请稍后重试', true],
] as const)('maps %s without exposing details', (code, message, retryable) => {
  const error = new DouyuAdapterError(code, 'response contained token=secret');
  expect(toIpcError(error)).toEqual({ code, message, retryable });
  expect(JSON.stringify(toIpcError(error))).not.toContain('secret');
});
```

- [x] **Step 2: Run the focused test and verify RED**

Run: `npm test -- tests/ipc-contract.test.ts`

Expected: FAIL because `DouyuAdapterError` and the new IPC codes do not exist.

- [x] **Step 3: Add the minimal domain error type**

```ts
export type DouyuAdapterErrorCode =
  | 'ROOM_NOT_FOUND'
  | 'NETWORK_UNAVAILABLE'
  | 'PROTOCOL_CHANGED';

export class DouyuAdapterError extends Error {
  constructor(public readonly code: DouyuAdapterErrorCode, message?: string) {
    super(message ?? code);
    this.name = 'DouyuAdapterError';
  }
}
```

- [x] **Step 4: Extend `IpcError` and `toIpcError` with fixed messages**

Map only the typed code and discard `error.message`. Unknown errors must keep the existing generic message.

- [x] **Step 5: Run the focused test and verify GREEN**

Run: `npm test -- tests/ipc-contract.test.ts`

Expected: all tests in the file pass with no warnings.

### Task 2: Room-ID HTTP Query

**Files:**
- Create: `tests/fixtures/douyu-room-api.json`
- Create: `tests/douyu-http-adapter.test.ts`
- Create: `src/infrastructure/douyu-http-adapter.ts`

- [x] **Step 1: Add a sanitized room fixture**

```json
{
  "error": 0,
  "data": {
    "room_id": "63136",
    "room_name": "示例直播间",
    "owner_name": "示例主播",
    "cate_name": "CS2",
    "room_status": "1",
    "online": 186000
  }
}
```

- [x] **Step 2: Write failing tests for room mapping, not-found, malformed JSON, HTTP failure, and fetch rejection**

```ts
const fetchRoom = vi.fn(async () => Response.json(roomFixture));
const adapter = createDouyuHttpAdapter({ fetch: fetchRoom });

await expect(adapter.search({ type: 'room-id', value: '63136' })).resolves.toEqual([
  {
    roomId: '63136',
    anchorName: '示例主播',
    title: '示例直播间',
    category: 'CS2',
    online: true,
    viewerLabel: '18.6 万',
  },
]);
expect(fetchRoom).toHaveBeenCalledWith(
  'https://open.douyucdn.cn/api/RoomApi/room/63136',
  expect.objectContaining({ headers: { Accept: 'application/json' } }),
);
```

Additional assertions:

- `error !== 0` rejects with `ROOM_NOT_FOUND`.
- missing `data.room_id`, `room_name`, or `owner_name` rejects with `PROTOCOL_CHANGED`.
- non-2xx responses and rejected fetch calls reject with `NETWORK_UNAVAILABLE`.
- the upstream response body and network error text are not included in the typed error message.

- [x] **Step 3: Run the focused test and verify RED**

Run: `npm test -- tests/douyu-http-adapter.test.ts`

Expected: FAIL because `createDouyuHttpAdapter` is missing.

- [x] **Step 4: Implement the minimal request and room parser**

```ts
export interface DouyuHttpAdapterOptions {
  fetch?: (url: string, init?: RequestInit) => Promise<Response>;
}

export function createDouyuHttpAdapter(
  options: DouyuHttpAdapterOptions = {},
): DouyuAdapter {
  const fetchImpl = options.fetch ?? globalThis.fetch;
  return {
    async search(input) {
      if (input.type === 'room-id') return [await fetchRoom(input.value, fetchImpl)];
      return searchAnchors(input.value, fetchImpl);
    },
  };
}
```

`fetchRoom` must URL-encode the ID, require a successful HTTP response, parse JSON inside a guarded block, validate the required scalar fields, derive `online` from `room_status === '1'`, and format heat values below 10,000 as integers or values at/above 10,000 as one-decimal `万` labels with a trailing `.0` removed.

- [x] **Step 5: Run focused tests and verify GREEN**

Run: `npm test -- tests/douyu-http-adapter.test.ts`

Expected: room-ID cases pass with no real network calls.

### Task 3: Anchor-Name Search

**Files:**
- Create: `tests/fixtures/douyu-search-api.json`
- Modify: `tests/douyu-http-adapter.test.ts`
- Modify: `src/infrastructure/douyu-http-adapter.ts`

- [x] **Step 1: Add a sanitized search fixture**

```json
{
  "error": 0,
  "data": {
    "relateShow": [
      {
        "rid": 6846643,
        "nickName": "示例主播",
        "roomName": "示例直播间",
        "cateName": "经典单机",
        "isLive": 1,
        "hot": "186000"
      },
      {
        "rid": 6846643,
        "nickName": "重复结果",
        "roomName": "重复直播间",
        "cateName": "经典单机",
        "isLive": 1,
        "hot": "1"
      },
      { "rid": null, "nickName": "无效结果" }
    ]
  }
}
```

- [x] **Step 2: Write failing tests for URL encoding, mapping, deduplication, empty results, and malformed payloads**

```ts
const searchFetch = vi.fn(async () => Response.json(searchFixture));
const adapter = createDouyuHttpAdapter({ fetch: searchFetch });
const result = await adapter.search({ type: 'anchor-name', value: '示例 主播' });

expect(new URL(searchFetch.mock.calls[0][0]).searchParams.get('kw')).toBe('示例 主播');
expect(result).toHaveLength(1);
expect(result[0]).toEqual(expect.objectContaining({
  roomId: '6846643',
  anchorName: '示例主播',
  online: true,
  viewerLabel: '18.6 万',
}));
```

- [x] **Step 3: Run the focused test and verify RED**

Run: `npm test -- tests/douyu-http-adapter.test.ts`

Expected: room-ID cases stay green and anchor search cases fail because `searchAnchors` is not implemented.

- [x] **Step 4: Implement minimal search parsing**

Build the endpoint with `URL` and `searchParams`; set `page=1` and `pageSize=20`. Require `data.relateShow` to be an array, ignore entries without a scalar `rid`, `nickName`, or `roomName`, map the six `RoomCandidate` fields, and keep the first candidate for each `roomId`.

- [x] **Step 5: Run focused tests and verify GREEN**

Run: `npm test -- tests/douyu-http-adapter.test.ts`

Expected: all adapter tests pass without live network access.

### Task 4: Main-Process Production Wiring

**Files:**
- Modify: `src/main/main.ts`
- Modify: `tests/main-config.test.ts` only if an injectable bootstrap helper is needed; do not expand window configuration scope.

- [x] **Step 1: Add a source-level wiring assertion before changing main**

Add a focused test that reads `src/main/main.ts` and asserts it imports and registers `createDouyuHttpAdapter`, while rejecting `createMockDouyuAdapter` in the Electron entry. This is a wiring guard, not a behavior substitute for adapter tests.

- [x] **Step 2: Run the wiring test and verify RED**

Run: `npm test -- tests/main-config.test.ts`

Expected: FAIL because main still injects the mock adapter.

- [x] **Step 3: Replace only the main-process injection**

```ts
import { createDouyuHttpAdapter } from '../infrastructure/douyu-http-adapter';

// Inside bootstrap after app.whenReady():
registerIpcHandlers(ipcMain, createDouyuHttpAdapter());
```

Do not change `renderer-douyu-adapter.ts`; browser-only Vite and SSR tests must continue to fall back to the mock.

- [x] **Step 4: Run the wiring test, adapter test, and full suite**

Run: `npm test -- tests/main-config.test.ts tests/douyu-http-adapter.test.ts`

Run: `npm test`

Expected: all tests pass and the existing browser fallback tests remain green.

### Task 5: Verification and Progress Update

**Files:**
- Modify: the existing Notion project development design page after runtime evidence is collected.

- [x] **Step 1: Run static verification**

Run: `npm run typecheck`

Run: `npm run build`

Run: `node --check dist/main/main.js`

Run: `node --check dist/preload/preload.cjs`

Expected: every command exits 0.

- [x] **Step 2: Run a live endpoint smoke without persisting the response**

Use Node built-in fetch to request room `63136` and an encoded anchor query. Print only HTTP status, upstream error code, room ID, and result count; never print headers, cookies, full response bodies, or playback URLs.

- [x] **Step 3: Rebuild and restart Electron in the interactive desktop session**

In DevTools, run:

```js
await window.appApi.searchRooms('63136')
await window.appApi.searchRooms('星河')
```

Acceptance criteria:

- room `63136` returns current upstream metadata rather than the old mock title;
- anchor search returns upstream candidates or a sanitized typed error;
- no generic `invoke` appears on `window.appApi`;
- a network or upstream failure never falls back to fabricated room data.

- [x] **Step 4: Update Notion**

Record test/build counts, live smoke evidence, the exact endpoint validation date, the remaining instability risk, and the explicit boundary that playback, quality, and danmaku are still unimplemented.

## Self-Review

- Spec coverage: room-ID and anchor-name metadata search are covered; playback, quality, and danmaku are explicitly outside this milestone.
- Placeholder scan: implementation behavior, files, commands, failure codes, and acceptance evidence are fully specified.
- Type consistency: `DouyuAdapterErrorCode`, IPC codes, fixture fields, endpoint parsers, and `RoomCandidate` fields use the same names across tasks.
