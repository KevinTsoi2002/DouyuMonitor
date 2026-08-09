import { afterEach, describe, expect, it, vi } from 'vitest';
import {
  createDouyuDanmakuClient,
  type DouyuClientEvent,
} from '../src/infrastructure/douyu-danmaku/client';
import { encodeDouyuFrame } from '../src/infrastructure/douyu-danmaku/protocol';
import { serializeStt } from '../src/infrastructure/douyu-danmaku/stt';
import type {
  DanmakuSocketFactory,
  DanmakuSocketHandlers,
} from '../src/infrastructure/douyu-danmaku/socket';

afterEach(() => {
  vi.clearAllTimers();
  vi.useRealTimers();
});

function createFakeSocketFactory() {
  const sockets: Array<{
    url: string;
    handlers: DanmakuSocketHandlers;
    sent: Uint8Array[];
    close: () => void;
    dispose: () => void;
  }> = [];
  const factory: DanmakuSocketFactory = (url, handlers) => {
    const socket: (typeof sockets)[number] = {
      url,
      handlers,
      sent: [],
      close: vi.fn<() => void>(),
      dispose: vi.fn<() => void>(),
    };
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
    get latest() {
      return sockets[sockets.length - 1];
    },
    get urls() {
      return sockets.map((socket) => socket.url);
    },
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

describe('Douyu one-room danmaku client', () => {
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
    fake.latest.handlers.message(serverFrame({ type: 'loginres' }));

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
    fake.latest.handlers.message(
      serverFrame({ type: 'chatmsg', rid: '63136', cid: '1', nn: 'User', txt: 'Hello' }),
    );
    fake.latest.handlers.message(serverFrame({ type: 'dgb', rid: '63136', gfid: '20001' }));
    fake.latest.handlers.message(
      serverFrame({ type: 'chatmsg', rid: '999', cid: '2', nn: 'Other', txt: 'Ignore' }),
    );

    expect(events.filter((event) => event.type === 'chat')).toEqual([
      {
        type: 'chat',
        message: { type: 'chatmsg', rid: '63136', cid: '1', nn: 'User', txt: 'Hello' },
      },
    ]);
    client.stop();
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
      status: {
        roomId: '63136',
        state: 'failed',
        attempt: 6,
        errorCode: 'RETRY_EXHAUSTED',
      },
    });
  });

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
});
