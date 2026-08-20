import { describe, expect, it } from 'vitest';
import type { DanmakuStatus } from '../src/shared/danmaku-contract';
import type { RoomSession } from '../src/renderer/store/workspace-store';
import {
  formatMonitoringTime,
  getMonitoringSummary,
  getRoomMonitoringView,
} from '../src/renderer/monitoring-status';

function room(roomId: string, patch: Partial<RoomSession> = {}): RoomSession {
  return {
    roomId,
    anchorName: `主播 ${roomId}`,
    title: `直播间 ${roomId}`,
    category: '综合直播',
    online: true,
    viewerLabel: '100',
    status: 'playing',
    quality: 'auto',
    effectiveQuality: 'auto',
    volume: 0.5,
    danmakuEnabled: true,
    playbackAvailabilityStatus: 'available',
    ...patch,
  };
}

const connected: DanmakuStatus = { roomId: '101', state: 'connected' };
const failed: DanmakuStatus = {
  roomId: '202',
  state: 'failed',
  attempt: 6,
  errorCode: 'RETRY_EXHAUSTED',
};

describe('monitoring status model', () => {
  it('summarizes online, playing, playback issue, and danmaku issue counts', () => {
    const rooms = [
      room('101'),
      room('202', {
        playbackAvailabilityStatus: 'blocked',
        streamAvailability: {
          kind: 'blocked',
          roomId: '202',
          reason: 'SIGNATURE_REQUIRED',
          observedQualities: [],
          checkedAt: '2026-08-15T00:00:00.000Z',
        },
      }),
      room('303', { online: false, status: 'offline' }),
    ];

    expect(getMonitoringSummary(rooms, { '101': connected, '202': failed })).toEqual({
      online: 2,
      playing: 1,
      playbackIssues: 1,
      danmakuIssues: 1,
    });
  });

  it('prioritizes the latest player recovery error and exposes retry state', () => {
    const view = getRoomMonitoringView(
      room('202', {
        playbackRecovery: {
          attempt: 3,
          exhausted: false,
          errorCode: 'NETWORK_ERROR',
          updatedAt: '2026-08-15T10:00:00.000Z',
        },
        playbackCheckedAt: '2026-08-15T09:58:00.000Z',
      }),
      failed,
    );

    expect(view).toEqual(expect.objectContaining({
      playbackState: 'recovering',
      playbackAttempt: 3,
      danmakuState: 'failed',
      lastErrorType: 'NETWORK_ERROR',
      lastCheckedAt: '2026-08-15T09:58:00.000Z',
    }));
  });

  it('uses a sanitized playback error code for the latest error label', () => {
    expect(getRoomMonitoringView(room('101', {
      playbackAvailabilityStatus: 'error',
      playbackError: '底层网络细节不应直接展示',
      playbackErrorCode: 'NETWORK_UNAVAILABLE',
    }), connected).lastErrorType).toBe('NETWORK_UNAVAILABLE');
  });

  it('formats recent check times without exposing invalid dates', () => {
    const now = new Date('2026-08-15T10:00:00.000Z');

    expect(formatMonitoringTime('2026-08-15T09:59:00.000Z', now)).toBe('1 分钟前');
    expect(formatMonitoringTime(undefined, now)).toBe('未检查');
    expect(formatMonitoringTime('invalid', now)).toBe('未检查');
  });
});
