import { afterEach, describe, expect, it, vi } from 'vitest';
import { createDanmakuStore } from '../src/renderer/store/danmaku-store';
import type { DanmakuEvent } from '../src/shared/danmaku-contract';
import type { DanmakuGovernanceSettings } from '../src/renderer/danmaku/danmaku-settings';

function messageBatch(
  roomId: string,
  count: number,
  start = 0,
): Extract<DanmakuEvent, { type: 'messages' }> {
  return {
    type: 'messages',
    roomId,
    dropped: 0,
    messages: Array.from({ length: count }, (_, index) => {
      const id = String(start + index);
      return {
        id,
        roomId,
        nickname: `User ${id}`,
        text: `Message ${id}`,
        receivedAt: '2026-08-07T00:00:00.000Z',
      };
    }),
  };
}

const governanceOff: DanmakuGovernanceSettings = {
  enabled: true,
  keywordBlacklist: [],
  duplicateWindowSeconds: 3,
  peakProtectionEnabled: false,
};

afterEach(() => {
  vi.useRealTimers();
});

describe('danmaku renderer store', () => {
  it('keeps the newest three hundred waiting messages', () => {
    const store = createDanmakuStore();
    store.getState().syncRoom('63136', true, governanceOff);
    store.getState().handleEvent(messageBatch('63136', 350));
    expect(store.getState().rooms['63136'].pending).toHaveLength(300);
    expect(store.getState().rooms['63136'].pending[0].id).toBe('50');
    expect(store.getState().rooms['63136'].dropped).toBe(50);
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
    expect(store.getState().rooms['63136'].pending.map((message) => message.id)).toEqual([
      '200',
    ]);
  });

  it('clears stale connection failures when danmaku is disabled', () => {
    const store = createDanmakuStore();
    store.getState().syncRoom('63136', true);
    store.getState().handleEvent({
      type: 'status',
      status: { roomId: '63136', state: 'failed', errorCode: 'NETWORK_UNAVAILABLE' },
    });

    store.getState().syncRoom('63136', false);

    expect(store.getState().rooms['63136']).toEqual(expect.objectContaining({
      enabled: false,
      status: { roomId: '63136', state: 'idle' },
    }));
  });

  it('deduplicates IDs and dequeues only the expected head message', () => {
    const store = createDanmakuStore();
    store.getState().syncRoom('63136', true);
    const batch = messageBatch('63136', 6);
    store.getState().handleEvent(batch);
    store.getState().handleEvent(batch);
    expect(store.getState().takePending('63136', '1')).toBe(false);
    expect(store.getState().takePending('63136', '0')).toBe(true);
    expect(store.getState().rooms['63136'].pending.map((message) => message.id)).toEqual([
      '1', '2', '3', '4', '5',
    ]);
  });

  it('keeps queued message ids in the deduplication window', () => {
    const store = createDanmakuStore();
    store.getState().syncRoom('63136', true, governanceOff);
    store.getState().handleEvent(messageBatch('63136', 300));

    store.getState().handleEvent(messageBatch('63136', 1));

    expect(store.getState().rooms['63136'].pending).toHaveLength(300);
    expect(store.getState().rooms['63136'].dropped).toBe(0);
  });

  it('removes rooms and isolates status', () => {
    const store = createDanmakuStore();
    store.getState().syncRoom('101', true);
    store.getState().syncRoom('202', true);
    store.getState().handleEvent(messageBatch('101', 1));
    store.getState().handleEvent({
      type: 'status',
      status: { roomId: '202', state: 'reconnecting', attempt: 1 },
    });
    expect(store.getState().rooms['101'].pending).toHaveLength(1);
    expect(store.getState().rooms['202'].status.state).toBe('reconnecting');
    store.getState().removeRoom('101');
    expect(store.getState().rooms['101']).toBeUndefined();
  });

  it('adds upstream drop counts to local overflow counts', () => {
    const store = createDanmakuStore();
    store.getState().syncRoom('63136', true, governanceOff);
    store.getState().handleEvent({ ...messageBatch('63136', 305), dropped: 7 });
    expect(store.getState().rooms['63136'].dropped).toBe(12);
    expect(store.getState().rooms['63136'].governanceStats.queueOverflow).toBe(5);
    expect(store.getState().rooms['63136'].governanceStats.upstreamDropped).toBe(7);
  });

  it('filters and suppresses messages before they enter the room queue', () => {
    vi.useFakeTimers();
    vi.setSystemTime(new Date('2026-08-07T00:00:00.000Z'));
    const store = createDanmakuStore();
    store.getState().syncRoom('63136', true, {
      ...governanceOff,
      keywordBlacklist: ['广告'],
    });
    store.getState().handleEvent({
      type: 'messages',
      roomId: '63136',
      dropped: 0,
      messages: [
        { ...messageBatch('63136', 1).messages[0], id: '1', text: '广告' },
        { ...messageBatch('63136', 1).messages[0], id: '2', text: '正常' },
        { ...messageBatch('63136', 1).messages[0], id: '3', text: ' 正常 ' },
      ],
    });

    expect(store.getState().rooms['63136'].pending.map((item) => item.id)).toEqual(['2']);
    expect(store.getState().rooms['63136'].governanceStats.filtered).toBe(1);
    expect(store.getState().rooms['63136'].governanceStats.duplicates).toBe(1);
  });

  it('limits a crowded batch and records the governance level', () => {
    vi.useFakeTimers();
    vi.setSystemTime(new Date('2026-08-07T00:00:00.000Z'));
    const store = createDanmakuStore();
    store.getState().syncRoom('63136', true, {
      ...governanceOff,
      peakProtectionEnabled: true,
    });
    store.getState().handleEvent(messageBatch('63136', 60));

    expect(store.getState().rooms['63136'].pending).toHaveLength(20);
    expect(store.getState().rooms['63136'].governanceStats).toEqual(expect.objectContaining({
      level: 'crowded',
      recentRate: 20,
      rateLimited: 40,
    }));
  });

  it('clears queued messages and governance runtime when the effective rules change', () => {
    vi.useFakeTimers();
    vi.setSystemTime(new Date('2026-08-07T00:00:00.000Z'));
    const store = createDanmakuStore();
    store.getState().syncRoom('63136', true, governanceOff);
    store.getState().handleEvent(messageBatch('63136', 1));
    expect(store.getState().rooms['63136'].pending).toHaveLength(1);

    store.getState().syncRoom('63136', true, {
      ...governanceOff,
      keywordBlacklist: ['消息'],
    });

    expect(store.getState().rooms['63136'].pending).toEqual([]);
    expect(store.getState().rooms['63136'].governanceStats.filtered).toBe(0);
  });

  it('clears governance counters without affecting the room connection', () => {
    vi.useFakeTimers();
    vi.setSystemTime(new Date('2026-08-07T00:00:00.000Z'));
    const store = createDanmakuStore();
    store.getState().syncRoom('63136', true, {
      ...governanceOff,
      keywordBlacklist: ['广告'],
    });
    store.getState().handleEvent({ ...messageBatch('63136', 1), dropped: 2 });
    store.getState().clearGovernanceStats('63136');

    expect(store.getState().rooms['63136'].status.state).toBe('idle');
    expect(store.getState().rooms['63136'].pending).toHaveLength(1);
    expect(store.getState().rooms['63136'].governanceStats.filtered).toBe(0);
    expect(store.getState().rooms['63136'].governanceStats.upstreamDropped).toBe(0);
  });
});
