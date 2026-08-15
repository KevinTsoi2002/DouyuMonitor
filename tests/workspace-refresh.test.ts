import { describe, expect, it, vi } from 'vitest';
import type { RoomStatus } from '../src/domain/douyu-adapter';
import {
  createWorkspaceRefreshScheduler,
  OFFLINE_REFRESH_INTERVAL_MS,
  ONLINE_REFRESH_INTERVAL_MS,
  REFRESH_BACKOFF_DELAYS_MS,
} from '../src/renderer/store/workspace-refresh';

function room(roomId: string, online: boolean): { roomId: string; online: boolean; status: RoomStatus } {
  return { roomId, online, status: online ? 'playing' : 'offline' };
}

describe('workspace refresh scheduler', () => {
  it('schedules online and offline rooms at different base intervals', async () => {
    vi.useFakeTimers();
    try {
      const rooms = [room('online', true), room('offline', false)];
      const refreshRoomMetadata = vi.fn(async () => true);
      const scheduler = createWorkspaceRefreshScheduler({
        getRooms: () => rooms,
        refreshRoomMetadata,
        jitterMs: () => 0,
      });

      scheduler.sync();
      await vi.advanceTimersByTimeAsync(ONLINE_REFRESH_INTERVAL_MS - 1);
      expect(refreshRoomMetadata).not.toHaveBeenCalled();

      await vi.advanceTimersByTimeAsync(1);
      expect(refreshRoomMetadata).toHaveBeenCalledWith('online');
      expect(refreshRoomMetadata).toHaveBeenCalledTimes(1);

      await vi.advanceTimersByTimeAsync(OFFLINE_REFRESH_INTERVAL_MS - ONLINE_REFRESH_INTERVAL_MS - 1);
      expect(refreshRoomMetadata).toHaveBeenCalledTimes(1);
      await vi.advanceTimersByTimeAsync(1);
      expect(refreshRoomMetadata).toHaveBeenCalledWith('offline');
      expect(refreshRoomMetadata).toHaveBeenCalledTimes(3);

      scheduler.dispose();
    } finally {
      vi.useRealTimers();
    }
  });

  it('backs off failures and resets the delay after success', async () => {
    vi.useFakeTimers();
    try {
      const refreshRoomMetadata = vi.fn()
        .mockResolvedValueOnce(false)
        .mockResolvedValueOnce(false)
        .mockResolvedValueOnce(true)
        .mockResolvedValueOnce(true);
      const scheduler = createWorkspaceRefreshScheduler({
        getRooms: () => [room('room-1', true)],
        refreshRoomMetadata,
        jitterMs: () => 0,
      });

      scheduler.sync();
      await vi.advanceTimersByTimeAsync(ONLINE_REFRESH_INTERVAL_MS);
      expect(refreshRoomMetadata).toHaveBeenCalledTimes(1);

      await vi.advanceTimersByTimeAsync(REFRESH_BACKOFF_DELAYS_MS[0] - 1);
      expect(refreshRoomMetadata).toHaveBeenCalledTimes(1);
      await vi.advanceTimersByTimeAsync(1);
      expect(refreshRoomMetadata).toHaveBeenCalledTimes(2);

      await vi.advanceTimersByTimeAsync(REFRESH_BACKOFF_DELAYS_MS[1]);
      expect(refreshRoomMetadata).toHaveBeenCalledTimes(3);
      await vi.advanceTimersByTimeAsync(ONLINE_REFRESH_INTERVAL_MS - 1);
      expect(refreshRoomMetadata).toHaveBeenCalledTimes(3);
      await vi.advanceTimersByTimeAsync(1);
      expect(refreshRoomMetadata).toHaveBeenCalledTimes(4);

      scheduler.dispose();
    } finally {
      vi.useRealTimers();
    }
  });

  it('does not run a second refresh while the first one is pending', async () => {
    vi.useFakeTimers();
    try {
      let resolveRefresh!: (value: boolean) => void;
      const refreshPromise = new Promise<boolean>((resolve) => { resolveRefresh = resolve; });
      const refreshRoomMetadata = vi.fn(() => refreshPromise);
      const rooms = [room('room-1', true)];
      const scheduler = createWorkspaceRefreshScheduler({
        getRooms: () => rooms,
        refreshRoomMetadata,
        jitterMs: () => 0,
      });

      scheduler.sync();
      await vi.advanceTimersByTimeAsync(ONLINE_REFRESH_INTERVAL_MS);
      scheduler.sync();
      await vi.advanceTimersByTimeAsync(ONLINE_REFRESH_INTERVAL_MS);
      expect(refreshRoomMetadata).toHaveBeenCalledTimes(1);

      resolveRefresh(true);
      await vi.advanceTimersByTimeAsync(ONLINE_REFRESH_INTERVAL_MS);
      expect(refreshRoomMetadata).toHaveBeenCalledTimes(2);
      scheduler.dispose();
    } finally {
      vi.useRealTimers();
    }
  });

  it('stops removed rooms and pending timers when disposed', async () => {
    vi.useFakeTimers();
    try {
      const rooms = [room('room-1', true)];
      const refreshRoomMetadata = vi.fn(async () => true);
      const scheduler = createWorkspaceRefreshScheduler({
        getRooms: () => rooms,
        refreshRoomMetadata,
        jitterMs: () => 0,
      });

      scheduler.sync();
      rooms.splice(0, 1);
      scheduler.sync();
      await vi.advanceTimersByTimeAsync(ONLINE_REFRESH_INTERVAL_MS * 2);
      expect(refreshRoomMetadata).not.toHaveBeenCalled();

      scheduler.dispose();
      await vi.advanceTimersByTimeAsync(ONLINE_REFRESH_INTERVAL_MS * 2);
      expect(refreshRoomMetadata).not.toHaveBeenCalled();
    } finally {
      vi.useRealTimers();
    }
  });
});
