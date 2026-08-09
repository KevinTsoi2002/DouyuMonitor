import { createStore, type StoreApi } from 'zustand/vanilla';
import type {
  DanmakuEvent,
  DanmakuMessage,
  DanmakuStatus,
} from '../../shared/danmaku-contract';

export const MAX_PENDING_DANMAKU = 300;
const MAX_DEDUPE_IDS = MAX_PENDING_DANMAKU * 2;

export interface DanmakuRoomView {
  enabled: boolean;
  status: DanmakuStatus;
  pending: DanmakuMessage[];
  dropped: number;
}

export interface DanmakuState {
  rooms: Record<string, DanmakuRoomView>;
  syncRoom(roomId: string, enabled: boolean): void;
  removeRoom(roomId: string): void;
  handleEvent(event: DanmakuEvent): void;
  takePending(roomId: string, expectedMessageId: string): boolean;
}

interface DedupeState {
  ids: Set<string>;
  order: string[];
}

function createRoom(roomId: string, enabled: boolean): DanmakuRoomView {
  return {
    enabled,
    status: { roomId, state: 'idle' },
    pending: [],
    dropped: 0,
  };
}

export function createDanmakuStore(): StoreApi<DanmakuState> {
  const dedupeByRoom = new Map<string, DedupeState>();

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

  return createStore<DanmakuState>((set, get) => ({
    rooms: {},
    syncRoom(roomId, enabled) {
      const existing = get().rooms[roomId];
      if (!existing) {
        set((state) => ({
          rooms: { ...state.rooms, [roomId]: createRoom(roomId, enabled) },
        }));
        return;
      }
      if (existing.enabled === enabled) return;
      if (!enabled) resetDedupe(roomId);
      set((state) => ({
        rooms: {
          ...state.rooms,
          [roomId]: {
            ...state.rooms[roomId],
            enabled,
            pending: [],
            dropped: 0,
          },
        },
      }));
    },
    removeRoom(roomId) {
      if (!get().rooms[roomId]) return;
      resetDedupe(roomId);
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
      const accepted: DanmakuMessage[] = [];
      for (const message of event.messages) {
        if (message.roomId !== event.roomId || dedupe.ids.has(message.id)) continue;
        dedupe.ids.add(message.id);
        dedupe.order.push(message.id);
        accepted.push(message);
        if (dedupe.order.length > MAX_DEDUPE_IDS) {
          const oldestId = dedupe.order.shift();
          if (oldestId !== undefined) dedupe.ids.delete(oldestId);
        }
      }

      set((state) => {
        const current = state.rooms[event.roomId];
        if (!current?.enabled) return state;
        const pending = [...current.pending, ...accepted];
        const overflow = Math.max(0, pending.length - MAX_PENDING_DANMAKU);
        if (overflow > 0) pending.splice(0, overflow);
        return {
          rooms: {
            ...state.rooms,
            [event.roomId]: {
              ...current,
              pending,
              dropped: current.dropped + event.dropped + overflow,
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
  }));
}
