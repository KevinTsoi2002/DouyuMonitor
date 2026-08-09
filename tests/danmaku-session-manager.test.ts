import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import type { DouyuClientEvent } from '../src/infrastructure/douyu-danmaku/client';
import {
  createDanmakuSessionManager,
  type DanmakuClientFactory,
  type DanmakuSessionManager,
} from '../src/main/danmaku-session-manager';
import type { DanmakuEvent } from '../src/shared/danmaku-contract';
import { IPC_CHANNELS } from '../src/shared/ipc-contract';

const managers: DanmakuSessionManager[] = [];

beforeEach(() => vi.useFakeTimers());
afterEach(() => {
  for (const manager of managers.splice(0)) manager.stopAll();
  vi.clearAllTimers();
  vi.useRealTimers();
});

function createManagerHarness() {
  const clients: Array<{
    start: () => void;
    stop: () => void;
    emit: (event: DouyuClientEvent) => void;
  }> = [];
  const clientFactory: DanmakuClientFactory = (_roomId, emit) => {
    const client = {
      start: vi.fn<() => void>(),
      stop: vi.fn<() => void>(),
      emit,
    };
    clients.push(client);
    return client;
  };
  const createOwner = (id: number) => ({
    id,
    destroyed: false,
    events: [] as DanmakuEvent[],
    isDestroyed() {
      return this.destroyed;
    },
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

describe('danmaku session manager', () => {
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
    const harness = createManagerHarness();
    harness.manager.start(harness.ownerA, '63136');
    harness.clients[0].emit({
      type: 'chat',
      message: {
        type: 'chatmsg',
        rid: '63136',
        cid: '1',
        nn: 'User\u0000',
        txt: 'First line\nSecond line',
      },
    });
    harness.clients[0].emit({
      type: 'chat',
      message: { type: 'chatmsg', rid: '63136', cid: '1', nn: 'User', txt: 'Duplicate' },
    });
    vi.advanceTimersByTime(250);

    expect(harness.ownerA.events).toEqual([
      expect.objectContaining({
        type: 'messages',
        roomId: '63136',
        dropped: 0,
        messages: [
          expect.objectContaining({
            id: '1',
            nickname: 'User',
            text: 'First line Second line',
          }),
        ],
      }),
    ]);
  });

  it('rejects a tenth distinct room', () => {
    const harness = createManagerHarness();
    for (let roomId = 1; roomId <= 9; roomId += 1) {
      expect(harness.manager.start(harness.ownerA, String(roomId))).toBe('started');
    }
    expect(harness.manager.start(harness.ownerA, '10')).toBe('limit');
  });

  it('keeps the newest one hundred messages and flushes ten at a time', () => {
    const harness = createManagerHarness();
    harness.manager.start(harness.ownerA, '63136');
    for (let index = 1; index <= 120; index += 1) {
      harness.clients[0].emit({
        type: 'chat',
        message: {
          type: 'chatmsg',
          rid: '63136',
          cid: String(index),
          nn: 'User',
          txt: `Message ${index}`,
        },
      });
    }
    vi.advanceTimersByTime(250);
    const batch = harness.ownerA.events.at(-1);
    expect(batch).toEqual(expect.objectContaining({ type: 'messages', dropped: 20 }));
    expect(batch?.type === 'messages' && batch.messages.map((message) => message.id)).toEqual([
      '21',
      '22',
      '23',
      '24',
      '25',
      '26',
      '27',
      '28',
      '29',
      '30',
    ]);
  });

  it('sends status immediately and removes destroyed owners', () => {
    const harness = createManagerHarness();
    harness.manager.start(harness.ownerA, '63136');
    harness.clients[0].emit({
      type: 'status',
      status: { roomId: '63136', state: 'connected' },
    });
    expect(harness.ownerA.events.at(-1)).toEqual({
      type: 'status',
      status: { roomId: '63136', state: 'connected' },
    });
    harness.ownerA.destroyed = true;
    harness.clients[0].emit({
      type: 'status',
      status: { roomId: '63136', state: 'reconnecting', attempt: 1 },
    });
    expect(harness.ownerA.events).toHaveLength(1);
  });

  it('stops every session owned only by one destroyed window', () => {
    const harness = createManagerHarness();
    harness.manager.start(harness.ownerA, '101');
    harness.manager.start(harness.ownerA, '202');
    harness.manager.stopOwner(harness.ownerA.id);
    for (const client of harness.clients) expect(client.stop).toHaveBeenCalledOnce();
  });

  it('restarts an existing failed session without creating a second client', () => {
    const harness = createManagerHarness();
    harness.manager.start(harness.ownerA, '63136');
    harness.clients[0].emit({
      type: 'status',
      status: {
        roomId: '63136',
        state: 'failed',
        attempt: 6,
        errorCode: 'RETRY_EXHAUSTED',
      },
    });
    expect(harness.manager.start(harness.ownerA, '63136')).toBe('existing');
    expect(harness.clients).toHaveLength(1);
    expect(harness.clients[0].start).toHaveBeenCalledTimes(2);
  });
});
