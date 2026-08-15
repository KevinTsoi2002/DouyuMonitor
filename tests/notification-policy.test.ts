import { describe, expect, it } from 'vitest';
import {
  createNotificationPolicy,
  type RoomNotificationSnapshot,
} from '../src/renderer/notifications/notification-policy';

function room(
  roomId: string,
  online: boolean,
  playbackState: RoomNotificationSnapshot['playbackState'] = online ? 'playing' : 'offline',
  playbackErrorCode?: string,
): RoomNotificationSnapshot {
  return { roomId, anchorName: `主播 ${roomId}`, online, playbackState, playbackErrorCode };
}

describe('notification policy', () => {
  it('seeds the first snapshot without emitting notifications', () => {
    const policy = createNotificationPolicy({ now: () => 0 });

    expect(policy.update([room('101', true)])).toEqual([]);
    expect(policy.update([room('101', true)])).toEqual([]);
  });

  it('emits playback failure and recovery only on state transitions', () => {
    let now = 1_000;
    const policy = createNotificationPolicy({ now: () => now });
    policy.update([room('101', true)]);

    const failure = policy.update([room('101', true, 'error', 'PLAYBACK_SOURCE_CHECK_FAILED')]);
    expect(failure).toEqual([expect.objectContaining({
      type: 'playback-failed',
      roomId: '101',
      dedupeKey: '101:playback-failed:PLAYBACK_SOURCE_CHECK_FAILED',
    })]);

    now += 1_000;
    expect(policy.update([room('101', true, 'error', 'PLAYBACK_SOURCE_CHECK_FAILED')])).toEqual([]);
    expect(policy.update([room('101', true, 'playing')])).toEqual([
      expect.objectContaining({ type: 'playback-recovered', roomId: '101' }),
    ]);
  });

  it('emits online and offline transitions but not a newly added room', () => {
    const policy = createNotificationPolicy({ now: () => 10_000 });
    policy.update([room('101', false)]);

    expect(policy.update([room('101', true)])).toEqual([
      expect.objectContaining({ type: 'room-online', roomId: '101' }),
    ]);
    expect(policy.update([room('101', false)])).toEqual([
      expect.objectContaining({ type: 'room-offline', roomId: '101' }),
    ]);
    expect(policy.update([room('101', false), room('202', true)])).toEqual([]);
  });

  it('deduplicates the same diagnostic key for five minutes', () => {
    let now = 20_000;
    const policy = createNotificationPolicy({ now: () => now });
    policy.update([room('101', true)]);
    expect(policy.update([room('101', true, 'error', 'SOURCE_FAILED')])).toHaveLength(1);
    expect(policy.update([room('101', true, 'playing')])).toHaveLength(1);

    now += 4 * 60 * 1000;
    expect(policy.update([room('101', true, 'error', 'SOURCE_FAILED')])).toEqual([]);
    expect(policy.update([room('101', true, 'playing')])).toEqual([]);

    now += 60 * 1000;
    expect(policy.update([room('101', true, 'error', 'SOURCE_FAILED')])).toHaveLength(1);
  });

  it('limits system notification events to six per minute', () => {
    let now = 30_000;
    const policy = createNotificationPolicy({ now: () => now });
    policy.update(Array.from({ length: 7 }, (_, index) => room(String(index), false)));

    const events = policy.update(Array.from({ length: 7 }, (_, index) => room(String(index), true)));
    expect(events).toHaveLength(6);

    now += 60_000;
    expect(policy.update([room('6', false)])).toEqual([
      expect.objectContaining({ type: 'room-offline', roomId: '6' }),
    ]);
  });
});
