import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useMemo,
  useRef,
  type PropsWithChildren,
} from 'react';
import { useStore } from 'zustand';
import type { StoreApi } from 'zustand/vanilla';
import type { DanmakuSource } from '../../infrastructure/renderer-danmaku-source';
import type { DanmakuMessage, DanmakuStatus } from '../../shared/danmaku-contract';
import {
  createDanmakuGovernanceRuntime,
  type DanmakuGovernanceStats,
} from '../danmaku/danmaku-governance';
import { resolveDanmakuGovernance } from '../danmaku/danmaku-settings';
import { useWorkspace } from './workspace-context';
import {
  createDanmakuStore,
  type DanmakuRoomView,
  type DanmakuState,
} from './danmaku-store';

interface DanmakuContextValue {
  store: StoreApi<DanmakuState>;
  retryRoom(roomId: string): Promise<void>;
  clearGovernanceStats(roomId: string): void;
}

interface DanmakuProviderProps extends PropsWithChildren {
  source: DanmakuSource;
}

const DanmakuContext = createContext<DanmakuContextValue | null>(null);
const fallbackStore = createDanmakuStore();
const fallbackRooms = new Map<string, DanmakuRoomView>();
const EMPTY_GOVERNANCE_STATS = createDanmakuGovernanceRuntime().stats;

function getFallbackRoom(roomId: string): DanmakuRoomView {
  let room = fallbackRooms.get(roomId);
  if (!room) {
    const emptyMessages = Object.freeze([]) as unknown as DanmakuMessage[];
    room = Object.freeze({
      enabled: false,
      status: Object.freeze({ roomId, state: 'idle' as const }),
      pending: emptyMessages,
      dropped: 0,
      governanceStats: EMPTY_GOVERNANCE_STATS,
    });
    fallbackRooms.set(roomId, room);
  }
  return room;
}

export function DanmakuProvider({ source, children }: DanmakuProviderProps) {
  const rooms = useWorkspace((state) => state.rooms);
  const globalDanmakuEnabled = useWorkspace((state) => state.globalDanmakuEnabled);
  const governanceDefaults = useWorkspace((state) => state.danmakuSettings.governance);
  const governanceOverrides = useWorkspace((state) => state.danmakuGovernanceOverrides);
  const storeRef = useRef<StoreApi<DanmakuState> | null>(null);
  const trackedRoomsRef = useRef(new Set<string>());
  if (!storeRef.current) storeRef.current = createDanmakuStore();
  const store = storeRef.current;

  const setFailed = useCallback(
    (roomId: string) => {
      store.getState().handleEvent({
        type: 'status',
        status: {
          roomId,
          state: 'failed',
          errorCode: 'NETWORK_UNAVAILABLE',
        },
      });
    },
    [store],
  );

  useEffect(() => {
    let active = true;
    const unsubscribe = source.subscribe((event) => {
      if (active) store.getState().handleEvent(event);
    });
    return () => {
      active = false;
      unsubscribe();
      const trackedRooms = [...trackedRoomsRef.current];
      trackedRoomsRef.current.clear();
      for (const roomId of trackedRooms) {
        void source.stop(roomId).catch(() => undefined);
      }
    };
  }, [source, store]);

  useEffect(() => {
    const currentRoomIds = new Set<string>();
    for (const room of rooms) {
      currentRoomIds.add(room.roomId);
      const shouldTrack = globalDanmakuEnabled
        && room.danmakuEnabled
        && room.online
        && room.status !== 'offline';
      store.getState().syncRoom(
        room.roomId,
        shouldTrack,
        resolveDanmakuGovernance(governanceDefaults, governanceOverrides[room.roomId]),
      );
      if (!shouldTrack) {
        if (trackedRoomsRef.current.has(room.roomId)) {
          trackedRoomsRef.current.delete(room.roomId);
          void source.stop(room.roomId).catch(() => undefined);
        }
        continue;
      }
      if (trackedRoomsRef.current.has(room.roomId)) continue;
      trackedRoomsRef.current.add(room.roomId);
      void source.start(room.roomId).catch(() => setFailed(room.roomId));
    }

    for (const roomId of [...trackedRoomsRef.current]) {
      if (currentRoomIds.has(roomId)) continue;
      trackedRoomsRef.current.delete(roomId);
      void source.stop(roomId).catch(() => undefined);
      store.getState().removeRoom(roomId);
    }
  }, [globalDanmakuEnabled, governanceDefaults, governanceOverrides, rooms, setFailed, source, store]);

  const retryRoom = useCallback(
    async (roomId: string) => {
      store.getState().handleEvent({
        type: 'status',
        status: { roomId, state: 'connecting', attempt: 0 },
      });
      await source.stop(roomId).catch(() => undefined);
      try {
        await source.start(roomId);
      } catch {
        setFailed(roomId);
      }
    },
    [setFailed, source, store],
  );

  const clearGovernanceStats = useCallback((roomId: string) => {
    store.getState().clearGovernanceStats(roomId);
  }, [store]);

  const value = useMemo(
    () => ({ store, retryRoom, clearGovernanceStats }),
    [clearGovernanceStats, retryRoom, store],
  );
  return <DanmakuContext.Provider value={value}>{children}</DanmakuContext.Provider>;
}

export function useDanmakuRoom(roomId: string): DanmakuRoomView {
  const context = useContext(DanmakuContext);
  return useStore(
    context?.store ?? fallbackStore,
    (state) => state.rooms[roomId] ?? getFallbackRoom(roomId),
  );
}

export function useDanmakuStatus(roomId: string): DanmakuStatus {
  const context = useContext(DanmakuContext);
  return useStore(
    context?.store ?? fallbackStore,
    (state) => state.rooms[roomId]?.status ?? getFallbackRoom(roomId).status,
  );
}

export function useDanmakuIssueCount(roomIdsKey: string): number {
  const context = useContext(DanmakuContext);
  const eligibleRoomIdsKey = useWorkspace((state) => {
    if (!state.globalDanmakuEnabled) return '';
    return state.rooms
      .filter((room) => room.danmakuEnabled && room.online && room.status !== 'offline')
      .map((room) => room.roomId)
      .join('|');
  });
  const eligibleRoomIds = new Set(eligibleRoomIdsKey ? eligibleRoomIdsKey.split('|') : []);
  return useStore(
    context?.store ?? fallbackStore,
    (state) => roomIdsKey.split('|').reduce((count, roomId) => {
      const status = state.rooms[roomId]?.status.state;
      return count + (eligibleRoomIds.has(roomId)
        && (status === 'failed' || status === 'platform-blocked') ? 1 : 0);
    }, 0),
  );
}

function mergeGovernanceStats(
  current: DanmakuGovernanceStats,
  next: DanmakuGovernanceStats,
): DanmakuGovernanceStats {
  const level = current.level === 'burst' || next.level === 'burst'
    ? 'burst'
    : current.level === 'crowded' || next.level === 'crowded'
      ? 'crowded'
      : 'normal';
  return {
    level,
    recentRate: current.recentRate + next.recentRate,
    peakRate: Math.max(current.peakRate, next.peakRate),
    filtered: current.filtered + next.filtered,
    duplicates: current.duplicates + next.duplicates,
    rateLimited: current.rateLimited + next.rateLimited,
    queueOverflow: current.queueOverflow + next.queueOverflow,
    upstreamDropped: current.upstreamDropped + next.upstreamDropped,
  };
}

export function useDanmakuGovernanceStats(roomIdsKey: string): DanmakuGovernanceStats {
  const context = useContext(DanmakuContext);
  const rooms = useStore(
    context?.store ?? fallbackStore,
    (state) => state.rooms,
  );
  return useMemo(
    () => roomIdsKey.split('|').filter(Boolean).reduce(
      (stats, roomId) => mergeGovernanceStats(
        stats,
        rooms[roomId]?.governanceStats ?? EMPTY_GOVERNANCE_STATS,
      ),
      EMPTY_GOVERNANCE_STATS,
    ),
    [roomIdsKey, rooms],
  );
}

export function useDanmakuControls(): {
  retryRoom(roomId: string): Promise<void>;
  clearGovernanceStats(roomId: string): void;
} {
  const context = useContext(DanmakuContext);
  return useMemo(
    () => ({
      retryRoom: context?.retryRoom ?? (async () => {}),
      clearGovernanceStats: context?.clearGovernanceStats ?? (() => {}),
    }),
    [context],
  );
}

export function useDanmakuTake(): (roomId: string, messageId: string) => boolean {
  const context = useContext(DanmakuContext);
  return useCallback(
    (roomId: string, messageId: string) => (
      context?.store.getState().takePending(roomId, messageId) ?? false
    ),
    [context],
  );
}
