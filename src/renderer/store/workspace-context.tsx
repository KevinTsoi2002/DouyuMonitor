import { createContext, useContext, useEffect, useRef, type PropsWithChildren } from 'react';
import { useStore } from 'zustand';
import type { RoomCandidate, DouyuAdapter } from '../../domain/douyu-adapter';
import { createWorkspaceStore, type WorkspaceState } from './workspace-store';
import type { StoreApi } from 'zustand/vanilla';

const WorkspaceStoreContext = createContext<StoreApi<WorkspaceState> | null>(null);

interface WorkspaceProviderProps extends PropsWithChildren {
  adapter: DouyuAdapter;
  demoMode: boolean;
  initialRooms?: RoomCandidate[];
  initialSidebarOpen?: boolean;
}

export function WorkspaceProvider({
  adapter,
  demoMode,
  initialRooms,
  initialSidebarOpen,
  children,
}: WorkspaceProviderProps) {
  const storeRef = useRef<StoreApi<WorkspaceState> | null>(null);
  if (!storeRef.current) {
    storeRef.current = createWorkspaceStore(adapter, {
      demoMode,
      initialRooms,
      initialSidebarOpen,
    });
  }

  useEffect(() => {
    if (demoMode) return;
    const store = storeRef.current;
    if (!store) return;
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
