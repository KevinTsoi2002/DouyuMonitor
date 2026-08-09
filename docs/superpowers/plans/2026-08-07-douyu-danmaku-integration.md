# Douyu Live Danmaku Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Connect each Electron room to Douyu's anonymous read-only danmaku service and display isolated real-time `chatmsg` messages over that room's playback surface.

**Architecture:** Electron Main owns one WebSocket session per room and contains all non-official protocol logic. Main normalizes and batches messages through a typed Preload bridge; a separate Renderer store schedules bounded, one-shot overlays without putting high-frequency data in `workspace-store`.

**Tech Stack:** TypeScript, Electron 43, React 19, Zustand, Vitest, `ws` 8.21.2, `@types/ws` 8.18.1, Vite, Playwright/browser QA.

---

## Execution Constraints

- Read the approved design before implementation: `docs/superpowers/specs/2026-08-07-douyu-danmaku-integration-design.md`.
- Keep the video constraint unchanged. Do not add `getH5Play`, signing, cookies, page-script execution, embedded Douyu pages, guessed CDN URLs, or a video element.
- Send only `loginreq`, `joingroup`, `mrkl`, and `logout` to Douyu.
- Forward only normal `chatmsg` messages.
- Do not log nicknames, message text, raw frames, cookies, tokens, or response bodies.
- `D:\DouyuMonitor` has no `.git` directory. Do not initialize Git without user authorization. Each task ends with a test checkpoint instead of a commit. If the user creates a repository later, commit each completed task separately.
- Context7 and GitHub MCP were unavailable during design. Before relying on version-specific `ws` behavior, recheck the installed package types and npm README. Do not invent library APIs.

## File Map

**Create:**

- `src/shared/danmaku-contract.ts`: normalized types, status codes, event validator.
- `src/infrastructure/douyu-danmaku/stt.ts`: flat STT serialization and parsing.
- `src/infrastructure/douyu-danmaku/protocol.ts`: binary frame encoder and incremental decoder.
- `src/infrastructure/douyu-danmaku/socket.ts`: narrow wrapper around `ws`.
- `src/infrastructure/douyu-danmaku/client.ts`: one-room connection state machine.
- `src/main/danmaku-session-manager.ts`: ownership, deduplication, normalization, batching, room cap.
- `src/infrastructure/renderer-danmaku-source.ts`: typed Renderer adapter for Preload.
- `src/infrastructure/mock-danmaku-source.ts`: browser-only Mock event source.
- `src/renderer/store/danmaku-store.ts`: per-room pending and visible queues.
- `src/renderer/store/danmaku-context.tsx`: source lifecycle, scheduler, hooks, retry command.
- `tests/danmaku-contract.test.ts`
- `tests/douyu-danmaku-protocol.test.ts`
- `tests/douyu-danmaku-socket.test.ts`
- `tests/douyu-danmaku-client.test.ts`
- `tests/danmaku-session-manager.test.ts`
- `tests/renderer-danmaku-source.test.ts`
- `tests/danmaku-store.test.ts`
- `tests/danmaku-overlay.test.tsx`

**Modify:**

- `package.json`, `package-lock.json`: add pinned WebSocket dependencies.
- `vite.main.config.ts`: keep `ws` external in the Main bundle.
- `src/shared/ipc-contract.ts`: add three allowlisted channels and room-limit error code.
- `src/preload/bridge.ts`, `src/preload/preload.ts`: expose bounded start, stop, and event subscription methods. `src/shared/window-api.d.ts` already imports `AppApi` and needs no edit.
- `src/main/ipc-handlers.ts`, `src/main/main.ts`: register and clean up danmaku sessions.
- `src/renderer/main.tsx`: choose real or Mock source and mount `DanmakuProvider`.
- `src/renderer/components/DanmakuOverlay.tsx`: consume scheduled real messages.
- `src/renderer/components/RoomPlaybackSurface.tsx`: render the overlay in both demo and blocked playback states.
- `src/renderer/components/RoomTile.tsx`: show connection state and a failed-state retry icon.
- `src/renderer/styles.css`: one-shot animation, stable dimensions, status styling.
- Existing IPC, Preload, playback, and smoke tests: update expected API keys and add integration assertions.

### Task 1: Shared Danmaku Contract

**Files:**

- Create: `src/shared/danmaku-contract.ts`
- Create: `tests/danmaku-contract.test.ts`
- Modify: `src/shared/ipc-contract.ts`
- Modify: `tests/ipc-contract.test.ts`

- [ ] **Step 1: Write the failing contract tests**

Create `tests/danmaku-contract.test.ts`:

```ts
import { describe, expect, it } from 'vitest';
import {
  isDanmakuEvent,
  type DanmakuEvent,
} from '../src/shared/danmaku-contract';

const messageEvent: DanmakuEvent = {
  type: 'messages',
  roomId: '63136',
  dropped: 0,
  messages: [{
    id: 'cid-1',
    roomId: '63136',
    nickname: '测试用户',
    text: '测试弹幕',
    receivedAt: '2026-08-07T00:00:00.000Z',
  }],
};

describe('danmaku contract', () => {
  it('accepts normalized message and status events', () => {
    expect(isDanmakuEvent(messageEvent)).toBe(true);
    expect(isDanmakuEvent({
      type: 'status',
      status: { roomId: '63136', state: 'reconnecting', attempt: 2 },
    })).toBe(true);
  });

  it('rejects malformed or cross-room message batches', () => {
    expect(isDanmakuEvent(null)).toBe(false);
    expect(isDanmakuEvent({ ...messageEvent, roomId: 'abc' })).toBe(false);
    expect(isDanmakuEvent({
      ...messageEvent,
      messages: [{ ...messageEvent.messages[0], roomId: '999' }],
    })).toBe(false);
    expect(isDanmakuEvent({
      type: 'status', status: { roomId: '63136', state: 'reconnecting', attempt: -1 },
    })).toBe(false);
  });
});
```

Extend `tests/ipc-contract.test.ts` channel assertions:

```ts
expect(IPC_CHANNELS.startDanmaku).toBe('danmaku.start');
expect(IPC_CHANNELS.stopDanmaku).toBe('danmaku.stop');
expect(IPC_CHANNELS.danmakuEvent).toBe('danmaku.event');
```

- [ ] **Step 2: Run the tests and confirm the missing contract fails**

Run:

```powershell
npm test -- tests/danmaku-contract.test.ts tests/ipc-contract.test.ts
```

Expected: FAIL because `src/shared/danmaku-contract.ts` and the new channel keys do not exist.

- [ ] **Step 3: Add the normalized types and strict runtime validator**

Create `src/shared/danmaku-contract.ts` with these public types and validators:

```ts
export type DanmakuConnectionState =
  | 'idle'
  | 'connecting'
  | 'connected'
  | 'reconnecting'
  | 'failed'
  | 'platform-blocked';

export type DanmakuErrorCode =
  | 'NETWORK_UNAVAILABLE'
  | 'HANDSHAKE_TIMEOUT'
  | 'PROTOCOL_CHANGED'
  | 'AUTH_REQUIRED'
  | 'RETRY_EXHAUSTED';

export interface DanmakuMessage {
  id: string;
  roomId: string;
  nickname: string;
  text: string;
  receivedAt: string;
}

export interface DanmakuStatus {
  roomId: string;
  state: DanmakuConnectionState;
  attempt?: number;
  errorCode?: DanmakuErrorCode;
}

export type DanmakuEvent =
  | { type: 'messages'; roomId: string; messages: DanmakuMessage[]; dropped: number }
  | { type: 'status'; status: DanmakuStatus };

export interface DanmakuRoomRequest {
  roomId: string;
}

const ROOM_ID = /^\d{1,20}$/;
const CONNECTION_STATES = new Set<DanmakuConnectionState>([
  'idle', 'connecting', 'connected', 'reconnecting', 'failed', 'platform-blocked',
]);
const ERROR_CODES = new Set<DanmakuErrorCode>([
  'NETWORK_UNAVAILABLE', 'HANDSHAKE_TIMEOUT', 'PROTOCOL_CHANGED',
  'AUTH_REQUIRED', 'RETRY_EXHAUSTED',
]);

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null;
}

export function isValidDanmakuRoomRequest(value: unknown): value is DanmakuRoomRequest {
  return isRecord(value) && typeof value.roomId === 'string' && ROOM_ID.test(value.roomId);
}

function isMessage(value: unknown, roomId: string): value is DanmakuMessage {
  if (!isRecord(value)) return false;
  return typeof value.id === 'string' && value.id.length > 0 && value.id.length <= 200
    && value.roomId === roomId
    && typeof value.nickname === 'string' && Array.from(value.nickname).length <= 40
    && typeof value.text === 'string' && Array.from(value.text).length <= 200
    && typeof value.receivedAt === 'string'
    && !Number.isNaN(Date.parse(value.receivedAt));
}

export function isDanmakuEvent(value: unknown): value is DanmakuEvent {
  if (!isRecord(value) || typeof value.type !== 'string') return false;
  if (value.type === 'messages') {
    if (typeof value.roomId !== 'string' || !ROOM_ID.test(value.roomId)) return false;
    return Array.isArray(value.messages) && value.messages.length <= 10
      && value.messages.every((message) => isMessage(message, value.roomId as string))
      && Number.isInteger(value.dropped)
      && Number(value.dropped) >= 0;
  }
  if (value.type !== 'status' || !isRecord(value.status)) return false;
  const status = value.status;
  return typeof status.roomId === 'string'
    && ROOM_ID.test(status.roomId)
    && typeof status.state === 'string'
    && CONNECTION_STATES.has(status.state as DanmakuConnectionState)
    && (status.attempt === undefined
      || (Number.isInteger(status.attempt) && Number(status.attempt) >= 0))
    && (status.errorCode === undefined
      || (typeof status.errorCode === 'string'
        && ERROR_CODES.has(status.errorCode as DanmakuErrorCode)));
}
```

Add to `IPC_CHANNELS` in `src/shared/ipc-contract.ts`:

```ts
startDanmaku: 'danmaku.start',
stopDanmaku: 'danmaku.stop',
danmakuEvent: 'danmaku.event',
```

Extend `IpcError['code']` with `'ROOM_LIMIT'` for a tenth concurrent room.

- [ ] **Step 4: Run the focused contract tests**

Run:

```powershell
npm test -- tests/danmaku-contract.test.ts tests/ipc-contract.test.ts
```

Expected: both files PASS.

- [ ] **Step 5: Record the checkpoint**

Run `npm run typecheck`. Expected: PASS. Do not create a Git repository.

### Task 2: STT and Incremental Binary Framing

**Files:**

- Create: `src/infrastructure/douyu-danmaku/stt.ts`
- Create: `src/infrastructure/douyu-danmaku/protocol.ts`
- Create: `tests/douyu-danmaku-protocol.test.ts`

- [ ] **Step 1: Write failing protocol tests**

Create `tests/douyu-danmaku-protocol.test.ts`:

```ts
import { describe, expect, it } from 'vitest';
import {
  DouyuFrameDecoder,
  encodeDouyuFrame,
} from '../src/infrastructure/douyu-danmaku/protocol';
import {
  parseStt,
  serializeStt,
} from '../src/infrastructure/douyu-danmaku/stt';

describe('Douyu STT', () => {
  it('round-trips escaped at-signs and slashes', () => {
    const encoded = serializeStt({ type: 'chatmsg', txt: 'A@B/C' });
    expect(encoded).toBe('type@=chatmsg/txt@=A@AB@SC/');
    expect(parseStt(encoded)).toEqual({ type: 'chatmsg', txt: 'A@B/C' });
  });

  it('splits each field at its first key separator', () => {
    expect(parseStt('type@=chatmsg/txt@=a@=b/')).toEqual({
      type: 'chatmsg',
      txt: 'a@=b',
    });
  });
});

describe('Douyu binary protocol', () => {
  it('decodes one server frame and validates its repeated length', () => {
    const frame = encodeDouyuFrame('type@=chatmsg/rid@=63136/', 690);
    const decoder = new DouyuFrameDecoder();
    expect(decoder.push(frame)).toEqual(['type@=chatmsg/rid@=63136/']);
  });

  it('preserves half a frame and emits coalesced frames in order', () => {
    const first = encodeDouyuFrame('type@=loginres/', 690);
    const second = encodeDouyuFrame('type@=chatmsg/txt@=hi/', 690);
    const joined = new Uint8Array(first.byteLength + second.byteLength);
    joined.set(first, 0);
    joined.set(second, first.byteLength);
    const decoder = new DouyuFrameDecoder();

    expect(decoder.push(joined.slice(0, 7))).toEqual([]);
    expect(decoder.push(joined.slice(7))).toEqual([
      'type@=loginres/',
      'type@=chatmsg/txt@=hi/',
    ]);
  });

  it('rejects mismatched lengths and oversized frames', () => {
    const mismatched = encodeDouyuFrame('type@=loginres/', 690);
    new DataView(mismatched.buffer).setUint32(4, 99, true);
    expect(() => new DouyuFrameDecoder().push(mismatched)).toThrow('length');

    const oversized = new Uint8Array(12);
    new DataView(oversized.buffer).setUint32(0, 1024 * 1024 + 1, true);
    expect(() => new DouyuFrameDecoder().push(oversized)).toThrow('size');
  });

  it('rejects an unexpected server protocol type', () => {
    const clientFrame = encodeDouyuFrame('type@=loginreq/', 689);
    expect(() => new DouyuFrameDecoder().push(clientFrame)).toThrow('protocol');
  });
});
```

- [ ] **Step 2: Confirm the protocol tests fail**

Run `npm test -- tests/douyu-danmaku-protocol.test.ts`.

Expected: FAIL because both modules are missing.

- [ ] **Step 3: Implement flat STT**

Create `src/infrastructure/douyu-danmaku/stt.ts`:

```ts
export function escapeStt(value: string): string {
  return value.replace(/@/g, '@A').replace(/\//g, '@S');
}

export function unescapeStt(value: string): string {
  return value.replace(/@S/g, '/').replace(/@A/g, '@');
}

export function serializeStt(value: Record<string, string | number>): string {
  return Object.entries(value)
    .map(([key, entry]) => `${escapeStt(key)}@=${escapeStt(String(entry))}/`)
    .join('');
}

export function parseStt(raw: string): Record<string, string> {
  const result: Record<string, string> = {};
  for (const part of raw.split('/')) {
    if (!part) continue;
    const separator = part.indexOf('@=');
    if (separator < 1) continue;
    const key = unescapeStt(part.slice(0, separator));
    result[key] = unescapeStt(part.slice(separator + 2));
  }
  return result;
}
```

- [ ] **Step 4: Implement the bounded incremental decoder**

Create `src/infrastructure/douyu-danmaku/protocol.ts` with these constants and behavior:

```ts
const HEADER_BYTES = 12;
const LENGTH_AFTER_FIRST_FIELD = 8;
const CLIENT_PROTOCOL = 689;
const SERVER_PROTOCOL = 690;
const MAX_FRAME_BYTES = 1024 * 1024;

function concat(left: Uint8Array, right: Uint8Array): Uint8Array {
  const result = new Uint8Array(left.byteLength + right.byteLength);
  result.set(left, 0);
  result.set(right, left.byteLength);
  return result;
}

export function encodeDouyuFrame(text: string, protocol = CLIENT_PROTOCOL): Uint8Array {
  const payload = new TextEncoder().encode(text);
  const bodyLength = LENGTH_AFTER_FIRST_FIELD + payload.byteLength + 1;
  const frame = new Uint8Array(bodyLength + 4);
  const view = new DataView(frame.buffer);
  view.setUint32(0, bodyLength, true);
  view.setUint32(4, bodyLength, true);
  view.setUint16(8, protocol, true);
  frame.set(payload, HEADER_BYTES);
  frame[frame.byteLength - 1] = 0;
  return frame;
}

export class DouyuFrameDecoder {
  private pending = new Uint8Array(0);
  private readonly decoder = new TextDecoder('utf-8', { fatal: true });

  push(chunk: ArrayBuffer | Uint8Array): string[] {
    const incoming = chunk instanceof Uint8Array ? chunk : new Uint8Array(chunk);
    this.pending = concat(this.pending, incoming);
    const messages: string[] = [];

    while (this.pending.byteLength >= 4) {
      const view = new DataView(
        this.pending.buffer,
        this.pending.byteOffset,
        this.pending.byteLength,
      );
      const bodyLength = view.getUint32(0, true);
      const totalLength = bodyLength + 4;
      if (bodyLength < LENGTH_AFTER_FIRST_FIELD + 1) throw new Error('Invalid frame length');
      if (totalLength > MAX_FRAME_BYTES) throw new Error('Frame size exceeds limit');
      if (this.pending.byteLength < totalLength) break;
      if (view.getUint32(4, true) !== bodyLength) throw new Error('Repeated length mismatch');
      if (view.getUint16(8, true) !== SERVER_PROTOCOL) throw new Error('Unexpected protocol type');
      if (this.pending[totalLength - 1] !== 0) throw new Error('Missing frame terminator');

      messages.push(this.decoder.decode(this.pending.slice(HEADER_BYTES, totalLength - 1)));
      this.pending = this.pending.slice(totalLength);
    }

    return messages;
  }
}
```

- [ ] **Step 5: Run focused protocol verification**

Run:

```powershell
npm test -- tests/douyu-danmaku-protocol.test.ts
npm run typecheck
```

Expected: PASS. Do not create a Git repository.

### Task 3: WebSocket Transport Adapter

**Files:**

- Create: `src/infrastructure/douyu-danmaku/socket.ts`
- Create: `tests/douyu-danmaku-socket.test.ts`
- Modify: `package.json`
- Modify: `package-lock.json`
- Modify: `vite.main.config.ts`

- [ ] **Step 1: Install pinned transport packages**

Run:

```powershell
npm install ws@8.21.2
npm install --save-dev @types/ws@8.18.1
```

Expected: `package.json` lists `ws` under dependencies and `@types/ws` under devDependencies; npm reports zero known vulnerabilities or reports any advisory for review before continuing.

- [ ] **Step 2: Write the failing data-normalization test**

Create `tests/douyu-danmaku-socket.test.ts`:

```ts
import { Buffer } from 'node:buffer';
import { describe, expect, it } from 'vitest';
import { rawDataToArrayBuffer } from '../src/infrastructure/douyu-danmaku/socket';

describe('ws transport normalization', () => {
  it('copies Buffer and fragmented Buffer data into exact ArrayBuffers', () => {
    expect([...new Uint8Array(rawDataToArrayBuffer(Buffer.from([1, 2, 3])))]).toEqual([1, 2, 3]);
    expect([...new Uint8Array(rawDataToArrayBuffer([
      Buffer.from([4, 5]),
      Buffer.from([6]),
    ])))]).toEqual([4, 5, 6]);
  });
});
```

- [ ] **Step 3: Confirm the transport test fails**

Run `npm test -- tests/douyu-danmaku-socket.test.ts`.

Expected: FAIL because `socket.ts` is missing.

- [ ] **Step 4: Add the narrow `ws` wrapper**

Create `src/infrastructure/douyu-danmaku/socket.ts` with this public boundary:

```ts
import { Buffer } from 'node:buffer';
import type { ClientRequest, IncomingMessage } from 'node:http';
import WebSocket, { type RawData } from 'ws';

export interface DanmakuSocketHandlers {
  open(): void;
  message(data: ArrayBuffer): void;
  error(error: unknown): void;
  close(code: number, reason: string): void;
  unexpectedResponse(statusCode: number): void;
}

export interface DanmakuSocket {
  send(data: Uint8Array): void;
  close(): void;
  dispose(): void;
}

export type DanmakuSocketFactory = (
  url: string,
  handlers: DanmakuSocketHandlers,
) => DanmakuSocket;

export function rawDataToArrayBuffer(data: RawData): ArrayBuffer {
  const buffer = Array.isArray(data) ? Buffer.concat(data) : Buffer.from(data);
  return buffer.buffer.slice(
    buffer.byteOffset,
    buffer.byteOffset + buffer.byteLength,
  ) as ArrayBuffer;
}

export const createWsDanmakuSocket: DanmakuSocketFactory = (url, handlers) => {
  const socket = new WebSocket(url);
  socket.binaryType = 'arraybuffer';

  const onOpen = () => handlers.open();
  const onMessage = (data: RawData) => handlers.message(rawDataToArrayBuffer(data));
  const onError = (error: Error) => handlers.error(error);
  const onClose = (code: number, reason: Buffer) => handlers.close(code, reason.toString('utf8'));
  const onUnexpectedResponse = (_request: ClientRequest, response: IncomingMessage) => {
    handlers.unexpectedResponse(response.statusCode ?? 0);
  };

  socket.on('open', onOpen);
  socket.on('message', onMessage);
  socket.on('error', onError);
  socket.on('close', onClose);
  socket.on('unexpected-response', onUnexpectedResponse);

  return {
    send(data) {
      if (socket.readyState !== WebSocket.OPEN) throw new Error('Socket is not open');
      socket.send(data);
    },
    close() {
      if (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING) {
        socket.close();
      }
    },
    dispose() {
      socket.off('open', onOpen);
      socket.off('message', onMessage);
      socket.off('error', onError);
      socket.off('close', onClose);
      socket.off('unexpected-response', onUnexpectedResponse);
    },
  };
};
```

- [ ] **Step 5: Keep `ws` external in the Main build**

Append `'ws'` to `build.rollupOptions.external` in `vite.main.config.ts`:

```ts
external: ['electron', 'node:buffer', 'node:path', 'node:url', 'ws'],
```

- [ ] **Step 6: Run the transport checkpoint**

Run:

```powershell
npm test -- tests/douyu-danmaku-socket.test.ts
npm run typecheck
npm run build:main
```

Expected: PASS and `dist/main/main.js` keeps `ws` as an external import. Do not create a Git repository.

### Task 4: One-Room Danmaku Client

**Files:**

- Create: `src/infrastructure/douyu-danmaku/client.ts`
- Create: `tests/douyu-danmaku-client.test.ts`

- [ ] **Step 1: Write failing state-machine tests with a fake socket**

Create this fake `DanmakuSocketFactory` and frame helpers in `tests/douyu-danmaku-client.test.ts`:

```ts
function createFakeSocketFactory() {
  const sockets: Array<{
    url: string;
    handlers: DanmakuSocketHandlers;
    sent: Uint8Array[];
    close: ReturnType<typeof vi.fn>;
    dispose: ReturnType<typeof vi.fn>;
  }> = [];
  const factory: DanmakuSocketFactory = (url, handlers) => {
    const socket = { url, handlers, sent: [], close: vi.fn(), dispose: vi.fn() };
    sockets.push(socket);
    return {
      send: (data) => socket.sent.push(data),
      close: socket.close,
      dispose: socket.dispose,
    };
  };
  return {
    factory,
    sockets,
    get latest() { return sockets[sockets.length - 1]; },
    get urls() { return sockets.map((socket) => socket.url); },
  };
}

function decodeClientFrames(frames: Uint8Array[]): string[] {
  return frames.map((frame) => {
    const view = new DataView(frame.buffer, frame.byteOffset, frame.byteLength);
    expect(view.getUint32(0, true)).toBe(view.getUint32(4, true));
    expect(view.getUint16(8, true)).toBe(689);
    return new TextDecoder().decode(frame.slice(12, frame.byteLength - 1));
  });
}

function serverFrame(value: Record<string, string>): ArrayBuffer {
  const frame = encodeDouyuFrame(serializeStt(value), 690);
  return frame.buffer.slice(
    frame.byteOffset,
    frame.byteOffset + frame.byteLength,
  ) as ArrayBuffer;
}
```

Import `DanmakuSocketHandlers`, `DanmakuSocketFactory`, `encodeDouyuFrame`, and `serializeStt`, then add these tests:

```ts
it('sends anonymous login, group join, heartbeat, and logout only', () => {
  vi.useFakeTimers();
  const fake = createFakeSocketFactory();
  const events: DouyuClientEvent[] = [];
  const random = vi.fn().mockReturnValueOnce(0).mockReturnValue(0.5);
  const client = createDouyuDanmakuClient('63136', events.push.bind(events), {
    socketFactory: fake.factory,
    random,
  });

  client.start();
  fake.latest.handlers.open();
  expect(decodeClientFrames(fake.latest.sent)).toEqual([
    'type@=loginreq/roomid@=63136/',
    'type@=joingroup/rid@=63136/gid@=-9999/',
  ]);

  vi.advanceTimersByTime(45_000);
  expect(decodeClientFrames(fake.latest.sent).at(-1)).toBe('type@=mrkl/');
  client.stop();
  expect(decodeClientFrames(fake.latest.sent).at(-1)).toBe('type@=logout/');
});

it('emits only same-room chatmsg messages', () => {
  const fake = createFakeSocketFactory();
  const events: DouyuClientEvent[] = [];
  const client = createDouyuDanmakuClient('63136', events.push.bind(events), {
    socketFactory: fake.factory,
    random: () => 0,
  });
  client.start();
  fake.latest.handlers.open();
  fake.latest.handlers.message(serverFrame({
    type: 'chatmsg', rid: '63136', cid: '1', nn: '用户', txt: '你好',
  }));
  fake.latest.handlers.message(serverFrame({
    type: 'dgb', rid: '63136', gfid: '20001',
  }));
  fake.latest.handlers.message(serverFrame({
    type: 'chatmsg', rid: '999', cid: '2', nn: '其他房间', txt: '忽略',
  }));

  expect(events.filter((event) => event.type === 'chat')).toEqual([{
    type: 'chat',
    message: { type: 'chatmsg', rid: '63136', cid: '1', nn: '用户', txt: '你好' },
  }]);
});

it('rotates endpoints with bounded backoff and stops after six failures', () => {
  vi.useFakeTimers();
  const fake = createFakeSocketFactory();
  const events: DouyuClientEvent[] = [];
  const random = vi.fn().mockReturnValueOnce(0).mockReturnValue(0.5);
  const client = createDouyuDanmakuClient('63136', events.push.bind(events), {
    socketFactory: fake.factory,
    random,
  });
  client.start();

  for (const delay of [1_000, 2_000, 4_000, 8_000, 15_000, 15_000]) {
    fake.latest.handlers.close(1006, '');
    vi.advanceTimersByTime(delay);
  }

  expect(fake.urls).toEqual([
    'wss://danmuproxy.douyu.com:8501/',
    'wss://danmuproxy.douyu.com:8502/',
    'wss://danmuproxy.douyu.com:8503/',
    'wss://danmuproxy.douyu.com:8504/',
    'wss://danmuproxy.douyu.com:8505/',
    'wss://danmuproxy.douyu.com:8506/',
  ]);
  expect(events.at(-1)).toEqual({
    type: 'status',
    status: { roomId: '63136', state: 'failed', attempt: 6, errorCode: 'RETRY_EXHAUSTED' },
  });
});
```

Add the explicit authentication and stop-during-backoff cases:

```ts
it('marks platform blocked only after every endpoint rejects authentication', () => {
  vi.useFakeTimers();
  const fake = createFakeSocketFactory();
  const events: DouyuClientEvent[] = [];
  const random = vi.fn().mockReturnValueOnce(0).mockReturnValue(0.5);
  const client = createDouyuDanmakuClient('63136', events.push.bind(events), {
    socketFactory: fake.factory,
    random,
  });
  client.start();

  for (let index = 0; index < 6; index += 1) {
    fake.latest.handlers.unexpectedResponse(401);
    if (index < 5) vi.runOnlyPendingTimers();
  }

  expect(events.at(-1)).toEqual({
    type: 'status',
    status: {
      roomId: '63136',
      state: 'platform-blocked',
      attempt: 6,
      errorCode: 'AUTH_REQUIRED',
    },
  });
});

it('does not reconnect after a manual stop during backoff', () => {
  vi.useFakeTimers();
  const fake = createFakeSocketFactory();
  const client = createDouyuDanmakuClient('63136', () => {}, {
    socketFactory: fake.factory,
    random: vi.fn().mockReturnValueOnce(0).mockReturnValue(0.5),
  });
  client.start();
  fake.latest.handlers.close(1006, '');
  client.stop();
  vi.runAllTimers();
  expect(fake.urls).toHaveLength(1);
});
```

- [ ] **Step 2: Run the client tests and confirm failure**

Run `npm test -- tests/douyu-danmaku-client.test.ts`.

Expected: FAIL because the client module is missing.

- [ ] **Step 3: Implement the explicit client API and constants**

Create `src/infrastructure/douyu-danmaku/client.ts` with this public API:

```ts
import type { DanmakuStatus } from '../../shared/danmaku-contract';
import { DouyuFrameDecoder, encodeDouyuFrame } from './protocol';
import { parseStt, serializeStt } from './stt';
import { createWsDanmakuSocket, type DanmakuSocket, type DanmakuSocketFactory } from './socket';

export interface RawChatMessage {
  type: 'chatmsg';
  rid: string;
  cid?: string;
  nn?: string;
  txt?: string;
}

export type DouyuClientEvent =
  | { type: 'chat'; message: RawChatMessage }
  | { type: 'status'; status: DanmakuStatus };

export interface DouyuDanmakuClient {
  start(): void;
  stop(): void;
}

interface ClientDependencies {
  socketFactory: DanmakuSocketFactory;
  random: () => number;
  setTimeout: typeof globalThis.setTimeout;
  clearTimeout: typeof globalThis.clearTimeout;
  setInterval: typeof globalThis.setInterval;
  clearInterval: typeof globalThis.clearInterval;
}

const ENDPOINTS = [8501, 8502, 8503, 8504, 8505, 8506]
  .map((port) => `wss://danmuproxy.douyu.com:${port}/`);
const RETRY_DELAYS = [1_000, 2_000, 4_000, 8_000, 15_000, 15_000];
const HEARTBEAT_MS = 45_000;
const HANDSHAKE_MS = 10_000;
const STABLE_MS = 60_000;
```

Implement `createDouyuDanmakuClient(roomId, emit, overrides)` with these exact transitions:

- Public `start()` is idempotent while a socket or reconnect timer exists. From an idle, failed, or blocked state it resets manual-stop and failure state, chooses `Math.floor(random() * 6)` as the first endpoint, emits `connecting`, then calls a private `connectNext()`. Scheduled retries call `connectNext()` directly so they never reset counters.
- `open` sends serialized `loginreq` and `joingroup`, starts the 45-second heartbeat, 10-second handshake timer, and 60-second stable timer.
- `message` feeds one persistent `DouyuFrameDecoder`. Before marking success, join only the decoded `msg`, `message`, `reason`, and `error` values; when `type` is `error` or `loginres` and those values match `/auth|login|token|sign|认证|登录|签名/i`, emit `platform-blocked` with `AUTH_REQUIRED`. A `loginres` without authentication evidence, `setmsggroup`, or same-room `chatmsg` clears the handshake timer and emits `connected` once. Same-room `chatmsg` emits a `chat` event; all other message types are ignored.
- The first same-room `chatmsg` or the 60-second stable timer resets the failure count and denied-endpoint set.
- `error`, `close`, handshake timeout, UTF-8 error, frame error, and STT error call one idempotent `failCurrent(errorCode)` path. That path clears timers, disposes and closes the current socket, increments the failure count, emits `reconnecting`, and schedules the indexed delay with ±20% jitter.
- `unexpectedResponse(401|403)` records the current endpoint. Six denied endpoints emit `platform-blocked` with `AUTH_REQUIRED` before the generic retry-exhaustion check and stop. Other HTTP statuses use `NETWORK_UNAVAILABLE`.
- Close code `1008` with a reason matching `/auth|login|token|sign|认证|登录|签名/i` emits `platform-blocked` with `AUTH_REQUIRED`.
- Failure count 6 emits `failed` with `RETRY_EXHAUSTED` and schedules nothing.
- `stop()` clears all timers, sends `logout` only when the socket reached open state, disposes and closes it, and emits `idle`. Delayed callbacks check a generation counter so stale callbacks cannot reopen a stopped session.

Keep helper functions private except `createDouyuDanmakuClient`. Use `createWsDanmakuSocket` and real timer functions as defaults. Never log payloads or errors.

- [ ] **Step 4: Make deterministic jitter testable**

Use this delay calculation in the client:

```ts
function withJitter(base: number, random: () => number): number {
  return Math.round(base * (0.8 + random() * 0.4));
}
```

The client's first random call selects the initial endpoint. Later calls calculate retry jitter. Tests that require endpoint `8501` and exact base delays must use `vi.fn().mockReturnValueOnce(0).mockReturnValue(0.5)`.

- [ ] **Step 5: Run the state-machine checkpoint**

Run:

```powershell
npm test -- tests/douyu-danmaku-client.test.ts tests/douyu-danmaku-protocol.test.ts
npm run typecheck
```

Expected: PASS, no open timers reported by Vitest. Do not create a Git repository.

### Task 5: Main-Process Session Manager

**Files:**

- Create: `src/main/danmaku-session-manager.ts`
- Create: `tests/danmaku-session-manager.test.ts`

- [ ] **Step 1: Write failing ownership and batching tests**

Create `tests/danmaku-session-manager.test.ts` with this harness and timer cleanup:

```ts
const managers: DanmakuSessionManager[] = [];

beforeEach(() => vi.useFakeTimers());
afterEach(() => {
  for (const manager of managers.splice(0)) manager.stopAll();
  vi.clearAllTimers();
  vi.useRealTimers();
});

function createManagerHarness() {
  const clients: Array<{
    start: ReturnType<typeof vi.fn>;
    stop: ReturnType<typeof vi.fn>;
    emit: (event: DouyuClientEvent) => void;
  }> = [];
  const clientFactory: DanmakuClientFactory = (_roomId, emit) => {
    const client = { start: vi.fn(), stop: vi.fn(), emit };
    clients.push(client);
    return client;
  };
  const createOwner = (id: number) => ({
    id,
    destroyed: false,
    events: [] as DanmakuEvent[],
    isDestroyed() { return this.destroyed; },
    send(channel: string, event: DanmakuEvent) {
      expect(channel).toBe(IPC_CHANNELS.danmakuEvent);
      this.events.push(event);
    },
  });
  const manager = createDanmakuSessionManager(clientFactory);
  managers.push(manager);
  return {
    manager,
    clients,
    ownerA: createOwner(1),
    ownerB: createOwner(2),
  };
}
```

Import `beforeEach`, `afterEach`, and all referenced contract/factory types. Cover these cases:

```ts
it('shares one room client across repeated starts and closes after the last owner stops', () => {
  const harness = createManagerHarness();
  expect(harness.manager.start(harness.ownerA, '63136')).toBe('started');
  expect(harness.manager.start(harness.ownerA, '63136')).toBe('existing');
  expect(harness.manager.start(harness.ownerB, '63136')).toBe('existing');
  expect(harness.clients).toHaveLength(1);

  harness.manager.stop(harness.ownerA.id, '63136');
  expect(harness.clients[0].stop).not.toHaveBeenCalled();
  harness.manager.stop(harness.ownerB.id, '63136');
  expect(harness.clients[0].stop).toHaveBeenCalledOnce();
});

it('normalizes, sanitizes, deduplicates, and batches same-room chat', () => {
  vi.useFakeTimers();
  const harness = createManagerHarness();
  harness.manager.start(harness.ownerA, '63136');
  harness.clients[0].emit({
    type: 'chat',
    message: { type: 'chatmsg', rid: '63136', cid: '1', nn: '用户\u0000', txt: '第一行\n第二行' },
  });
  harness.clients[0].emit({
    type: 'chat',
    message: { type: 'chatmsg', rid: '63136', cid: '1', nn: '用户', txt: '重复' },
  });
  vi.advanceTimersByTime(250);

  expect(harness.ownerA.events).toEqual([expect.objectContaining({
    type: 'messages',
    roomId: '63136',
    dropped: 0,
    messages: [expect.objectContaining({
      id: '1', nickname: '用户', text: '第一行 第二行',
    })],
  })]);
});
```

Add these concrete manager cases:

```ts
it('rejects a tenth distinct room', () => {
  const harness = createManagerHarness();
  for (let roomId = 1; roomId <= 9; roomId += 1) {
    expect(harness.manager.start(harness.ownerA, String(roomId))).toBe('started');
  }
  expect(harness.manager.start(harness.ownerA, '10')).toBe('limit');
});

it('keeps the newest one hundred messages and flushes ten at a time', () => {
  vi.useFakeTimers();
  const harness = createManagerHarness();
  harness.manager.start(harness.ownerA, '63136');
  for (let index = 1; index <= 120; index += 1) {
    harness.clients[0].emit({
      type: 'chat',
      message: {
        type: 'chatmsg', rid: '63136', cid: String(index), nn: '用户', txt: `消息${index}`,
      },
    });
  }
  vi.advanceTimersByTime(250);
  const batch = harness.ownerA.events.at(-1);
  expect(batch).toEqual(expect.objectContaining({ type: 'messages', dropped: 20 }));
  expect(batch?.type === 'messages' && batch.messages.map((message) => message.id))
    .toEqual(['21', '22', '23', '24', '25', '26', '27', '28', '29', '30']);
});

it('sends status immediately and removes destroyed owners', () => {
  const harness = createManagerHarness();
  harness.manager.start(harness.ownerA, '63136');
  harness.clients[0].emit({
    type: 'status', status: { roomId: '63136', state: 'connected' },
  });
  expect(harness.ownerA.events.at(-1)).toEqual({
    type: 'status', status: { roomId: '63136', state: 'connected' },
  });
  harness.ownerA.destroyed = true;
  harness.clients[0].emit({
    type: 'status', status: { roomId: '63136', state: 'reconnecting', attempt: 1 },
  });
  expect(harness.ownerA.events).toHaveLength(1);
});

it('stops every session owned only by one destroyed window', () => {
  const harness = createManagerHarness();
  harness.manager.start(harness.ownerA, '101');
  harness.manager.start(harness.ownerA, '202');
  harness.manager.stopOwner(harness.ownerA.id);
  expect(harness.clients.every((client) => client.stop.mock.calls.length === 1)).toBe(true);
});

it('restarts an existing failed session without creating a second client', () => {
  const harness = createManagerHarness();
  harness.manager.start(harness.ownerA, '63136');
  harness.clients[0].emit({
    type: 'status',
    status: { roomId: '63136', state: 'failed', attempt: 6, errorCode: 'RETRY_EXHAUSTED' },
  });
  expect(harness.manager.start(harness.ownerA, '63136')).toBe('existing');
  expect(harness.clients).toHaveLength(1);
  expect(harness.clients[0].start).toHaveBeenCalledTimes(2);
});
```

- [ ] **Step 2: Confirm the manager tests fail**

Run `npm test -- tests/danmaku-session-manager.test.ts`.

Expected: FAIL because the manager is missing.

- [ ] **Step 3: Implement the manager boundary**

Create `src/main/danmaku-session-manager.ts` with these interfaces:

```ts
import type { DanmakuEvent, DanmakuMessage } from '../shared/danmaku-contract';
import type { DouyuClientEvent, DouyuDanmakuClient } from '../infrastructure/douyu-danmaku/client';

export interface DanmakuEventTarget {
  id: number;
  isDestroyed(): boolean;
  send(channel: string, event: DanmakuEvent): void;
}

export type DanmakuClientFactory = (
  roomId: string,
  emit: (event: DouyuClientEvent) => void,
) => DouyuDanmakuClient;

export interface DanmakuSessionManager {
  start(owner: DanmakuEventTarget, roomId: string): 'started' | 'existing' | 'limit';
  stop(ownerId: number, roomId: string): void;
  stopOwner(ownerId: number): void;
  stopAll(): void;
}

export function createDanmakuSessionManager(
  clientFactory: DanmakuClientFactory,
  now: () => Date = () => new Date(),
): DanmakuSessionManager
```

Use a `Map<string, Session>` where each session contains the client, owners keyed by WebContents ID, pending messages, a 200-ID dedupe set plus FIFO order, a fallback sequence, dropped count, and last status. Enforce these constants:

```ts
const MAX_ROOMS = 9;
const MAX_PENDING = 100;
const MAX_SEEN_IDS = 200;
const FLUSH_MS = 250;
const MAX_BATCH = 10;
```

Normalize messages with a pure helper exported for tests:

```ts
export function normalizeChatMessage(
  roomId: string,
  raw: { cid?: string; nn?: string; txt?: string },
  fallbackId: string,
  now: Date,
): DanmakuMessage | null
```

Use this sanitization before truncation:

```ts
function sanitizeText(value: string): string {
  return value
    .replace(/[\u0000-\u0008\u000b\u000c\u000e-\u001f\u007f]/g, '')
    .replace(/[\r\n]+/g, ' ')
    .trim();
}

function truncateUnicode(value: string, limit: number): string {
  return Array.from(value).slice(0, limit).join('');
}
```

The helper truncates nicknames to 40 Unicode code points and text to 200 code points, uses `匿名用户` for an empty nickname, and returns `null` for empty text. This preserves surrogate pairs.

Start one 250ms interval when the manager is created. Each tick sends at most 10 pending messages per session to every live owner through `IPC_CHANNELS.danmakuEvent`. Remove destroyed owners before sending. Stop and delete a session when no owners remain. Calling `start` for an existing session adds the owner idempotently; if the last status is `failed`, it calls the existing client's `start()` to reset retry state without allocating another client. A `platform-blocked` session does not restart through this path. `stopAll()` clears the interval and stops every client.

- [ ] **Step 4: Run the manager checkpoint**

Run:

```powershell
npm test -- tests/danmaku-session-manager.test.ts
npm run typecheck
```

Expected: PASS. Inspect fake-timer cleanup and confirm `stopAll()` leaves no interval. Do not create a Git repository.

### Task 6: IPC, Preload, and Main Lifecycle

**Files:**

- Modify: `src/main/ipc-handlers.ts`
- Modify: `src/main/main.ts`
- Modify: `src/preload/bridge.ts`
- Modify: `src/preload/preload.ts`
- Modify: `tests/ipc-handlers.test.ts`
- Modify: `tests/preload-bridge.test.ts`

- [ ] **Step 1: Extend failing IPC tests**

Update the fake IPC event in `tests/ipc-handlers.test.ts` to include a sender target. Pass a fake `DanmakuSessionManager` to `registerIpcHandlers`. Add tests asserting:

```ts
await expect(handlers.get(IPC_CHANNELS.startDanmaku)?.(
  { sender }, { roomId: '63136' },
)).resolves.toEqual({ ok: true, data: undefined });
expect(manager.start).toHaveBeenCalledWith(sender, '63136');

await expect(handlers.get(IPC_CHANNELS.stopDanmaku)?.(
  { sender }, { roomId: '63136' },
)).resolves.toEqual({ ok: true, data: undefined });
expect(manager.stop).toHaveBeenCalledWith(sender.id, '63136');
```

Add malformed room-ID assertions and a manager `limit` result mapping to:

```ts
{
  ok: false,
  error: { code: 'ROOM_LIMIT', message: '最多同时连接 9 个直播间弹幕', retryable: false },
}
```

Use this exact invalid-input assertion for both start and stop:

```ts
await expect(handlers.get(IPC_CHANNELS.startDanmaku)?.(
  { sender }, { roomId: 'abc' },
)).resolves.toEqual({
  ok: false,
  error: { code: 'INVALID_INPUT', message: '请输入有效的直播间号', retryable: false },
});
expect(manager.start).not.toHaveBeenCalled();
```

- [ ] **Step 2: Extend failing Preload tests**

Expand `IpcRendererLike` test doubles with `on` and `removeListener`. Add:

```ts
it('subscribes only to normalized danmaku events and returns an exact unsubscriber', () => {
  const listeners = new Map<string, (event: unknown, payload: unknown) => void>();
  const removed: string[] = [];
  const api = createAppApi({
    invoke: async () => ok(undefined),
    on(channel, listener) { listeners.set(channel, listener); },
    removeListener(channel, listener) {
      if (listeners.get(channel) === listener) listeners.delete(channel);
      removed.push(channel);
    },
  });
  const received: unknown[] = [];
  const unsubscribe = api.onDanmakuEvent((event) => received.push(event));

  listeners.get(IPC_CHANNELS.danmakuEvent)?.({}, {
    type: 'status', status: { roomId: '63136', state: 'connected' },
  });
  listeners.get(IPC_CHANNELS.danmakuEvent)?.({}, { type: 'messages', roomId: 'abc' });
  unsubscribe();

  expect(received).toHaveLength(1);
  expect(removed).toEqual([IPC_CHANNELS.danmakuEvent]);
});
```

Update the exact API key expectation to:

```ts
[
  'searchRooms',
  'getStreamAvailability',
  'startDanmaku',
  'stopDanmaku',
  'onDanmakuEvent',
  'ping',
]
```

- [ ] **Step 3: Run IPC tests and confirm failure**

Run `npm test -- tests/ipc-handlers.test.ts tests/preload-bridge.test.ts`.

Expected: FAIL on missing handlers and API methods.

- [ ] **Step 4: Add typed handlers**

Change the `registerIpcHandlers` signature to:

```ts
export function registerIpcHandlers(
  ipcMain: IpcMainLike,
  adapter: DouyuAdapter,
  danmakuManager: DanmakuSessionManager,
): void
```

Use an `IpcEventLike` with `sender: DanmakuEventTarget`. Register `startDanmaku` and `stopDanmaku`, validate both through `isValidDanmakuRoomRequest`, and return the fixed room-limit error shown in Step 1. Keep all existing search, availability, and ping behavior unchanged.

- [ ] **Step 5: Extend the bounded Preload bridge**

Add to `IpcRendererLike`:

```ts
on(channel: string, listener: (event: unknown, payload: unknown) => void): void;
removeListener(channel: string, listener: (event: unknown, payload: unknown) => void): void;
```

Add to `AppApi`:

```ts
startDanmaku(roomId: string): Promise<IpcResult<void>>;
stopDanmaku(roomId: string): Promise<IpcResult<void>>;
onDanmakuEvent(listener: (event: DanmakuEvent) => void): () => void;
```

Implement the subscription with a wrapper that calls `isDanmakuEvent(payload)` before invoking the Renderer listener. Remove the same wrapper in the returned function. Update `src/preload/preload.ts` to pass only `invoke`, `on`, and `removeListener`; do not expose the raw Electron object.

- [ ] **Step 6: Wire Main ownership and application cleanup**

In `src/main/main.ts`:

1. Create one manager after `app.whenReady()` using a factory that calls `createDouyuDanmakuClient(roomId, emit)`.
2. Pass it to `registerIpcHandlers`.
3. Capture `window.webContents.id` when creating each window and call `manager.stopOwner(ownerId)` in the window's `closed` handler.
4. Call `manager.stopAll()` from `app.on('before-quit')`.

Keep `contextIsolation`, sandboxing, `nodeIntegration: false`, window-open denial, and playback behavior unchanged.

- [ ] **Step 7: Run the Main/Preload checkpoint**

Run:

```powershell
npm test -- tests/ipc-handlers.test.ts tests/preload-bridge.test.ts tests/main-config.test.ts
npm run typecheck
npm run build:main
npm run build:preload
```

Expected: PASS. Search `dist/preload/preload.cjs` and confirm it does not expose generic `invoke`, `on`, `ipcRenderer`, WebSocket, or Node globals as properties of `window.appApi`. Do not create a Git repository.

### Task 7: Renderer Danmaku Sources

**Files:**

- Create: `src/infrastructure/renderer-danmaku-source.ts`
- Create: `src/infrastructure/mock-danmaku-source.ts`
- Create: `tests/renderer-danmaku-source.test.ts`
- Read without modification: `src/renderer/data/mock-danmaku.ts`

- [ ] **Step 1: Write failing source tests**

Create `tests/renderer-danmaku-source.test.ts`:

```ts
import { describe, expect, it, vi } from 'vitest';
import { ok } from '../src/shared/ipc-contract';
import { createRendererDanmakuSource } from '../src/infrastructure/renderer-danmaku-source';

it('delegates start, stop, and subscription to the bounded AppApi', async () => {
  const unsubscribe = vi.fn();
  const api = {
    startDanmaku: vi.fn(async () => ok(undefined)),
    stopDanmaku: vi.fn(async () => ok(undefined)),
    onDanmakuEvent: vi.fn(() => unsubscribe),
  };
  const source = createRendererDanmakuSource(api);
  const listener = vi.fn();

  await source.start('63136');
  await source.stop('63136');
  expect(source.subscribe(listener)).toBe(unsubscribe);
  expect(api.startDanmaku).toHaveBeenCalledWith('63136');
  expect(api.stopDanmaku).toHaveBeenCalledWith('63136');
  expect(api.onDanmakuEvent).toHaveBeenCalledWith(listener);
});

it('throws only the sanitized IPC message when start fails', async () => {
  const source = createRendererDanmakuSource({
    startDanmaku: async () => ({
      ok: false,
      error: { code: 'UNKNOWN', message: '弹幕连接失败', retryable: true },
    }),
    stopDanmaku: async () => ok(undefined),
    onDanmakuEvent: () => () => {},
  });
  await expect(source.start('63136')).rejects.toThrow('弹幕连接失败');
});
```

Add the Mock lifecycle test:

```ts
it('runs one browser Mock timer per room and clears it on stop', async () => {
  vi.useFakeTimers();
  const source = createMockDanmakuSource();
  const events: DanmakuEvent[] = [];
  const unsubscribe = source.subscribe((event) => events.push(event));
  await source.start('63136');
  await source.start('63136');
  expect(events[0]).toEqual({
    type: 'status', status: { roomId: '63136', state: 'connected' },
  });
  vi.advanceTimersByTime(1_250);
  expect(events.filter((event) => event.type === 'messages')).toHaveLength(1);
  await source.stop('63136');
  const eventCount = events.length;
  vi.advanceTimersByTime(5_000);
  expect(events).toHaveLength(eventCount);
  unsubscribe();
});
```

- [ ] **Step 2: Confirm the source tests fail**

Run `npm test -- tests/renderer-danmaku-source.test.ts`.

Expected: FAIL because both source modules are missing.

- [ ] **Step 3: Define one source interface and real adapter**

Create `src/infrastructure/renderer-danmaku-source.ts`:

```ts
import type { AppApi } from '../preload/bridge';
import type { DanmakuEvent } from '../shared/danmaku-contract';

export interface DanmakuSource {
  start(roomId: string): Promise<void>;
  stop(roomId: string): Promise<void>;
  subscribe(listener: (event: DanmakuEvent) => void): () => void;
}

type DanmakuAppApi = Pick<
  AppApi,
  'startDanmaku' | 'stopDanmaku' | 'onDanmakuEvent'
>;

export function createRendererDanmakuSource(api: DanmakuAppApi): DanmakuSource {
  return {
    async start(roomId) {
      const result = await api.startDanmaku(roomId);
      if (!result.ok) throw new Error(result.error.message);
    },
    async stop(roomId) {
      const result = await api.stopDanmaku(roomId);
      if (!result.ok) throw new Error(result.error.message);
    },
    subscribe(listener) {
      return api.onDanmakuEvent(listener);
    },
  };
}
```

- [ ] **Step 4: Add the browser-only Mock source**

Create `src/infrastructure/mock-danmaku-source.ts`. Keep a `Set` of listeners and a `Map` of room timers. `start` emits `connected` once and starts one 1.25-second interval per room. Each tick cycles through `getDanmakuMessages(roomId)` and emits one normalized message with nickname `演示用户`, ID `mock-{roomId}-{sequence}`, and an ISO receive time. `stop` clears one room timer. Unsubscribe only removes the listener and does not mutate room state.

Do not import this module from Main, Preload, or the real Renderer source.

- [ ] **Step 5: Run the source checkpoint**

Run:

```powershell
npm test -- tests/renderer-danmaku-source.test.ts
npm run typecheck
```

Expected: PASS and fake timers have no leaks. Do not create a Git repository.

### Task 8: Bounded Renderer Store and Runtime Provider

**Files:**

- Create: `src/renderer/store/danmaku-store.ts`
- Create: `src/renderer/store/danmaku-context.tsx`
- Create: `tests/danmaku-store.test.ts`
- Modify: `src/renderer/main.tsx`

- [ ] **Step 1: Write failing store tests**

Create `tests/danmaku-store.test.ts` with this normalized batch helper:

```ts
function messageBatch(roomId: string, count: number, start = 0): DanmakuEvent {
  return {
    type: 'messages',
    roomId,
    dropped: 0,
    messages: Array.from({ length: count }, (_, index) => {
      const id = String(start + index);
      return {
        id,
        roomId,
        nickname: `用户${id}`,
        text: `消息${id}`,
        receivedAt: '2026-08-07T00:00:00.000Z',
      };
    }),
  };
}
```

Import `DanmakuEvent`, then cover:

```ts
it('queues at most fifty enabled-room messages and schedules three per second', () => {
  const store = createDanmakuStore();
  store.getState().syncRoom('63136', true);
  store.getState().handleEvent(messageBatch('63136', 60));
  expect(store.getState().rooms['63136'].pending).toHaveLength(50);
  expect(store.getState().rooms['63136'].dropped).toBe(10);

  store.getState().tick();
  expect(store.getState().rooms['63136'].visible).toHaveLength(1);
  expect(store.getState().rooms['63136'].pending).toHaveLength(49);
});

it('clears and discards messages while hidden, then accepts only new messages', () => {
  const store = createDanmakuStore();
  store.getState().syncRoom('63136', true);
  store.getState().handleEvent(messageBatch('63136', 2));
  store.getState().syncRoom('63136', false);
  store.getState().handleEvent(messageBatch('63136', 2, 100));
  expect(store.getState().rooms['63136'].pending).toEqual([]);

  store.getState().syncRoom('63136', true);
  store.getState().handleEvent(messageBatch('63136', 1, 200));
  expect(store.getState().rooms['63136'].pending.map((message) => message.id)).toEqual(['200']);
});
```

Add the remaining bounded-state cases:

```ts
it('deduplicates IDs and caps visible messages', () => {
  const store = createDanmakuStore();
  store.getState().syncRoom('63136', true);
  const batch = messageBatch('63136', 6);
  store.getState().handleEvent(batch);
  store.getState().handleEvent(batch);
  for (let index = 0; index < 6; index += 1) store.getState().tick();
  expect(store.getState().rooms['63136'].visible).toHaveLength(5);
  expect(store.getState().rooms['63136'].pending).toHaveLength(1);
});

it('expires one message, removes rooms, and isolates status', () => {
  const store = createDanmakuStore();
  store.getState().syncRoom('101', true);
  store.getState().syncRoom('202', true);
  store.getState().handleEvent(messageBatch('101', 1));
  store.getState().tick();
  const visibleId = store.getState().rooms['101'].visible[0].id;
  store.getState().expireMessage('101', visibleId);
  store.getState().handleEvent({
    type: 'status', status: { roomId: '202', state: 'reconnecting', attempt: 1 },
  });
  expect(store.getState().rooms['101'].visible).toEqual([]);
  expect(store.getState().rooms['202'].status.state).toBe('reconnecting');
  store.getState().removeRoom('101');
  expect(store.getState().rooms['101']).toBeUndefined();
});

it('adds upstream drop counts to local overflow counts', () => {
  const store = createDanmakuStore();
  store.getState().syncRoom('63136', true);
  store.getState().handleEvent({ ...messageBatch('63136', 55), dropped: 7 });
  expect(store.getState().rooms['63136'].dropped).toBe(12);
});
```

- [ ] **Step 2: Confirm the store tests fail**

Run `npm test -- tests/danmaku-store.test.ts`.

Expected: FAIL because the store does not exist.

- [ ] **Step 3: Implement the vanilla Zustand store**

Create `src/renderer/store/danmaku-store.ts` with:

```ts
export const MAX_PENDING_DANMAKU = 50;
export const MAX_VISIBLE_DANMAKU = 5;

export interface DanmakuRoomView {
  enabled: boolean;
  status: DanmakuStatus;
  pending: DanmakuMessage[];
  visible: DanmakuMessage[];
  dropped: number;
}

export interface DanmakuState {
  rooms: Record<string, DanmakuRoomView>;
  syncRoom(roomId: string, enabled: boolean): void;
  removeRoom(roomId: string): void;
  handleEvent(event: DanmakuEvent): void;
  tick(): void;
  expireMessage(roomId: string, messageId: string): void;
}
```

Use `createStore<DanmakuState>` from `zustand/vanilla`. Keep per-room 200-ID FIFO dedupe sets in the store closure. `handleEvent` updates status regardless of visibility. It ignores message batches for missing or disabled rooms, appends enabled messages, keeps the newest 50, and adds local overflow plus `event.dropped` to the room count. `tick` moves at most one pending message into each enabled room's visible list and stops at five visible messages. `expireMessage` removes one visible ID. Disabling or removing a room clears its dedupe state and all queued messages.

- [ ] **Step 4: Add the Provider and room/source synchronization**

Create `src/renderer/store/danmaku-context.tsx` with:

- A context containing the vanilla store and `retryRoom(roomId)`.
- `DanmakuProvider({ source, children })`, mounted inside `WorkspaceProvider`.
- One effect that subscribes to `source` and dispatches validated events to `handleEvent`.
- One effect that compares current workspace rooms with a `Set` in a ref. It calls `syncRoom` for every current `danmakuEnabled` value before starting added IDs, then stops removed IDs and removes their Store state. This ordering lets a source emit status during `start()` without losing it.
- One 333ms interval that calls `store.getState().tick()`.
- Unmount cleanup that unsubscribes, stops every tracked room, and clears the scheduler.
- `retryRoom` that updates the room status to `connecting`, awaits `source.stop(roomId).catch(() => undefined)`, then calls `source.start(roomId)`; a rejected start sets `failed` with `NETWORK_UNAVAILABLE`.
- Every fire-and-forget `source.start` or `source.stop` call has a `.catch` branch. Start failures set a sanitized `failed` status; removal and unmount stop failures are swallowed without logging payloads because Main also owns final window cleanup.
- `useDanmakuRoom(roomId)`, `useDanmakuControls()`, and `useDanmakuExpire()` hooks. Outside a Provider, the room hook returns a frozen idle empty view, controls expose a no-op retry, and expiry is a no-op. This keeps isolated server-render tests deterministic while production still mounts the Provider.

- [ ] **Step 5: Choose the source once at Renderer startup**

In `src/renderer/main.tsx`, construct the source once at module initialization:

```tsx
const electronMode = typeof window !== 'undefined' && Boolean(window.appApi);
const danmakuSource = electronMode
  ? createRendererDanmakuSource(window.appApi)
  : createMockDanmakuSource();
```

Wrap `App` inside `DanmakuProvider`, which remains inside `WorkspaceProvider` so it can observe rooms:

```tsx
<WorkspaceProvider
  adapter={createRendererDouyuAdapter()}
  demoMode={!electronMode}
  initialRooms={getInitialRoomsForRuntime(electronMode)}
>
  <DanmakuProvider source={danmakuSource}>
    <App />
  </DanmakuProvider>
</WorkspaceProvider>
```

- [ ] **Step 6: Run the Store/Provider checkpoint**

Run:

```powershell
npm test -- tests/danmaku-store.test.ts tests/workspace-store.test.ts tests/runtime-mode.test.ts
npm run typecheck
```

Expected: PASS. Confirm the new Provider did not add danmaku arrays to `workspace-store`. Do not create a Git repository.

### Task 9: Overlay and Room Controls

**Files:**

- Modify: `src/renderer/components/DanmakuOverlay.tsx`
- Modify: `src/renderer/components/RoomPlaybackSurface.tsx`
- Modify: `src/renderer/components/RoomTile.tsx`
- Modify: `src/renderer/styles.css`
- Create: `tests/danmaku-overlay.test.tsx`
- Modify: `tests/room-playback-surface.test.tsx`
- Modify: `tests/app-smoke.test.tsx`

- [ ] **Step 1: Write failing rendering tests**

Create `tests/danmaku-overlay.test.tsx` using `renderToStaticMarkup`. Export a pure `DanmakuLines` component from `DanmakuOverlay.tsx` and test:

```tsx
const html = renderToStaticMarkup(
  <DanmakuLines
    messages={[{
      id: '1', roomId: '63136', nickname: '用户', text: '<script>bad()</script>',
      receivedAt: '2026-08-07T00:00:00.000Z',
    }]}
    onExpire={() => {}}
  />,
);
expect(html).toContain('用户：');
expect(html).toContain('&lt;script&gt;bad()&lt;/script&gt;');
expect(html).not.toContain('<script>');
expect(html).toContain('aria-label="弹幕"');
```

Extend `tests/room-playback-surface.test.tsx` so both demo and blocked branches contain the `danmaku-overlay` container when enabled, and neither branch contains old static Mock text. Add an app smoke assertion that production copy remains truthful.

- [ ] **Step 2: Confirm UI tests fail**

Run:

```powershell
npm test -- tests/danmaku-overlay.test.tsx tests/room-playback-surface.test.tsx tests/app-smoke.test.tsx
```

Expected: FAIL on the missing pure component and blocked-state overlay.

- [ ] **Step 3: Replace static Mock rendering**

Implement `DanmakuOverlay` as a store-connected wrapper and `DanmakuLines` as a pure component:

```tsx
export function DanmakuLines({
  messages,
  onExpire,
}: {
  messages: DanmakuMessage[];
  onExpire: (messageId: string) => void;
}) {
  return (
    <div className="danmaku-overlay" aria-label="弹幕" aria-live="off">
      {messages.map((message) => (
        <span
          className="danmaku-line"
          key={message.id}
          onAnimationEnd={() => onExpire(message.id)}
        >
          <strong>{message.nickname}：</strong>{message.text}
        </span>
      ))}
    </div>
  );
}

export function DanmakuOverlay({ roomId, enabled }: DanmakuOverlayProps) {
  const room = useDanmakuRoom(roomId);
  const expireMessage = useDanmakuExpire();
  if (!enabled) return null;
  return (
    <DanmakuLines
      messages={room.visible}
      onExpire={(messageId) => expireMessage(roomId, messageId)}
    />
  );
}
```

Expose `useDanmakuExpire()` from the context as a stable command hook. Do not import `mock-danmaku` from this component.

- [ ] **Step 4: Render the overlay on every playback surface**

Keep the existing demo branch overlay. Add the same `DanmakuOverlay` as a child of `.playback-state-surface` after the playback state controls. This preserves the truthful platform-blocked copy while allowing real danmaku over that room's placeholder.

- [ ] **Step 5: Add connection status and failed-state retry**

In `RoomTile.tsx`, import `LoaderCircle`, `RotateCw`, and `ShieldAlert`. Read the room's danmaku view and `retryRoom`. Keep the existing message-circle button as the display toggle.

- `connecting` and `reconnecting`: show a non-clickable spinning `LoaderCircle` with Tooltip text `弹幕连接中` or `弹幕重连中`.
- `failed`: show a 27px icon button containing `RotateCw`, label and title `重试弹幕连接`, calling `retryRoom(room.roomId)`.
- `platform-blocked`: show a non-clickable `ShieldAlert` with Tooltip text `弹幕平台阻塞`.
- `connected`: add `is-danmaku-connected` to the existing toggle button and include `弹幕已连接` in its title.
- `idle`: show no extra status control.

Do not put text labels in the compact action bar.

- [ ] **Step 6: Make animations finite and dimensions stable**

Replace the current infinite animation with:

```css
.danmaku-overlay {
  position: absolute;
  top: 18%;
  right: 11px;
  bottom: 27%;
  left: 11px;
  z-index: 1;
  display: flex;
  min-height: 0;
  flex-direction: column;
  align-items: flex-start;
  gap: 6px;
  overflow: hidden;
  pointer-events: none;
}

.danmaku-line {
  max-width: 90%;
  overflow: hidden;
  padding: 3px 7px;
  border-radius: 3px;
  background: rgba(7, 10, 13, 0.62);
  color: rgba(255, 255, 255, 0.94);
  font-size: 11px;
  line-height: 1.3;
  text-overflow: ellipsis;
  text-shadow: 0 1px 3px rgba(0, 0, 0, 0.7);
  white-space: nowrap;
  animation: danmaku-lift 7s ease-in-out both;
}

.danmaku-line strong { color: #ffd0b8; font-weight: 700; }
.danmaku-status { display: inline-grid; width: 27px; height: 27px; place-items: center; color: #a6b0bb; }
.danmaku-status.is-blocked { color: var(--danger); }
.tile-action-button.is-danmaku-connected::after {
  position: absolute;
  top: 3px;
  right: 3px;
  width: 5px;
  height: 5px;
  border-radius: 50%;
  background: var(--green);
  content: '';
}
```

Add `position: relative` to `.tile-action-button`. Keep letter spacing at `0`. Validate the blocked-state copy and overlay at 1440×900 and 390×844; adjust only the overlay's top/bottom bounds if screenshots show incoherent overlap.

- [ ] **Step 7: Run the UI checkpoint**

Run:

```powershell
npm test -- tests/danmaku-overlay.test.tsx tests/room-playback-surface.test.tsx tests/app-smoke.test.tsx
npm run typecheck
npm run build:renderer
```

Expected: PASS. Confirm `DanmakuOverlay.tsx` has no `mock-danmaku` import and `createRendererDanmakuSource` never falls back to Mock data after an IPC failure. The shared Renderer bundle may contain browser Mock code because one bundle supports both runtime modes. Do not create a Git repository.

### Task 10: Full Regression and Real Runtime Verification

**Files:**

- Modify only files required to fix failures caused by Tasks 1 through 9.
- Do not weaken assertions, security settings, queue limits, or platform-block rules to make checks pass.

- [ ] **Step 1: Run the complete automated suite**

Run:

```powershell
npm test
npm run typecheck
npm run build
npm audit
```

Expected: every Vitest file passes, TypeScript reports no errors, Main/Preload/Renderer builds complete, and npm reports no unresolved high or critical vulnerability. Record exact test-file and test counts.

- [ ] **Step 2: Inspect security and protocol boundaries**

Run focused searches:

```powershell
rg -n "getH5Play|cookie|document\.cookie|eval\(|new Function|nodeIntegration:\s*true|contextIsolation:\s*false" src dist
rg -n "loginreq|joingroup|mrkl|logout" src/infrastructure/douyu-danmaku
rg -n "dgb|comm_chatmsg|gift" src/main src/renderer src/shared
```

Expected: the first search finds no newly introduced bypass or weakened Electron setting; the second finds only the four allowed outbound protocol messages; the third finds no forwarding/rendering path for gifts or other non-chat events.

- [ ] **Step 3: Validate browser Mock mode with Playwright**

Start the development server on an unused port, then open its URL with Playwright. At 1440×900 and 390×844:

- Confirm the first screen remains the monitoring workspace.
- Confirm every browser room retains the `模拟画面` label.
- Wait for Mock messages and confirm they stay inside the correct room tile.
- Toggle one room's danmaku and confirm that room stops showing new messages while other rooms continue.
- Confirm headers, bottom bars, status icons, text, and layout controls do not overlap.
- Capture one desktop and one mobile screenshot for evidence.

Expected: no console error, no failed local asset, no clipped button label, and no cross-room message.

- [ ] **Step 4: Validate Electron against a real room**

Run:

```powershell
npm run build
npm start
```

In Electron:

1. Add room `63136`.
2. Confirm real room metadata and the existing `SIGNATURE_REQUIRED` playback block remain visible.
3. Confirm the danmaku state reaches `connected`, or record the exact sanitized `failed`/`platform-blocked` state.
4. Wait up to 60 seconds for a `chatmsg`. If one arrives, confirm “昵称：内容” appears only on room `63136` and disappears after one animation.
5. Hide and show danmaku. Hidden-period messages must not appear after re-enabling.
6. Add a second active room and confirm message isolation.
7. Remove a room, then close the window. Confirm Electron exits without a lingering process or reconnect timer.

If no chat arrives during the observation window, report only the verified connection state. Do not claim receipt of a real message.

- [ ] **Step 5: Final review against the approved spec**

Check every acceptance criterion in `docs/superpowers/specs/2026-08-07-douyu-danmaku-integration-design.md` against a test result, source inspection, screenshot, or Electron observation. List any unverified criterion as a remaining limitation. Do not declare completion until the evidence covers tests, typecheck, build, browser UI, and Electron runtime.

## Plan Self-Review Checklist

- Shared types use the same state names in Main, Preload, Renderer, and tests.
- Only `chatmsg` reaches `DanmakuEvent`.
- Main owns transport and keeps Renderer sandboxed.
- A room has one session and one room cannot affect another.
- Message queues, ID sets, retry counts, frame sizes, and visible overlays have hard limits.
- Hiding drops messages without disconnecting; removing a room closes its unowned session.
- Mock messages stay in browser demo mode and never become a production fallback.
- Playback blocking and quality-control behavior remain unchanged.
- The plan contains no credential, signature, cookie, private endpoint, or access-control workaround.
