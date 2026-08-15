import { createStore, type StoreApi } from 'zustand/vanilla';
import type {
  DanmakuEvent,
  DanmakuMessage,
  DanmakuStatus,
} from '../../shared/danmaku-contract';
import {
  applyDanmakuGovernance,
  createDanmakuGovernanceRuntime,
  type DanmakuGovernanceRuntime,
  type DanmakuGovernanceStats,
} from '../danmaku/danmaku-governance';
import {
  DEFAULT_DANMAKU_GOVERNANCE,
  type DanmakuGovernanceSettings,
} from '../danmaku/danmaku-settings';

export const MAX_PENDING_DANMAKU = 300;
const MAX_DEDUPE_IDS = MAX_PENDING_DANMAKU * 2;

export interface DanmakuRoomView {
  enabled: boolean;
  status: DanmakuStatus;
  pending: DanmakuMessage[];
  dropped: number;
  governanceStats: DanmakuGovernanceStats;
}

export interface DanmakuState {
  rooms: Record<string, DanmakuRoomView>;
  syncRoom(roomId: string, enabled: boolean, governance?: DanmakuGovernanceSettings): void;
  removeRoom(roomId: string): void;
  handleEvent(event: DanmakuEvent): void;
  takePending(roomId: string, expectedMessageId: string): boolean;
  clearGovernanceStats(roomId: string): void;
}

interface DedupeState {
  ids: Set<string>;
  order: string[];
}

function createRoom(
  roomId: string,
  enabled: boolean,
  governanceStats: DanmakuGovernanceStats,
): DanmakuRoomView {
  return {
    enabled,
    status: { roomId, state: 'idle' },
    pending: [],
    dropped: 0,
    governanceStats,
  };
}

function governanceSignature(settings: DanmakuGovernanceSettings): string {
  return JSON.stringify(settings);
}

export function createDanmakuStore(): StoreApi<DanmakuState> {
  const dedupeByRoom = new Map<string, DedupeState>();
  const governanceByRoom = new Map<string, DanmakuGovernanceSettings>();
  const governanceRuntimeByRoom = new Map<string, DanmakuGovernanceRuntime>();

  const resetDedupe = (roomId: string) => {
    dedupeByRoom.delete(roomId);
  };

  const getDedupe = (roomId: string) => {
    let dedupe = dedupeByRoom.get(roomId);
    if (!dedupe) {
      dedupe = { ids: new Set(), order: [] };
      dedupeByRoom.set(roomId, dedupe);
    }
    return dedupe;
  };

  const getGovernance = (roomId: string): DanmakuGovernanceSettings => (
    governanceByRoom.get(roomId) ?? DEFAULT_DANMAKU_GOVERNANCE
  );

  const getGovernanceRuntime = (roomId: string): DanmakuGovernanceRuntime => {
    let runtime = governanceRuntimeByRoom.get(roomId);
    if (!runtime) {
      runtime = createDanmakuGovernanceRuntime();
      governanceRuntimeByRoom.set(roomId, runtime);
    }
    return runtime;
  };

  const resetGovernance = (roomId: string) => {
    governanceRuntimeByRoom.delete(roomId);
    governanceByRoom.delete(roomId);
  };

  return createStore<DanmakuState>((set, get) => ({
    rooms: {},
    syncRoom(roomId, enabled, governance = DEFAULT_DANMAKU_GOVERNANCE) {
      const existing = get().rooms[roomId];
      if (!existing) {
        const runtime = createDanmakuGovernanceRuntime();
        governanceByRoom.set(roomId, governance);
        governanceRuntimeByRoom.set(roomId, runtime);
        set((state) => ({
          rooms: {
            ...state.rooms,
            [roomId]: createRoom(roomId, enabled, runtime.stats),
          },
        }));
        return;
      }
      const previousGovernance = getGovernance(roomId);
      const hasGovernanceChanged = governanceSignature(previousGovernance)
        !== governanceSignature(governance);
      if (existing.enabled === enabled && !hasGovernanceChanged) return;
      if (!enabled || hasGovernanceChanged) {
        resetGovernance(roomId);
        governanceByRoom.set(roomId, governance);
        governanceRuntimeByRoom.set(roomId, createDanmakuGovernanceRuntime());
      }
      if (!enabled) resetDedupe(roomId);
      const nextRuntime = getGovernanceRuntime(roomId);
      set((state) => ({
        rooms: {
          ...state.rooms,
          [roomId]: {
            ...state.rooms[roomId],
            enabled,
            status: enabled ? state.rooms[roomId].status : { roomId, state: 'idle' },
            pending: [],
            dropped: 0,
            governanceStats: nextRuntime.stats,
          },
        },
      }));
    },
    removeRoom(roomId) {
      if (!get().rooms[roomId]) return;
      resetDedupe(roomId);
      resetGovernance(roomId);
      set((state) => {
        const rooms = { ...state.rooms };
        delete rooms[roomId];
        return { rooms };
      });
    },
    handleEvent(event) {
      if (event.type === 'status') {
        const roomId = event.status.roomId;
        if (!get().rooms[roomId]) return;
        set((state) => ({
          rooms: {
            ...state.rooms,
            [roomId]: { ...state.rooms[roomId], status: event.status },
          },
        }));
        return;
      }

      const room = get().rooms[event.roomId];
      if (!room?.enabled) return;
      const dedupe = getDedupe(event.roomId);
      const uniqueMessages: DanmakuMessage[] = [];
      for (const message of event.messages) {
        if (message.roomId !== event.roomId || dedupe.ids.has(message.id)) continue;
        dedupe.ids.add(message.id);
        dedupe.order.push(message.id);
        uniqueMessages.push(message);
        if (dedupe.order.length > MAX_DEDUPE_IDS) {
          const oldestId = dedupe.order.shift();
          if (oldestId !== undefined) dedupe.ids.delete(oldestId);
        }
      }

      const governanceResult = applyDanmakuGovernance(
        uniqueMessages,
        getGovernance(event.roomId),
        getGovernanceRuntime(event.roomId),
        Date.now(),
      );
      governanceRuntimeByRoom.set(event.roomId, governanceResult.runtime);

      set((state) => {
        const current = state.rooms[event.roomId];
        if (!current?.enabled) return state;
        const pending = [...current.pending, ...governanceResult.accepted];
        const overflow = Math.max(0, pending.length - MAX_PENDING_DANMAKU);
        if (overflow > 0) pending.splice(0, overflow);
        const governanceStats = {
          ...governanceResult.stats,
          queueOverflow: governanceResult.stats.queueOverflow + overflow,
          upstreamDropped: governanceResult.stats.upstreamDropped + event.dropped,
        };
        governanceRuntimeByRoom.set(event.roomId, {
          ...governanceResult.runtime,
          stats: governanceStats,
        });
        return {
          rooms: {
            ...state.rooms,
            [event.roomId]: {
              ...current,
              pending,
              dropped: current.dropped + event.dropped + overflow,
              governanceStats,
            },
          },
        };
      });
    },
    takePending(roomId, expectedMessageId) {
      let taken = false;
      set((state) => {
        const room = state.rooms[roomId];
        if (!room?.enabled || room.pending[0]?.id !== expectedMessageId) return state;
        taken = true;
        return {
          rooms: {
            ...state.rooms,
            [roomId]: {
              ...room,
              pending: room.pending.slice(1),
            },
          },
        };
      });
      return taken;
    },
    clearGovernanceStats(roomId) {
      const room = get().rooms[roomId];
      if (!room) return;
      const runtime = createDanmakuGovernanceRuntime();
      governanceRuntimeByRoom.set(roomId, runtime);
      set((state) => ({
        rooms: {
          ...state.rooms,
          [roomId]: { ...state.rooms[roomId], governanceStats: runtime.stats },
        },
      }));
    },
  }));
}
