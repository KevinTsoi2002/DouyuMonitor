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
import type { DanmakuMessage } from '../../shared/danmaku-contract';
import { useWorkspace } from './workspace-context';
import {
  createDanmakuStore,
  type DanmakuRoomView,
  type DanmakuState,
} from './danmaku-store';

interface DanmakuContextValue {
  store: StoreApi<DanmakuState>;
  retryRoom(roomId: string): Promise<void>;
}

interface DanmakuProviderProps extends PropsWithChildren {
  source: DanmakuSource;
}

const DanmakuContext = createContext<DanmakuContextValue | null>(null);
const fallbackStore = createDanmakuStore();
const fallbackRooms = new Map<string, DanmakuRoomView>();

function getFallbackRoom(roomId: string): DanmakuRoomView {
  let room = fallbackRooms.get(roomId);
  if (!room) {
    const emptyMessages = Object.freeze([]) as unknown as DanmakuMessage[];
    room = Object.freeze({
      enabled: false,
      status: Object.freeze({ roomId, state: 'idle' as const }),
      pending: emptyMessages,
      dropped: 0,
    });
    fallbackRooms.set(roomId, room);
  }
  return room;
}

export function DanmakuProvider({ source, children }: DanmakuProviderProps) {
  const rooms = useWorkspace((state) => state.rooms);
  const globalDanmakuEnabled = useWorkspace((state) => state.globalDanmakuEnabled);
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
      const shouldTrack = globalDanmakuEnabled && room.danmakuEnabled;
      store.getState().syncRoom(room.roomId, shouldTrack);
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
  }, [globalDanmakuEnabled, rooms, setFailed, source, store]);

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

  const value = useMemo(() => ({ store, retryRoom }), [retryRoom, store]);
  return <DanmakuContext.Provider value={value}>{children}</DanmakuContext.Provider>;
}

export function useDanmakuRoom(roomId: string): DanmakuRoomView {
  const context = useContext(DanmakuContext);
  return useStore(
    context?.store ?? fallbackStore,
    (state) => state.rooms[roomId] ?? getFallbackRoom(roomId),
  );
}

export function useDanmakuControls(): { retryRoom(roomId: string): Promise<void> } {
  const context = useContext(DanmakuContext);
  return useMemo(
    () => ({ retryRoom: context?.retryRoom ?? (async () => {}) }),
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
