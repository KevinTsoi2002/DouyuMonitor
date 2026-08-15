import type { RoomStatus } from '../../domain/douyu-adapter';

export const ONLINE_REFRESH_INTERVAL_MS = 60_000;
export const OFFLINE_REFRESH_INTERVAL_MS = 120_000;
export const REFRESH_BACKOFF_DELAYS_MS = [30_000, 60_000, 120_000, 240_000] as const;

export interface RefreshRoomSnapshot {
  roomId: string;
  online: boolean;
  status: RoomStatus;
}

export interface WorkspaceRefreshSchedulerOptions {
  getRooms: () => RefreshRoomSnapshot[];
  refreshRoomMetadata: (roomId: string) => Promise<boolean>;
  setTimeout?: typeof globalThis.setTimeout;
  clearTimeout?: typeof globalThis.clearTimeout;
  jitterMs?: (roomId: string, intervalMs: number) => number;
}

export interface WorkspaceRefreshScheduler {
  sync(): void;
  dispose(): void;
}

type TimerHandle = ReturnType<typeof globalThis.setTimeout>;

interface RoomRefreshState {
  failures: number;
  running: boolean;
  timer?: TimerHandle;
}

function roomInterval(room: RefreshRoomSnapshot): number {
  return room.online && room.status !== 'offline'
    ? ONLINE_REFRESH_INTERVAL_MS
    : OFFLINE_REFRESH_INTERVAL_MS;
}

function defaultJitter(roomId: string, intervalMs: number): number {
  let hash = 0;
  for (const character of roomId) hash = (hash * 31 + character.charCodeAt(0)) >>> 0;
  return hash % Math.min(5_000, Math.max(1, Math.floor(intervalMs * 0.1)));
}

export function createWorkspaceRefreshScheduler(
  options: WorkspaceRefreshSchedulerOptions,
): WorkspaceRefreshScheduler {
  const scheduleTimer = options.setTimeout ?? globalThis.setTimeout;
  const clearTimer = options.clearTimeout ?? globalThis.clearTimeout;
  const jitter = options.jitterMs ?? defaultJitter;
  const states = new Map<string, RoomRefreshState>();
  let disposed = false;

  const getRoom = (roomId: string) => options.getRooms().find((room) => room.roomId === roomId);

  const schedule = (roomId: string, delayMs?: number) => {
    if (disposed) return;
    const room = getRoom(roomId);
    if (!room) {
      states.delete(roomId);
      return;
    }

    const state = states.get(roomId) ?? { failures: 0, running: false };
    states.set(roomId, state);
    if (state.running || state.timer !== undefined) return;

    const intervalMs = roomInterval(room);
    const delay = delayMs ?? intervalMs + jitter(roomId, intervalMs);
    state.timer = scheduleTimer(() => {
      state.timer = undefined;
      void run(roomId, state);
    }, delay);
  };

  const run = async (roomId: string, state: RoomRefreshState): Promise<void> => {
    if (disposed) return;
    if (!getRoom(roomId)) {
      states.delete(roomId);
      return;
    }

    state.running = true;
    let succeeded = false;
    try {
      succeeded = await options.refreshRoomMetadata(roomId);
    } catch {
      succeeded = false;
    } finally {
      state.running = false;
    }

    const room = getRoom(roomId);
    if (disposed || !room) {
      states.delete(roomId);
      return;
    }

    if (succeeded) {
      state.failures = 0;
      schedule(roomId);
      return;
    }

    state.failures = Math.min(state.failures + 1, REFRESH_BACKOFF_DELAYS_MS.length);
    schedule(roomId, REFRESH_BACKOFF_DELAYS_MS[state.failures - 1]);
  };

  return {
    sync() {
      if (disposed) return;
      const roomIds = new Set(options.getRooms().map((room) => room.roomId));

      for (const [roomId, state] of states) {
        if (roomIds.has(roomId)) continue;
        if (state.timer !== undefined) clearTimer(state.timer);
        states.delete(roomId);
      }

      for (const roomId of roomIds) schedule(roomId);
    },

    dispose() {
      if (disposed) return;
      disposed = true;
      for (const state of states.values()) {
        if (state.timer !== undefined) clearTimer(state.timer);
      }
      states.clear();
    },
  };
}
