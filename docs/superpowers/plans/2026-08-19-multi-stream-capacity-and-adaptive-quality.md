# Multi-Stream Capacity and Adaptive Quality Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make eight live Douyu rooms play continuously by routing each room through its own loopback origin and reducing non-primary rooms to 720p when more than four live rooms are active.

**Architecture:** The main process resolves signed FLV URLs through a two-worker StreamGet queue, registers each URL with a per-room loopback proxy, and returns only the local URL to the renderer. The workspace keeps persisted user quality separate from a non-persisted effective quality, refreshes only rooms whose effective quality changes, and ignores stale resolver results.

**Tech Stack:** Electron 43, TypeScript 7, React 19, Zustand, Node.js `http`/`https`, StreamGet 4.0.10, Python 3.14, mpegts.js 1.8, Vitest 4, Playwright 1.62

---

## File Structure

New production files:

- `src/main/douyu-stream-url.ts`: validate Douyu CDN FLV URLs for both the bridge and proxy redirect path.
- `src/main/streamget-resolution-queue.ts`: limit sidecar concurrency, coalesce identical requests, and cancel queued room work.
- `src/main/stream-proxy-manager.ts`: own one loopback server, token, port, upstream URL, and Node agents per room.
- `src/renderer/store/stream-quality-policy.ts`: calculate effective quality and changed room IDs without touching persisted preferences.

New tests:

- `tests/streamget-resolution-queue.test.ts`
- `tests/stream-proxy-manager.test.ts`
- `tests/stream-quality-policy.test.ts`
- `tests/room-tile-quality.test.tsx`
- `tests/performance-baseline-script.test.ts`

Modified boundaries:

- `src/domain/douyu-adapter.ts`: add request-quality and local-proxy error types; allow playback release through the adapter boundary.
- `src/shared/ipc-contract.ts`, `src/main/ipc-handlers.ts`, `src/preload/bridge.ts`, `src/infrastructure/renderer-douyu-adapter.ts`: carry effective quality and proxy release across the approved IPC surface.
- `src/main/streamget-bridge.ts`, `scripts/streamget_bridge.py`: request a StreamGet H5 quality, retain App-search fallback, validate output, and use a 30-second timeout.
- `src/infrastructure/streamget-douyu-adapter.ts`: replace upstream URLs with loopback URLs and expose the resolved quality label.
- `src/renderer/store/workspace-store.ts`: apply adaptive quality transitions, release removed rooms, and reject stale resolution results.
- `src/renderer/components/RoomTile.tsx`: show the effective quality returned by the active playback source while preserving the user's stored selection.
- `src/main/main.ts`: assemble and dispose the resolver queue and proxy manager.
- `scripts/performance-baseline.mjs`: verify continued frame progression and decoded resolution for at least eight rooms.

Do not modify `src/renderer/components/FlvVideo.tsx` worker settings. `enableWorker` and `enableWorkerForMSE` must remain `false` because Blob workers fail in the packaged `file://` renderer.

### Task 1: Define playback quality and lifecycle contracts

**Files:**
- Modify: `src/domain/douyu-adapter.ts`
- Modify: `src/shared/ipc-contract.ts`
- Test: `tests/ipc-contract.test.ts`
- Test: `tests/streamget-ipc-error.test.ts`

- [ ] **Step 1: Write failing contract tests**

Add assertions that availability requests require a supported quality, proxy release uses a separate namespaced channel, and proxy bind failures map to a safe retryable IPC error:

```ts
expect(isValidGetStreamAvailabilityRequest({ roomId: '63136', quality: '720p' })).toBe(true);
expect(isValidGetStreamAvailabilityRequest({ roomId: '63136', quality: 'original' })).toBe(true);
expect(isValidGetStreamAvailabilityRequest({ roomId: '63136', quality: '4k' })).toBe(false);
expect(isValidGetStreamAvailabilityRequest({ roomId: '63136' })).toBe(false);
expect(IPC_CHANNELS.releaseStreamProxy).toBe('playback.releaseProxy');

expect(toIpcError(new DouyuAdapterError('LOCAL_STREAM_PROXY_FAILED', 'port=secret')))
  .toEqual({
    code: 'LOCAL_STREAM_PROXY_FAILED',
    message: '无法创建本地播放通道，请重试',
    retryable: true,
  });
```

- [ ] **Step 2: Run the focused tests and confirm the expected failure**

Run: `npx vitest run tests/ipc-contract.test.ts tests/streamget-ipc-error.test.ts`

Expected: FAIL because `quality`, `releaseStreamProxy`, and `LOCAL_STREAM_PROXY_FAILED` are not defined.

- [ ] **Step 3: Add the domain and IPC types**

Use one request type that can carry the persisted user options plus the explicit adaptive override:

```ts
export type StreamRequestQuality = StreamQuality | '720p';

export const STREAM_REQUEST_QUALITIES: readonly StreamRequestQuality[] = [
  'auto', 'original', 'super', 'high', 'standard', '720p',
];

export type DouyuAdapterErrorCode =
  | 'ROOM_NOT_FOUND'
  | 'NETWORK_UNAVAILABLE'
  | 'PROTOCOL_CHANGED'
  | 'STREAMGET_UNAVAILABLE'
  | 'LOCAL_STREAM_PROXY_FAILED';

export interface DouyuAdapter {
  search(input: RoomInput): Promise<RoomCandidate[]>;
  getStreamAvailability(
    roomId: string,
    quality?: StreamRequestQuality,
  ): Promise<StreamAvailability>;
  releaseStream?(roomId: string): Promise<void>;
}
```

Update the IPC request and validator:

```ts
export interface GetStreamAvailabilityRequest {
  roomId: string;
  quality: StreamRequestQuality;
}

export interface ReleaseStreamProxyRequest {
  roomId: string;
}

export function isValidGetStreamAvailabilityRequest(
  value: unknown,
): value is GetStreamAvailabilityRequest {
  if (!value || typeof value !== 'object') return false;
  const request = value as { roomId?: unknown; quality?: unknown };
  return typeof request.roomId === 'string'
    && /^\d{1,20}$/.test(request.roomId)
    && STREAM_REQUEST_QUALITIES.includes(request.quality as StreamRequestQuality);
}
```

Reuse the numeric room validator for release requests, add `playback.releaseProxy`, and add the safe Chinese error mapping. Do not include upstream URLs, paths, ports, or exception text in `IpcError`.

- [ ] **Step 4: Run the focused tests**

Run: `npx vitest run tests/ipc-contract.test.ts tests/streamget-ipc-error.test.ts`

Expected: PASS.

- [ ] **Step 5: Commit the contract**

```bash
git add src/domain/douyu-adapter.ts src/shared/ipc-contract.ts tests/ipc-contract.test.ts tests/streamget-ipc-error.test.ts
git commit -m "feat: define adaptive playback contracts"
```

### Task 2: Add the bounded StreamGet resolution queue

**Files:**
- Create: `src/main/streamget-resolution-queue.ts`
- Create: `tests/streamget-resolution-queue.test.ts`

- [ ] **Step 1: Write failing queue tests**

Cover the concurrency cap, identical-request coalescing, quality-specific keys, failure isolation, and queued cancellation:

```ts
it('runs at most two sidecars and continues after a rejection', async () => {
  const gates = new Map<string, ReturnType<typeof deferred<StreamgetRawResult>>>();
  let active = 0;
  let peak = 0;
  const queue = createStreamgetResolutionQueue(async (roomId, quality) => {
    active += 1;
    peak = Math.max(peak, active);
    const gate = deferred<StreamgetRawResult>();
    gates.set(`${roomId}:${quality}`, gate);
    try { return await gate.promise; } finally { active -= 1; }
  }, { concurrency: 2 });

  const first = queue.resolve('1', 'original');
  const second = queue.resolve('2', '720p');
  const third = queue.resolve('3', '720p');
  await Promise.resolve();
  expect(peak).toBe(2);
  expect(gates.has('3:720p')).toBe(false);

  gates.get('1:original')!.reject(new Error('failed'));
  await expect(first).rejects.toThrow('failed');
  await vi.waitFor(() => expect(gates.has('3:720p')).toBe(true));
  gates.get('2:720p')!.resolve(liveResult('2'));
  gates.get('3:720p')!.resolve(liveResult('3'));
  await expect(Promise.all([second, third])).resolves.toHaveLength(2);
});

it('coalesces only matching room and quality requests', async () => {
  const resolver = vi.fn(async (roomId: string) => liveResult(roomId));
  const queue = createStreamgetResolutionQueue(resolver);
  await Promise.all([
    queue.resolve('1', '720p'),
    queue.resolve('1', '720p'),
    queue.resolve('1', 'original'),
  ]);
  expect(resolver).toHaveBeenCalledTimes(2);
});

it('rejects queued work for a removed room without cancelling active work', async () => {
  const queue = createStreamgetResolutionQueue(controlledResolver, { concurrency: 1 });
  const active = queue.resolve('1', 'original');
  const queued = queue.resolve('2', '720p');
  queue.cancel('2');
  await expect(queued).rejects.toMatchObject({ code: 'RESOLUTION_CANCELLED' });
  finishRoom('1');
  await expect(active).resolves.toMatchObject({ roomId: '1' });
});
```

- [ ] **Step 2: Run the new test and confirm failure**

Run: `npx vitest run tests/streamget-resolution-queue.test.ts`

Expected: FAIL because the queue module does not exist.

- [ ] **Step 3: Implement the queue**

Expose this exact boundary:

```ts
export type StreamgetResolver = (
  roomId: string,
  quality: StreamRequestQuality,
) => Promise<StreamgetRawResult>;

export interface StreamgetResolutionQueue {
  resolve(roomId: string, quality: StreamRequestQuality): Promise<StreamgetRawResult>;
  cancel(roomId: string): void;
  cancelAll(): void;
}

export class StreamgetResolutionCancelledError extends Error {
  readonly code = 'RESOLUTION_CANCELLED';
}
```

Store queued jobs in FIFO order, key the coalescing map by `${roomId}:${quality}`, and start work while `activeCount < concurrency`. `cancel(roomId)` must reject only jobs that have not started. Every resolve or reject path must delete the coalescing entry and start the next queued job. Validate `concurrency` as a positive integer and default it to `2`.

- [ ] **Step 4: Run the queue tests**

Run: `npx vitest run tests/streamget-resolution-queue.test.ts`

Expected: PASS with observed peak concurrency `2`.

- [ ] **Step 5: Commit the queue**

```bash
git add src/main/streamget-resolution-queue.ts tests/streamget-resolution-queue.test.ts
git commit -m "feat: bound StreamGet resolution concurrency"
```

### Task 3: Resolve requested quality through StreamGet with App fallback

**Files:**
- Create: `src/main/douyu-stream-url.ts`
- Modify: `src/main/streamget-bridge.ts`
- Modify: `scripts/streamget_bridge.py`
- Modify: `tests/streamget-bridge.test.ts`

- [ ] **Step 1: Write failing bridge and sidecar-boundary tests**

Replace the App-only source assertion and add launch, timeout, output-quality, and URL validation cases:

```ts
expect(resolveStreamgetLaunch('63136', '720p', devOptions)).toMatchObject({
  args: ['C:\\DouyuMonitor\\scripts\\streamget_bridge.py', '63136', '720p'],
});
expect(resolveStreamgetLaunch('63136', 'original', packagedOptions)).toMatchObject({
  args: ['63136', 'original'],
});

await bridge.resolve('63136', '720p');
expect(run).toHaveBeenCalledWith('63136', '720p');

expect(parseStreamgetResponse('63136', JSON.stringify({
  roomId: '63136',
  isLive: true,
  resolvedQuality: '720p',
  source: 'web-h5',
  flvUrl: allowedUrl,
}))).toMatchObject({ resolvedQuality: '720p', source: 'web-h5' });

expect(sidecarSource).toContain('fetch_web_stream_data');
expect(sidecarSource).toContain('fetch_stream_url');
expect(sidecarSource).toContain('fetch_app_stream_data');
expect(sidecarSource).toContain('"720p": "HD"');
```

Use a fake `execFile` runner or fake timer to assert the default timeout passed by `createStreamgetBridge` is `30_000` rather than `20_000`.

- [ ] **Step 2: Run the bridge test and confirm failure**

Run: `npx vitest run tests/streamget-bridge.test.ts`

Expected: FAIL because the bridge accepts only `roomId`, returns no quality metadata, and the sidecar uses only App search.

- [ ] **Step 3: Extract the shared URL validator**

Move the allowlist to `src/main/douyu-stream-url.ts` and export:

```ts
const ALLOWED_HOST_SUFFIXES = ['.douyucdn.cn', '.douyucdn2.cn', '.edgesrv.com'];

export function parseAllowedDouyuFlvUrl(value: unknown): URL {
  if (typeof value !== 'string' || value.length === 0) {
    throw new DouyuStreamUrlError('INVALID_RESPONSE');
  }
  const url = new URL(value);
  const hostname = url.hostname.toLowerCase();
  const allowed = ['http:', 'https:'].includes(url.protocol)
    && ALLOWED_HOST_SUFFIXES.some((suffix) => hostname.endsWith(suffix));
  if (!allowed) throw new DouyuStreamUrlError('UNSAFE_STREAM_URL');
  return url;
}
```

Preserve the existing spoof-host tests. Convert URL constructor failures to `UNSAFE_STREAM_URL` without exposing the original value.

- [ ] **Step 4: Extend the TypeScript bridge**

Use these result fields and signatures:

```ts
export type StreamgetSource = 'web-h5' | 'app-fallback';

export type StreamgetRawResult = {
  roomId: string;
  isLive: boolean;
  flvUrl?: string;
  resolvedQuality?: StreamRequestQuality;
  source?: StreamgetSource;
};

export interface StreamgetBridge {
  resolve(roomId: string, quality: StreamRequestQuality): Promise<StreamgetRawResult>;
}
```

Pass `quality` as the final executable/script argument, require valid `resolvedQuality` and `source` for live results, keep offline results URL-free, and change the default timeout to `30_000`.

- [ ] **Step 5: Implement H5 resolution and App fallback in the Python sidecar**

Use this exact quality map:

```py
QUALITY_CODES = {
    "auto": None,
    "original": "OD",
    "super": "UHD",
    "high": "HD",
    "standard": "SD",
    "720p": "HD",
}
```

For a valid room and quality:

1. Call `fetch_web_stream_data(room_url)`.
2. If `is_live` is false, emit `{roomId, isLive: false}` without trying App search.
3. Call `fetch_stream_url(web_data, video_quality=QUALITY_CODES[quality])`.
4. When it returns an FLV URL, emit `source: "web-h5"` and the requested semantic quality.
5. If H5 resolution raises or returns no FLV URL, call `fetch_app_stream_data(room_url)` and emit `source: "app-fallback"`, `resolvedQuality: "original"` when its FLV URL exists.
6. Emit only the existing safe error code if both paths fail. Do not print exception text, cookies, signatures, or URLs to stderr.

Require exactly two arguments after the script name: room ID and request quality.

- [ ] **Step 6: Run bridge tests and a local sidecar smoke check**

Run: `npx vitest run tests/streamget-bridge.test.ts`

Run: `.venv\Scripts\python.exe scripts\streamget_bridge.py 71415 720p`

Expected: tests PASS; smoke output is one JSON object with either a live safe FLV result, an offline result, or the safe `STREAMGET_UNAVAILABLE` code. Do not copy the signed URL into logs or documentation.

- [ ] **Step 7: Commit the bridge change**

```bash
git add src/main/douyu-stream-url.ts src/main/streamget-bridge.ts scripts/streamget_bridge.py tests/streamget-bridge.test.ts
git commit -m "feat: resolve StreamGet playback quality"
```

### Task 4: Add the secure per-room loopback proxy

**Files:**
- Create: `src/main/stream-proxy-manager.ts`
- Create: `tests/stream-proxy-manager.test.ts`

- [ ] **Step 1: Write failing security and transport tests**

Start a local upstream HTTP server in the test and inject a validator that accepts only that server. Cover:

```ts
const manager = createStreamProxyManager({
  validateUpstream: (url) => url.hostname === '127.0.0.1',
  createToken: () => 'fixed-test-token',
});
const firstUrl = await manager.register('1', `${upstreamBase}/live.flv`);
const secondUrl = await manager.register('2', `${upstreamBase}/live.flv`);

expect(new URL(firstUrl).hostname).toBe('127.0.0.1');
expect(new URL(firstUrl).port).not.toBe(new URL(secondUrl).port);
expect(new URL(firstUrl).pathname).toBe('/stream/fixed-test-token.flv');
```

Also assert:

- the correct local path streams the upstream bytes with `Content-Type: video/x-flv` and an `Access-Control-Allow-Origin` header;
- a wrong token, wrong path, non-GET method, or wrong `Host` returns 403/404/405 without opening upstream;
- the default validator rejects `example.invalid` and spoofed Douyu suffixes;
- eight rooms receive eight distinct ports and can read concurrently;
- updating a room reuses its port and token but uses the new upstream URL on the next request;
- a blocked downstream pauses upstream until `drain` and does not buffer without a bound;
- client abort destroys the upstream request;
- redirects are revalidated and stop after three hops;
- `release(roomId)` closes that room's active requests and server;
- `closeAll()` closes every room and is idempotent.

- [ ] **Step 2: Run the proxy test and confirm failure**

Run: `npx vitest run tests/stream-proxy-manager.test.ts`

Expected: FAIL because the proxy manager does not exist.

- [ ] **Step 3: Implement the proxy manager boundary**

Expose:

```ts
export interface StreamProxyManager {
  register(roomId: string, upstreamUrl: string): Promise<string>;
  release(roomId: string): Promise<void>;
  closeAll(): Promise<void>;
}

export interface StreamProxyManagerOptions {
  validateUpstream?: (url: URL) => boolean;
  createToken?: () => string;
  maxRedirects?: number;
}
```

Production defaults must:

- call `parseAllowedDouyuFlvUrl` for the original URL and every redirect;
- create the token with `randomBytes(24).toString('hex')`;
- bind every room server with `{ host: '127.0.0.1', port: 0 }`;
- require `Host` to equal `127.0.0.1:${assignedPort}` and path to equal `/stream/${token}.flv`;
- create separate `http.Agent` and `https.Agent` instances per room;
- copy no upstream cookies and send only required FLV request headers, including a Douyu `Referer` and user agent;
- set `Content-Type: video/x-flv`, `Cache-Control: no-store`, `Access-Control-Allow-Origin: *`, and `X-Content-Type-Options: nosniff`;
- pause the upstream response when `downstream.write(chunk)` returns false and resume only on `drain`;
- destroy the upstream request on downstream `close` or `error`;
- return 502 and close the response on upstream failures or non-success status;
- follow at most three 301/302/303/307/308 responses;
- destroy both agents and all tracked sockets during room release.

Wrap listen/bind failures in a dedicated `StreamProxyError` with code `LOCAL_STREAM_PROXY_FAILED`.

- [ ] **Step 4: Run the proxy tests**

Run: `npx vitest run tests/stream-proxy-manager.test.ts`

Expected: PASS, including eight distinct loopback ports and connection cleanup.

- [ ] **Step 5: Commit the proxy**

```bash
git add src/main/stream-proxy-manager.ts tests/stream-proxy-manager.test.ts
git commit -m "feat: proxy each room through loopback"
```

### Task 5: Return local playback URLs from the StreamGet adapter

**Files:**
- Modify: `src/infrastructure/streamget-douyu-adapter.ts`
- Modify: `tests/streamget-douyu-adapter.test.ts`

- [ ] **Step 1: Write failing adapter tests**

Inject a resolver queue and proxy manager and assert the adapter never returns the signed upstream URL:

```ts
const resolver = {
  resolve: vi.fn(async () => ({
    roomId: '63136',
    isLive: true,
    flvUrl: upstreamUrl,
    resolvedQuality: '720p' as const,
    source: 'web-h5' as const,
  })),
  cancel: vi.fn(),
  cancelAll: vi.fn(),
};
const proxy = {
  register: vi.fn(async () => 'http://127.0.0.1:41001/stream/token.flv'),
  release: vi.fn(async () => undefined),
  closeAll: vi.fn(async () => undefined),
};

const availability = await createStreamgetDouyuAdapter(
  onlineBaseAdapter(), resolver, proxy,
).getStreamAvailability('63136', '720p');

expect(resolver.resolve).toHaveBeenCalledWith('63136', '720p');
expect(proxy.register).toHaveBeenCalledWith('63136', upstreamUrl);
expect(availability).toMatchObject({
  kind: 'available',
  variants: [{ quality: 'high', label: '720p', playbackUrl: expect.stringMatching(/^http:\/\/127\.0\.0\.1:/) }],
});
expect(JSON.stringify(availability)).not.toContain('wsAuth');
```

Add cases for `original`, App fallback labeling, offline rooms not invoking the resolver/proxy, resolver failure mapping to `STREAMGET_UNAVAILABLE`, and proxy failure mapping to `LOCAL_STREAM_PROXY_FAILED`.

- [ ] **Step 2: Run the adapter tests and confirm failure**

Run: `npx vitest run tests/streamget-douyu-adapter.test.ts`

Expected: FAIL because the adapter returns the upstream URL and accepts no proxy.

- [ ] **Step 3: Implement the adapter integration**

Accept `StreamgetResolutionQueue` and `StreamProxyManager`. Default omitted quality to `auto` only for compatibility with non-renderer callers. Keep the base adapter's offline short-circuit. Map semantic qualities to existing `StreamVariant.quality` values:

```ts
const VARIANT_QUALITY: Record<StreamRequestQuality, StreamQuality> = {
  auto: 'auto',
  original: 'original',
  super: 'super',
  high: 'high',
  standard: 'standard',
  '720p': 'high',
};
```

Use the semantic label `720p` for the adaptive override and `原画/超清/高清/标清/自动` for user modes. Register the validated upstream URL with the proxy, return only the local URL, and translate errors without exposing upstream details.

- [ ] **Step 4: Run the adapter tests**

Run: `npx vitest run tests/streamget-douyu-adapter.test.ts`

Expected: PASS.

- [ ] **Step 5: Commit the adapter**

```bash
git add src/infrastructure/streamget-douyu-adapter.ts tests/streamget-douyu-adapter.test.ts
git commit -m "feat: return local proxied playback sources"
```

### Task 6: Carry quality and release through approved IPC

**Files:**
- Modify: `src/main/ipc-handlers.ts`
- Modify: `src/preload/bridge.ts`
- Modify: `src/infrastructure/renderer-douyu-adapter.ts`
- Modify: `tests/ipc-handlers.test.ts`
- Modify: `tests/preload-bridge.test.ts`
- Modify: `tests/renderer-douyu-adapter.test.ts`

- [ ] **Step 1: Write failing IPC and renderer-adapter tests**

Assert exact payloads and lifecycle calls:

```ts
await handlers.get(IPC_CHANNELS.getStreamAvailability)?.(
  {}, { roomId: '63136', quality: '720p' },
);
expect(adapter.getStreamAvailability).toHaveBeenCalledWith('63136', '720p');

await handlers.get(IPC_CHANNELS.releaseStreamProxy)?.({}, { roomId: '63136' });
expect(playbackLifecycle.release).toHaveBeenCalledWith('63136');

await api.getStreamAvailability('63136', 'original');
await api.releaseStreamProxy('63136');
expect(calls).toContainEqual({
  channel: IPC_CHANNELS.getStreamAvailability,
  payload: { roomId: '63136', quality: 'original' },
});
expect(calls).toContainEqual({
  channel: IPC_CHANNELS.releaseStreamProxy,
  payload: { roomId: '63136' },
});

await rendererAdapter.releaseStream?.('63136');
expect(appApi.releaseStreamProxy).toHaveBeenCalledWith('63136');
```

Also reject invalid quality and malformed release room IDs before calling main-process dependencies.

- [ ] **Step 2: Run the focused tests and confirm failure**

Run: `npx vitest run tests/ipc-handlers.test.ts tests/preload-bridge.test.ts tests/renderer-douyu-adapter.test.ts`

Expected: FAIL because quality is not forwarded and release is not exposed.

- [ ] **Step 3: Add typed IPC handlers**

Add a narrow dependency after the notification service parameter:

```ts
export interface PlaybackProxyLifecycle {
  release(roomId: string): Promise<void>;
}
```

Provide an `unavailablePlaybackProxyLifecycle` default whose `release` resolves without work so existing mock and test registrations remain source-compatible. The availability handler must call `adapter.getStreamAvailability(request.roomId, request.quality)`. The release handler must validate the numeric room ID, await `playbackLifecycle.release(roomId)`, return `ok(undefined)`, and map errors through `failed`.

- [ ] **Step 4: Extend preload and renderer adapter**

Expose only these typed additions:

```ts
getStreamAvailability(
  roomId: string,
  quality: StreamRequestQuality,
): Promise<GetStreamAvailabilityResult>;
releaseStreamProxy(roomId: string): Promise<IpcResult<void>>;
```

Update the renderer adapter to pass quality, translate failed results to `DouyuAdapterError` when the code is a domain code, and implement `releaseStream` by calling the preload method. Keep the mock fallback functional with a no-op release method.

- [ ] **Step 5: Run the focused IPC tests**

Run: `npx vitest run tests/ipc-handlers.test.ts tests/preload-bridge.test.ts tests/renderer-douyu-adapter.test.ts`

Expected: PASS and `AppApi` still has no generic invoke method.

- [ ] **Step 6: Commit the IPC lifecycle**

```bash
git add src/main/ipc-handlers.ts src/preload/bridge.ts src/infrastructure/renderer-douyu-adapter.ts tests/ipc-handlers.test.ts tests/preload-bridge.test.ts tests/renderer-douyu-adapter.test.ts
git commit -m "feat: expose typed playback proxy lifecycle"
```

### Task 7: Calculate effective quality without overwriting user choice

**Files:**
- Create: `src/renderer/store/stream-quality-policy.ts`
- Create: `tests/stream-quality-policy.test.ts`

- [ ] **Step 1: Write failing policy tests**

Use plain room inputs and cover the threshold, offline exclusion, primary override, and transition diff:

```ts
expect(resolveEffectiveQualities(fourOnlineRooms, '1')).toEqual(new Map([
  ['1', 'original'],
  ['2', 'super'],
  ['3', 'high'],
  ['4', 'standard'],
]));

expect(resolveEffectiveQualities(fiveOnlineRooms, '1')).toEqual(new Map([
  ['1', 'original'],
  ['2', '720p'],
  ['3', '720p'],
  ['4', '720p'],
  ['5', '720p'],
]));

expect(changedEffectiveQualityRoomIds(
  resolveEffectiveQualities(fiveOnlineRooms, '1'),
  resolveEffectiveQualities(fiveOnlineRooms, '3'),
)).toEqual(['1', '3']);
```

Add a case where five listed rooms include one offline room; the four online rooms must retain user quality.

- [ ] **Step 2: Run the policy test and confirm failure**

Run: `npx vitest run tests/stream-quality-policy.test.ts`

Expected: FAIL because the policy module does not exist.

- [ ] **Step 3: Implement pure policy functions**

Expose:

```ts
export interface QualityPolicyRoom {
  roomId: string;
  online: boolean;
  status: RoomStatus;
  quality: StreamQuality;
}

export function resolveEffectiveQualities(
  rooms: readonly QualityPolicyRoom[],
  primaryRoomId?: string,
): ReadonlyMap<string, StreamRequestQuality>;

export function changedEffectiveQualityRoomIds(
  before: ReadonlyMap<string, StreamRequestQuality>,
  after: ReadonlyMap<string, StreamRequestQuality>,
): string[];
```

Count only rooms where `online` is true and `status !== 'offline'`. At four or fewer, return each room's stored `quality`. Above four, return `original` for the primary and `720p` for every other online room. Keep offline rooms in the map with their stored quality so returning online has a deterministic transition.

- [ ] **Step 4: Run policy tests**

Run: `npx vitest run tests/stream-quality-policy.test.ts`

Expected: PASS.

- [ ] **Step 5: Commit the policy**

```bash
git add src/renderer/store/stream-quality-policy.ts tests/stream-quality-policy.test.ts
git commit -m "feat: calculate adaptive stream quality"
```

### Task 8: Apply quality transitions and reject stale results in the workspace

**Files:**
- Modify: `src/renderer/store/workspace-store.ts`
- Modify: `tests/workspace-store.test.ts`
- Modify: `tests/monitoring-status-panel.test.tsx`

- [ ] **Step 1: Write failing workspace tests**

Add deterministic tests for each state transition:

```ts
it('downgrades non-primary rooms when the fifth online room is added', async () => {
  const getStreamAvailability = vi.fn(async (roomId, quality) => available(roomId, quality));
  const store = createWorkspaceStore({ ...createMockDouyuAdapter(), getStreamAvailability });
  for (const roomId of ['1', '2', '3', '4', '5']) store.getState().addRoom(candidate(roomId));
  await flushPromises();
  expect(getStreamAvailability).toHaveBeenCalledWith('1', 'original');
  for (const roomId of ['2', '3', '4', '5']) {
    expect(getStreamAvailability).toHaveBeenCalledWith(roomId, '720p');
  }
  expect(store.getState().rooms.find((room) => room.roomId === '2')?.quality).toBe('auto');
});

it('refreshes only the old and new primary above four rooms', async () => {
  // Settle initial resolutions, clear the spy, then switch primary from 1 to 3.
  store.getState().setPrimaryRoom('3');
  await flushPromises();
  expect(getStreamAvailability.mock.calls).toEqual(expect.arrayContaining([
    ['1', '720p'],
    ['3', 'original'],
  ]));
  expect(getStreamAvailability).toHaveBeenCalledTimes(2);
});
```

Also test:

- removing the fifth room restores each survivor's persisted quality and refreshes only changed effective qualities;
- changing a stored quality at four rooms refreshes that room, while changing it under a forced `720p` override persists without a needless refresh;
- an offline room does not count toward the threshold or trigger source resolution;
- `removeRoom` calls `adapter.releaseStream(roomId)` once and a removed room never accepts a late availability result;
- a late `720p` result is ignored after that room becomes the primary and requests `original`;
- startup with eight online persisted rooms requests the correct effective qualities through the two-worker main queue;
- metadata transitions from offline to online recompute the threshold before source refresh.
- a `LOCAL_STREAM_PROXY_FAILED` workspace error remains visible in the monitoring status panel with its safe user message.

- [ ] **Step 2: Run workspace tests and confirm failure**

Run: `npx vitest run tests/workspace-store.test.ts`

Expected: FAIL because the store passes no quality, does not release proxies, and accepts stale results.

- [ ] **Step 3: Add non-persisted effective quality state**

Extend `RoomSession`:

```ts
effectiveQuality: StreamRequestQuality;
```

Set it in `toSession` and recompute it after the complete initial room list is built. Do not add it to `LibraryRoom`, `WorkspacePresetRoom`, or any persistence schema.

- [ ] **Step 4: Add one transition helper inside the store factory**

The helper must:

1. Capture `resolveEffectiveQualities` before a state-changing action.
2. Apply the state change.
3. Resolve the new map and update every session's `effectiveQuality` in one `set` call.
4. Call `refreshStreamAvailability` only for online room IDs returned by `changedEffectiveQualityRoomIds`.

Use it in `addRoom`, `removeRoom`, `setPrimaryRoom`, `setQuality`, successful online/offline metadata changes, preset loads, and group switches that replace the active room set. Preserve the current placement swap behavior in `setPrimaryRoom`.

Remove old direct refresh calls from add, metadata, preset, and group paths when the transition helper owns that refresh. Keep one startup loop after the initial effective-quality map has been applied. Each room/quality pair must enter the queue once per transition.

- [ ] **Step 5: Make refreshes quality-aware and stale-safe**

At the start of `refreshStreamAvailability`, capture:

```ts
const requestedQuality = room.effectiveQuality;
```

Call `adapter.getStreamAvailability(roomId, requestedQuality)`. Before applying either success or error, require the room to still exist and its current `effectiveQuality` to equal `requestedQuality`. A stale result must leave the newer `checking` or `available` state untouched.

Call `void adapter.releaseStream?.(roomId)` after registry removal. The renderer IPC handles release errors safely; room removal must remain synchronous from the UI's perspective.

- [ ] **Step 6: Run workspace tests**

Run: `npx vitest run tests/stream-quality-policy.test.ts tests/workspace-store.test.ts tests/workspace-persistence.test.ts tests/workspace-presets-persistence.test.ts tests/monitoring-status-panel.test.tsx`

Expected: PASS, with no persistence snapshot containing `effectiveQuality`.

- [ ] **Step 7: Commit workspace behavior**

```bash
git add src/renderer/store/workspace-store.ts tests/workspace-store.test.ts tests/monitoring-status-panel.test.tsx
git commit -m "feat: apply adaptive quality transitions"
```

### Task 9: Display the active effective quality

**Files:**
- Modify: `src/renderer/components/RoomTile.tsx`
- Create: `tests/room-tile-quality.test.tsx`
- Modify: `tests/app-smoke.test.tsx`

- [ ] **Step 1: Write failing UI tests**

Render a five-room workspace whose secondary room has `quality: 'original'`, `effectiveQuality: '720p'`, and an available `high` variant labeled `720p`. Assert its control displays `720p` while the stored user selection stays `original`. Switch that room to primary and assert the control changes to `原画` after the new availability arrives.

Also assert the video element remains mounted for unaffected rooms during a primary switch; only the old and new primary source URLs may change.

- [ ] **Step 2: Run focused renderer tests and confirm failure**

Run: `npx vitest run tests/room-tile-quality.test.tsx tests/app-smoke.test.tsx`

Expected: FAIL because the selector derives its value only from `room.quality`.

- [ ] **Step 3: Derive the selected option from effective quality**

Map the adaptive semantic value only for display:

```ts
const displayedQuality: StreamQuality = room.effectiveQuality === '720p'
  ? 'high'
  : room.effectiveQuality;
const selectedQuality = room.streamAvailability?.kind === 'available'
  ? qualityOptions.find((option) => option.value === displayedQuality)?.value
    ?? qualityOptions[0]?.value
    ?? displayedQuality
  : disabledQualityOptions[0].value;
```

Keep `onChange` connected to `setQuality`; the store decides whether that user selection changes the active source. Do not add a new card, badge, tooltip, or persistent field.

- [ ] **Step 4: Run renderer tests**

Run: `npx vitest run tests/room-tile-quality.test.tsx tests/app-smoke.test.tsx`

Expected: PASS and unaffected video nodes retain identity.

- [ ] **Step 5: Commit the display change**

```bash
git add src/renderer/components/RoomTile.tsx tests/room-tile-quality.test.tsx tests/app-smoke.test.tsx
git commit -m "feat: show effective stream quality"
```

### Task 10: Assemble and dispose main-process playback services

**Files:**
- Modify: `src/main/main.ts`
- Modify: `tests/main-config.test.ts`

- [ ] **Step 1: Write a failing assembly test**

Extend the existing source-level assembly test to require the service graph and cleanup hooks:

```ts
expect(source).toContain('createStreamProxyManager(');
expect(source).toContain('createStreamgetResolutionQueue(');
expect(source).toContain('concurrency: 2');
expect(source).toContain('proxyManager.release(roomId)');
expect(source).toContain('resolutionQueue.cancel(roomId)');
expect(source).toContain('resolutionQueue.cancelAll()');
expect(source).toContain('proxyManager.closeAll()');
```

- [ ] **Step 2: Run the assembly test and confirm failure**

Run: `npx vitest run tests/main-config.test.ts`

Expected: FAIL because main creates the StreamGet adapter directly.

- [ ] **Step 3: Wire the service graph**

Build services in this order after `app.whenReady()`:

```ts
const proxyManager = createStreamProxyManager();
const bridge = createStreamgetBridge({
  isPackaged: app.isPackaged,
  resourcesPath: process.resourcesPath,
  timeoutMs: 30_000,
});
const resolutionQueue = createStreamgetResolutionQueue(
  (roomId, quality) => bridge.resolve(roomId, quality),
  { concurrency: 2 },
);
const streamgetAdapter = createStreamgetDouyuAdapter(
  createDouyuHttpAdapter(),
  resolutionQueue,
  proxyManager,
);
const playbackLifecycle = {
  async release(roomId: string) {
    resolutionQueue.cancel(roomId);
    await proxyManager.release(roomId);
  },
};
```

Pass `playbackLifecycle` to `registerIpcHandlers`. In `before-quit`, call `resolutionQueue.cancelAll()` and start `proxyManager.closeAll()` alongside the existing danmaku cleanup. Guard cleanup so repeated quit events are idempotent.

- [ ] **Step 4: Run main and integration-focused tests**

Run: `npx vitest run tests/main-config.test.ts tests/ipc-handlers.test.ts tests/streamget-douyu-adapter.test.ts tests/stream-proxy-manager.test.ts`

Expected: PASS.

- [ ] **Step 5: Commit main assembly**

```bash
git add src/main/main.ts tests/main-config.test.ts
git commit -m "feat: assemble multi-stream playback services"
```

### Task 11: Strengthen the eight-room runtime probe

**Files:**
- Modify: `scripts/performance-baseline.mjs`
- Create: `tests/performance-baseline-script.test.ts`
- Modify: `tests/performance-utils.test.ts`

- [ ] **Step 1: Write failing probe-structure tests**

Require each room report to contain:

```js
{
  roomId,
  playable: false,
  firstFrameMs: null,
  initialCurrentTime: null,
  finalCurrentTime: null,
  initialDecodedFrames: null,
  finalDecodedFrames: null,
  videoWidth: null,
  videoHeight: null,
  continuedPlayback: false,
}
```

Add evaluation tests that fail when a room fires `loadeddata` but its time or decoded-frame count does not increase during the sustained sample. Add quality-policy evaluation: with more than four rooms, the primary may exceed 1280x720 while each non-primary room must not.

- [ ] **Step 2: Run probe tests and confirm failure**

Run: `npx vitest run tests/performance-baseline-script.test.ts tests/performance-utils.test.ts`

Expected: FAIL because the report records only first-frame readiness.

- [ ] **Step 3: Collect sustained playback metrics**

After all rooms are added, sample every video element before and after a 30-second sustained window. Read `currentTime`, `videoWidth`, `videoHeight`, and `getVideoPlaybackQuality()?.totalVideoFrames` when available; use Chromium's `webkitDecodedFrameCount` fallback. Mark `continuedPlayback` only when `currentTime` and decoded frames both increase.

The profile must fail if:

- any room lacks a first frame within 30 seconds;
- any room, including rooms 7 and 8, stops advancing during the next 30 seconds;
- any non-primary stream above the four-room threshold exceeds 1280x720;
- fewer videos are playing than requested;
- the renderer reports Blob Worker, MSE, console, page, or overlap errors.

Keep CPU, working-set, private-memory, screenshot, and isolated user-data evidence. Add total dropped frames when the browser exposes them.

- [ ] **Step 4: Run probe unit tests**

Run: `npx vitest run tests/performance-baseline-script.test.ts tests/performance-utils.test.ts`

Expected: PASS.

- [ ] **Step 5: Commit the runtime probe**

```bash
git add scripts/performance-baseline.mjs tests/performance-baseline-script.test.ts tests/performance-utils.test.ts
git commit -m "test: verify sustained eight-room playback"
```

### Task 12: Run full verification and packaged Electron acceptance

**Files:**
- Verify only unless a failing check requires a scoped fix.
- Evidence: temporary directory printed by `scripts/performance-baseline.mjs`.

- [ ] **Step 1: Run the complete automated suite**

Run: `npm test`

Expected: all Vitest files and tests PASS.

- [ ] **Step 2: Run static and production builds**

Run: `npm run typecheck`

Expected: exit code 0 with no TypeScript errors.

Run: `npm run build`

Expected: main, preload, and renderer builds finish with exit code 0.

- [ ] **Step 3: Verify protected playback settings and version**

Run:

```powershell
rg -n "enableWorker: false|enableWorkerForMSE: false" src/renderer/components/FlvVideo.tsx
(Get-Content -Raw package.json | ConvertFrom-Json).version
```

Expected: both worker flags remain `false`; version remains `0.1.4`.

- [ ] **Step 4: Build the sidecar and unpacked Electron app**

Run: `npm run dist:unpacked`

Expected: `release/win-unpacked/DouyuMonitor.exe` and bundled `resources/streamget/streamget_bridge.exe` exist.

- [ ] **Step 5: Run eight-room packaged acceptance**

Use eight currently live distinct room IDs. Start with the known probe list, replacing offline rooms before the run:

```powershell
$env:DOUYU_PERF_ROOM_IDS='320155,12816258,12467917,8825784,12767534,11921577,12821575,12738439'
$env:DOUYU_PERF_PROFILES='8'
$env:DOUYU_PERF_SAMPLE_MS='30000'
$env:DOUYU_PERF_EXECUTABLE='release/win-unpacked/DouyuMonitor.exe'
npm run test:performance
```

Expected:

- all eight rooms obtain a first frame within 30 seconds;
- rooms 7 and 8, as well as rooms 1 through 6, increase `currentTime` and decoded frame count during the sustained 30-second sample;
- the primary remains original quality;
- each non-primary is no larger than 1280x720;
- no Blob Worker, MSE, page, console, or layout-overlap error occurs;
- the report records CPU, memory, dropped frames, and screenshot evidence.

- [ ] **Step 6: Compare with the existing eight-original baseline**

Compare peak/average CPU and dropped frames with the pre-change artifact. Record exact values in the task handoff. Do not claim an improvement if the baseline artifact or metric is unavailable; report the missing comparison separately while still reporting functional playback evidence.

- [ ] **Step 7: Inspect the final diff and commit verification-driven fixes only**

Run: `git status --short` and `git diff --check`.

Expected: no whitespace errors; generated `release-*`, `artifacts`, and debug directories remain uncommitted. Preserve all pre-existing user changes.

If verification required a scoped fix, return to that task's explicit file list and test command, then create a focused `fix: stabilize eight-room playback` commit containing only those files.

Do not package a new version, upload a release, or change version `0.1.4` as part of this plan.
