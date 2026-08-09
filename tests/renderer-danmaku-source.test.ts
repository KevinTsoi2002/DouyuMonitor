import { afterEach, describe, expect, it, vi } from 'vitest';
import { createMockDanmakuSource } from '../src/infrastructure/mock-danmaku-source';
import { createRendererDanmakuSource } from '../src/infrastructure/renderer-danmaku-source';
import type { DanmakuEvent } from '../src/shared/danmaku-contract';
import { ok } from '../src/shared/ipc-contract';

afterEach(() => {
  vi.clearAllTimers();
  vi.useRealTimers();
});

describe('renderer danmaku sources', () => {
  it('delegates start, stop, and subscription to the bounded AppApi', async () => {
    const unsubscribe = vi.fn<() => void>();
    const api = {
      startDanmaku: vi.fn(async () => ok(undefined)),
      stopDanmaku: vi.fn(async () => ok(undefined)),
      onDanmakuEvent: vi.fn(() => unsubscribe),
    };
    const source = createRendererDanmakuSource(api);
    const listener = vi.fn<(event: DanmakuEvent) => void>();

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
        error: { code: 'UNKNOWN', message: 'Danmaku connection failed', retryable: true },
      }),
      stopDanmaku: async () => ok(undefined),
      onDanmakuEvent: () => () => {},
    });

    await expect(source.start('63136')).rejects.toThrow('Danmaku connection failed');
  });

  it('runs one browser Mock timer per room and clears it on stop', async () => {
    vi.useFakeTimers();
    const source = createMockDanmakuSource();
    const events: DanmakuEvent[] = [];
    const unsubscribe = source.subscribe((event) => events.push(event));
    await source.start('63136');
    await source.start('63136');
    expect(events[0]).toEqual({
      type: 'status',
      status: { roomId: '63136', state: 'connected' },
    });
    vi.advanceTimersByTime(1_250);
    expect(events.filter((event) => event.type === 'messages')).toHaveLength(1);
    await source.stop('63136');
    const eventCount = events.length;
    vi.advanceTimersByTime(5_000);
    expect(events).toHaveLength(eventCount);
    unsubscribe();
  });
});
