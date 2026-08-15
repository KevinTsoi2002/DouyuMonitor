import { createContext, useContext, useEffect, useRef, type PropsWithChildren } from 'react';
import { useStore } from 'zustand';
import type { RoomCandidate, DouyuAdapter } from '../../domain/douyu-adapter';
import { createWorkspaceStore, type WorkspaceState } from './workspace-store';
import { createWorkspaceRefreshScheduler } from './workspace-refresh';
import type { StoreApi } from 'zustand/vanilla';
import type { WorkspaceStorage } from './workspace-persistence';

const WorkspaceStoreContext = createContext<StoreApi<WorkspaceState> | null>(null);

interface WorkspaceProviderProps extends PropsWithChildren {
  adapter: DouyuAdapter;
  demoMode: boolean;
  initialRooms?: RoomCandidate[];
  initialSidebarOpen?: boolean;
  storage?: WorkspaceStorage;
}

export function WorkspaceProvider({
  adapter,
  demoMode,
  initialRooms,
  initialSidebarOpen,
  storage,
  children,
}: WorkspaceProviderProps) {
  const storeRef = useRef<StoreApi<WorkspaceState> | null>(null);
  if (!storeRef.current) {
    storeRef.current = createWorkspaceStore(adapter, {
      demoMode,
      initialRooms,
      initialSidebarOpen,
      storage,
    });
  }
  const store = storeRef.current;
  const roomIds = useStore(store, (state) => state.rooms.map((room) => room.roomId).sort().join('|'));

  useEffect(() => {
    if (demoMode) return;
    const scheduler = createWorkspaceRefreshScheduler({
      getRooms: () => store.getState().rooms.map((room) => ({
        roomId: room.roomId,
        online: room.online,
        status: room.status,
      })),
      refreshRoomMetadata: (roomId) => store.getState().refreshRoomMetadata(roomId),
    });
    scheduler.sync();
    return () => scheduler.dispose();
  }, [demoMode, roomIds, store]);

  useEffect(() => {
    if (demoMode) return;
    const missingAvatarRoomIds = store.getState().rooms
      .filter((room) => !room.avatarUrl)
      .map((room) => room.roomId);
    for (const roomId of missingAvatarRoomIds) {
      void store.getState().refreshRoomMetadata(roomId);
    }
  }, [demoMode]);

  return (
    <WorkspaceStoreContext.Provider value={storeRef.current}>
      {children}
    </WorkspaceStoreContext.Provider>
  );
}

export function useWorkspace<T>(selector: (state: WorkspaceState) => T): T {
  const store = useContext(WorkspaceStoreContext);
  if (!store) throw new Error('WorkspaceProvider is required');
  return useStore(store, selector);
}
