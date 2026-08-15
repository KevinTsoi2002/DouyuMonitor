import { createStore, type StoreApi } from 'zustand/vanilla';
import {
  DEFAULT_PRIMARY_ROOM_RATIO,
  type LayoutId,
  type PrimaryRoomRatio,
} from '../../domain/layout-engine';
import { resolveRoomInput } from '../../domain/input-resolver';
import type {
  DouyuAdapter,
  RoomCandidate,
  RoomStatus,
  StreamAvailability,
  StreamQuality,
} from '../../domain/douyu-adapter';
import { DouyuAdapterError } from '../../domain/douyu-adapter';
import { RoomRegistry } from '../../domain/room-registry';
import {
  DEFAULT_DANMAKU_SETTINGS,
  parseDanmakuGovernanceSettings,
  parseDanmakuSettings,
  type DanmakuGovernanceOverride,
  type DanmakuGovernanceSettings,
  type DanmakuSettings,
} from '../danmaku/danmaku-settings';
import {
  loadWorkspaceSnapshot,
  saveWorkspaceSnapshot,
  type WorkspaceSnapshot,
  type WorkspaceStorage,
} from './workspace-persistence';
import {
  addHistoryEntry,
  addRoomIdToGroup,
  DEFAULT_ROOM_VOLUME,
  toggleRoomId,
  type LibraryRoom,
  type RoomGroup,
  type RoomHistoryEntry,
  type RoomLibrary,
} from './room-library';
import {
  moveRoomPlacement,
  nextPrimaryAfterRemoval,
  normalizeRoomPlacementOrder,
  swapPrimaryRoomPlacement,
} from './room-placement';

export type PlaybackAvailabilityStatus = 'checking' | 'available' | 'blocked' | 'error';

export interface PlaybackRecoveryReport {
  attempt: number;
  exhausted: boolean;
  errorCode: string;
}

export interface PlaybackRecoveryDiagnostic extends PlaybackRecoveryReport {
  updatedAt: string;
}

export interface RoomSession extends RoomCandidate {
  status: RoomStatus;
  quality: StreamQuality;
  volume: number;
  danmakuEnabled: boolean;
  playbackAvailabilityStatus: PlaybackAvailabilityStatus;
  streamAvailability?: StreamAvailability;
  playbackError?: string;
  playbackErrorCode?: string;
  playbackCheckedAt?: string;
  playbackRecovery?: PlaybackRecoveryDiagnostic;
}

export type SearchStatus = 'idle' | 'searching' | 'success' | 'empty' | 'error';

export interface WorkspaceState {
  rooms: RoomSession[];
  roomLibrary: RoomLibrary;
  history: RoomHistoryEntry[];
  favoriteRoomIds: string[];
  groups: RoomGroup[];
  activeGroupId?: string;
  demoMode: boolean;
  layoutId: LayoutId;
  primaryRoomId?: string;
  roomPlacementOrder: string[];
  primaryRoomRatio: PrimaryRoomRatio;
  audioRoomId?: string;
  globalDanmakuEnabled: boolean;
  globalMuted: boolean;
  danmakuSettings: DanmakuSettings;
  danmakuGovernanceOverrides: Record<string, DanmakuGovernanceOverride>;
  sidebarOpen: boolean;
  searchResults: RoomCandidate[];
  searchStatus: SearchStatus;
  searchError?: string;
  addRoom: (candidate: RoomCandidate) => 'added' | 'duplicate' | 'limit';
  removeRoom: (roomId: string) => void;
  moveRoom: (roomId: string, delta: -1 | 1) => void;
  reorderRooms: (sourceRoomId: string, targetRoomId: string) => void;
  setLayout: (layoutId: LayoutId) => void;
  setPrimaryRoom: (roomId: string) => void;
  setPrimaryRoomRatio: (ratio: PrimaryRoomRatio) => void;
  setAudioRoom: (roomId?: string) => void;
  setQuality: (roomId: string, quality: StreamQuality) => void;
  setVolume: (roomId: string, volume: number) => void;
  toggleDanmaku: (roomId: string) => void;
  setGlobalDanmakuEnabled: (enabled: boolean) => void;
  setGlobalMuted: (muted: boolean) => void;
  setDanmakuSettings: (patch: Partial<DanmakuSettings>) => void;
  setDanmakuGovernance: (patch: DanmakuGovernanceOverride) => void;
  setRoomDanmakuGovernanceOverride: (roomId: string, patch: DanmakuGovernanceOverride) => void;
  clearRoomDanmakuGovernanceOverride: (roomId: string) => void;
  resetDanmakuSetting: (key: 'durationSeconds' | 'fontSize' | 'opacity') => void;
  setSidebarOpen: (open: boolean) => void;
  toggleFavorite: (roomId: string) => void;
  createGroup: (name: string) => string | undefined;
  renameGroup: (groupId: string, name: string) => boolean;
  deleteGroup: (groupId: string) => void;
  addRoomToGroup: (groupId: string, roomId: string) => 'added' | 'duplicate' | 'limit' | 'missing';
  removeRoomFromGroup: (groupId: string, roomId: string) => void;
  moveGroupRoom: (groupId: string, roomId: string, delta: -1 | 1) => void;
  switchGroup: (groupId: string) => void;
  searchRooms: (input: string) => Promise<void>;
  refreshRoomMetadata: (roomId: string) => Promise<boolean>;
  refreshStreamAvailability: (roomId: string) => Promise<void>;
  reportPlaybackRecovery: (roomId: string, report?: PlaybackRecoveryReport) => void;
}

export interface WorkspaceOptions {
  initialRooms?: RoomCandidate[];
  demoMode?: boolean;
  storage?: WorkspaceStorage;
  initialSidebarOpen?: boolean;
  now?: () => Date;
  createGroupId?: () => string;
}

function toLibraryRoom(candidate: RoomCandidate & Partial<LibraryRoom>): LibraryRoom {
  return {
    ...candidate,
    quality: candidate.quality ?? 'auto',
    volume: candidate.volume ?? DEFAULT_ROOM_VOLUME,
    danmakuEnabled: candidate.danmakuEnabled ?? true,
  };
}

function toSession(candidate: LibraryRoom): RoomSession {
  return {
    ...candidate,
    status: candidate.online ? 'playing' : 'offline',
    playbackAvailabilityStatus: 'checking',
  };
}

function normalizeGovernanceOverride(
  base: DanmakuGovernanceSettings,
  patch: DanmakuGovernanceOverride,
): DanmakuGovernanceOverride {
  const parsed = parseDanmakuGovernanceSettings({ ...base, ...patch });
  const normalized: DanmakuGovernanceOverride = {};
  if ('enabled' in patch) normalized.enabled = parsed.enabled;
  if ('keywordBlacklist' in patch) normalized.keywordBlacklist = parsed.keywordBlacklist;
  if ('duplicateWindowSeconds' in patch) {
    normalized.duplicateWindowSeconds = parsed.duplicateWindowSeconds;
  }
  if ('peakProtectionEnabled' in patch) {
    normalized.peakProtectionEnabled = parsed.peakProtectionEnabled;
  }
  return normalized;
}

export function createWorkspaceStore(
  adapter: DouyuAdapter,
  options: WorkspaceOptions = {},
): StoreApi<WorkspaceState> {
  const registry = new RoomRegistry();
  const storage = options.storage ?? (typeof globalThis.localStorage === 'undefined' ? undefined : globalThis.localStorage);
  const now = options.now ?? (() => new Date());
  const createGroupId = options.createGroupId ?? (() => globalThis.crypto.randomUUID());
  const metadataRefreshInFlight = new Set<string>();
  const persisted = loadWorkspaceSnapshot(storage);
  const initialRoomLibrary: RoomLibrary = persisted?.roomLibrary ?? Object.fromEntries(
    (options.initialRooms ?? []).slice(0, RoomRegistry.MAX_ROOMS).map((room) => {
      const libraryRoom = toLibraryRoom(room);
      return [libraryRoom.roomId, libraryRoom];
    }),
  );
  const initialRoomIds = persisted?.activeRoomIds
    ?? (options.initialRooms ?? []).slice(0, RoomRegistry.MAX_ROOMS).map((room) => room.roomId);
  const initialSessions = initialRoomIds.flatMap((roomId) => {
    const room = initialRoomLibrary[roomId];
    return room && registry.add({ roomId: room.roomId, anchorName: room.anchorName }).added
      ? [toSession(room)]
      : [];
  });
  const initialPlacementOrder = normalizeRoomPlacementOrder(
    initialSessions.map((room) => room.roomId),
    persisted?.roomPlacementOrder,
  );

  const hasRoom = (roomId: string | undefined): roomId is string => (
    roomId !== undefined && initialSessions.some((room) => room.roomId === roomId)
  );

  const store = createStore<WorkspaceState>((set, get) => {
    const persist = () => {
      const state = get();
      const snapshot: WorkspaceSnapshot = {
        schemaVersion: 4,
        roomLibrary: state.roomLibrary,
        activeRoomIds: state.rooms.map((room) => room.roomId),
        history: state.history,
        favoriteRoomIds: state.favoriteRoomIds,
        groups: state.groups,
        activeGroupId: state.activeGroupId,
        layoutId: state.layoutId,
        primaryRoomId: state.primaryRoomId,
        roomPlacementOrder: state.roomPlacementOrder,
        primaryRoomRatio: state.primaryRoomRatio,
        audioRoomId: state.audioRoomId,
        globalDanmakuEnabled: state.globalDanmakuEnabled,
        globalMuted: state.globalMuted,
        danmakuSettings: state.danmakuSettings,
        danmakuGovernanceOverrides: state.danmakuGovernanceOverrides,
        sidebarOpen: state.sidebarOpen,
      };
      saveWorkspaceSnapshot(storage, snapshot);
    };

    return ({
    rooms: initialSessions,
    roomLibrary: initialRoomLibrary,
    history: persisted?.history ?? [],
    favoriteRoomIds: persisted?.favoriteRoomIds ?? [],
    groups: persisted?.groups ?? [],
    activeGroupId: persisted?.activeGroupId,
    demoMode: options.demoMode ?? false,
    layoutId: persisted?.layoutId ?? 'auto',
    primaryRoomId: hasRoom(persisted?.primaryRoomId) ? persisted?.primaryRoomId : initialSessions[0]?.roomId,
    roomPlacementOrder: initialPlacementOrder,
    primaryRoomRatio: persisted?.primaryRoomRatio ?? DEFAULT_PRIMARY_ROOM_RATIO,
    audioRoomId: hasRoom(persisted?.audioRoomId) ? persisted?.audioRoomId : initialSessions[0]?.roomId,
    globalDanmakuEnabled: persisted?.globalDanmakuEnabled ?? true,
    globalMuted: persisted?.globalMuted ?? false,
    danmakuSettings: persisted?.danmakuSettings ?? { ...DEFAULT_DANMAKU_SETTINGS },
    danmakuGovernanceOverrides: persisted?.danmakuGovernanceOverrides ?? {},
    sidebarOpen: persisted?.sidebarOpen ?? options.initialSidebarOpen ?? true,
    searchResults: [],
    searchStatus: 'idle',
    searchError: undefined,

    addRoom(candidate) {
      const result = registry.add({ roomId: candidate.roomId, anchorName: candidate.anchorName });
      if (!result.added) {
        if (result.reason === 'duplicate' && get().roomLibrary[candidate.roomId]) {
          set((state) => ({
            history: addHistoryEntry(state.history, candidate.roomId, now().toISOString()),
          }));
          persist();
        }
        return result.reason;
      }

      const existing = get().roomLibrary[candidate.roomId];
      const libraryRoom = toLibraryRoom({ ...existing, ...candidate });
      const session = toSession(libraryRoom);
      set((state) => ({
        rooms: [...state.rooms, session],
        roomPlacementOrder: [...state.roomPlacementOrder, session.roomId],
        roomLibrary: { ...state.roomLibrary, [libraryRoom.roomId]: libraryRoom },
        history: addHistoryEntry(state.history, libraryRoom.roomId, now().toISOString()),
        activeGroupId: undefined,
        primaryRoomId: state.primaryRoomId ?? session.roomId,
        audioRoomId: state.audioRoomId ?? session.roomId,
      }));
      persist();
      if (candidate.online) void get().refreshStreamAvailability(candidate.roomId);
      return 'added';
    },

    removeRoom(roomId) {
      if (!registry.remove(roomId)) return;
      set((state) => {
        const rooms = state.rooms.filter((room) => room.roomId !== roomId);
        const nextPrimaryRoomId = nextPrimaryAfterRemoval(state.roomPlacementOrder, roomId);
        return {
          rooms,
          roomPlacementOrder: state.roomPlacementOrder.filter((id) => id !== roomId),
          danmakuGovernanceOverrides: Object.fromEntries(
            Object.entries(state.danmakuGovernanceOverrides)
              .filter(([id]) => id !== roomId),
          ),
          activeGroupId: undefined,
          primaryRoomId: state.primaryRoomId === roomId ? nextPrimaryRoomId : state.primaryRoomId,
          audioRoomId: state.audioRoomId === roomId ? rooms[0]?.roomId : state.audioRoomId,
        };
      });
      persist();
    },

    moveRoom(roomId, delta) {
      set((state) => {
        const from = state.rooms.findIndex((room) => room.roomId === roomId);
        const to = from + delta;
        if (from < 0 || to < 0 || to >= state.rooms.length) return state;
        const targetRoomId = state.rooms[to].roomId;
        const rooms = [...state.rooms];
        const [moved] = rooms.splice(from, 1);
        rooms.splice(to, 0, moved);
        return {
          rooms,
          roomPlacementOrder: moveRoomPlacement(state.roomPlacementOrder, roomId, targetRoomId),
          activeGroupId: undefined,
        };
      });
      persist();
    },

    reorderRooms(sourceRoomId, targetRoomId) {
      set((state) => {
        const from = state.rooms.findIndex((room) => room.roomId === sourceRoomId);
        const to = state.rooms.findIndex((room) => room.roomId === targetRoomId);
        if (from < 0 || to < 0 || from === to) return state;
        const rooms = [...state.rooms];
        const [moved] = rooms.splice(from, 1);
        rooms.splice(to, 0, moved);
        return {
          rooms,
          roomPlacementOrder: moveRoomPlacement(
            state.roomPlacementOrder,
            sourceRoomId,
            targetRoomId,
          ),
          activeGroupId: undefined,
        };
      });
      persist();
    },

    setLayout(layoutId) {
      set({ layoutId });
      persist();
    },

    setPrimaryRoom(roomId) {
      if (!get().rooms.some((room) => room.roomId === roomId)) return;
      set((state) => ({
        primaryRoomId: roomId,
        roomPlacementOrder: swapPrimaryRoomPlacement(
          state.roomPlacementOrder,
          state.primaryRoomId,
          roomId,
        ),
      }));
      persist();
    },

    setPrimaryRoomRatio(primaryRoomRatio) {
      set({ primaryRoomRatio });
      persist();
    },

    setAudioRoom(roomId) {
      if (roomId === undefined || get().rooms.some((room) => room.roomId === roomId)) {
        set({ audioRoomId: roomId });
        persist();
      }
    },

    setQuality(roomId, quality) {
      set((state) => ({
        rooms: state.rooms.map((room) => (room.roomId === roomId ? { ...room, quality } : room)),
        roomLibrary: state.roomLibrary[roomId]
          ? { ...state.roomLibrary, [roomId]: { ...state.roomLibrary[roomId], quality } }
          : state.roomLibrary,
      }));
      persist();
    },

    setVolume(roomId, volume) {
      if (!Number.isFinite(volume)) return;
      const nextVolume = Math.min(1, Math.max(0, volume));
      set((state) => ({
        rooms: state.rooms.map((room) => (room.roomId === roomId ? { ...room, volume: nextVolume } : room)),
        roomLibrary: state.roomLibrary[roomId]
          ? { ...state.roomLibrary, [roomId]: { ...state.roomLibrary[roomId], volume: nextVolume } }
          : state.roomLibrary,
      }));
      persist();
    },

    toggleDanmaku(roomId) {
      set((state) => ({
        rooms: state.rooms.map((room) =>
          room.roomId === roomId ? { ...room, danmakuEnabled: !room.danmakuEnabled } : room,
        ),
        roomLibrary: state.roomLibrary[roomId]
          ? {
              ...state.roomLibrary,
              [roomId]: {
                ...state.roomLibrary[roomId],
                danmakuEnabled: !state.roomLibrary[roomId].danmakuEnabled,
              },
            }
          : state.roomLibrary,
      }));
      persist();
    },

    setGlobalDanmakuEnabled(enabled) {
      set({ globalDanmakuEnabled: enabled });
      persist();
    },

    setGlobalMuted(muted) {
      set({ globalMuted: muted });
      persist();
    },

    setDanmakuSettings(patch) {
      set((state) => ({
        danmakuSettings: parseDanmakuSettings({ ...state.danmakuSettings, ...patch }),
      }));
      persist();
    },

    setDanmakuGovernance(patch) {
      set((state) => ({
        danmakuSettings: parseDanmakuSettings({
          ...state.danmakuSettings,
          governance: {
            ...state.danmakuSettings.governance,
            ...patch,
          },
        }),
      }));
      persist();
    },

    setRoomDanmakuGovernanceOverride(roomId, patch) {
      if (!get().roomLibrary[roomId]) return;
      set((state) => {
        const normalized = normalizeGovernanceOverride(
          state.danmakuSettings.governance,
          patch,
        );
        const current = state.danmakuGovernanceOverrides[roomId] ?? {};
        const next = { ...current, ...normalized };
        return {
          danmakuGovernanceOverrides: Object.keys(next).length > 0
            ? { ...state.danmakuGovernanceOverrides, [roomId]: next }
            : state.danmakuGovernanceOverrides,
        };
      });
      persist();
    },

    clearRoomDanmakuGovernanceOverride(roomId) {
      if (!get().danmakuGovernanceOverrides[roomId]) return;
      set((state) => {
        const overrides = { ...state.danmakuGovernanceOverrides };
        delete overrides[roomId];
        return { danmakuGovernanceOverrides: overrides };
      });
      persist();
    },

    resetDanmakuSetting(key) {
      set((state) => ({
        danmakuSettings: {
          ...state.danmakuSettings,
          [key]: DEFAULT_DANMAKU_SETTINGS[key],
        },
      }));
      persist();
    },

    setSidebarOpen(open) {
      set({ sidebarOpen: open });
      persist();
    },

    toggleFavorite(roomId) {
      if (!get().roomLibrary[roomId]) return;
      set((state) => ({ favoriteRoomIds: toggleRoomId(state.favoriteRoomIds, roomId) }));
      persist();
    },

    createGroup(name) {
      const trimmedName = name.trim();
      if (!trimmedName) return undefined;
      const id = createGroupId();
      if (!id || get().groups.some((group) => group.id === id)) return undefined;
      set((state) => ({
        groups: [...state.groups, {
          id,
          name: trimmedName,
          roomIds: [],
          createdAt: now().toISOString(),
        }],
      }));
      persist();
      return id;
    },

    renameGroup(groupId, name) {
      const trimmedName = name.trim();
      if (!trimmedName || !get().groups.some((group) => group.id === groupId)) return false;
      set((state) => ({
        groups: state.groups.map((group) => (
          group.id === groupId ? { ...group, name: trimmedName } : group
        )),
      }));
      persist();
      return true;
    },

    deleteGroup(groupId) {
      if (!get().groups.some((group) => group.id === groupId)) return;
      set((state) => ({
        groups: state.groups.filter((group) => group.id !== groupId),
        activeGroupId: state.activeGroupId === groupId ? undefined : state.activeGroupId,
      }));
      persist();
    },

    addRoomToGroup(groupId, roomId) {
      const state = get();
      const group = state.groups.find((item) => item.id === groupId);
      if (!group || !state.roomLibrary[roomId]) return 'missing';
      const addition = addRoomIdToGroup(group.roomIds, roomId);
      if (addition.result !== 'added') return addition.result;
      set((current) => ({
        groups: current.groups.map((item) => (
          item.id === groupId ? { ...item, roomIds: addition.roomIds } : item
        )),
        activeGroupId: current.activeGroupId === groupId ? undefined : current.activeGroupId,
      }));
      persist();
      return 'added';
    },

    removeRoomFromGroup(groupId, roomId) {
      const group = get().groups.find((item) => item.id === groupId);
      if (!group || !group.roomIds.includes(roomId)) return;
      set((state) => ({
        groups: state.groups.map((item) => (
          item.id === groupId
            ? { ...item, roomIds: item.roomIds.filter((id) => id !== roomId) }
            : item
        )),
        activeGroupId: state.activeGroupId === groupId ? undefined : state.activeGroupId,
      }));
      persist();
    },

    moveGroupRoom(groupId, roomId, delta) {
      const group = get().groups.find((item) => item.id === groupId);
      if (!group) return;
      const from = group.roomIds.indexOf(roomId);
      const to = from + delta;
      if (from < 0 || to < 0 || to >= group.roomIds.length) return;
      const roomIds = [...group.roomIds];
      const [moved] = roomIds.splice(from, 1);
      roomIds.splice(to, 0, moved);
      set((state) => ({
        groups: state.groups.map((item) => item.id === groupId ? { ...item, roomIds } : item),
        activeGroupId: state.activeGroupId === groupId ? undefined : state.activeGroupId,
      }));
      persist();
    },

    switchGroup(groupId) {
      const state = get();
      const group = state.groups.find((item) => item.id === groupId);
      if (!group) return;
      const rooms = group.roomIds.flatMap((roomId) => {
        const room = state.roomLibrary[roomId];
        return room ? [toSession(room)] : [];
      });
      for (const room of state.rooms) registry.remove(room.roomId);
      for (const room of rooms) {
        registry.add({ roomId: room.roomId, anchorName: room.anchorName });
      }
      const roomPlacementOrder = normalizeRoomPlacementOrder(
        rooms.map((room) => room.roomId),
        state.roomPlacementOrder,
      );
      const primaryRoomId = state.primaryRoomId && roomPlacementOrder.includes(state.primaryRoomId)
        ? state.primaryRoomId
        : roomPlacementOrder[0];
      set({
        rooms,
        roomPlacementOrder,
        activeGroupId: groupId,
        primaryRoomId,
        audioRoomId: rooms[0]?.roomId,
      });
      persist();
      for (const room of rooms.filter((item) => item.online)) {
        void get().refreshStreamAvailability(room.roomId);
      }
    },

    async searchRooms(rawInput) {
      set({ searchStatus: 'searching', searchResults: [], searchError: undefined });
      try {
        const input = resolveRoomInput(rawInput);
        const searchResults = await adapter.search(input);
        set({ searchResults, searchStatus: searchResults.length ? 'success' : 'empty' });
      } catch (error) {
        set({ searchStatus: 'error', searchError: error instanceof Error ? error.message : '搜索失败' });
      }
    },

    async refreshRoomMetadata(roomId) {
      if (!get().roomLibrary[roomId] || metadataRefreshInFlight.has(roomId)) return false;
      metadataRefreshInFlight.add(roomId);
      try {
        const [candidate] = await adapter.search({ type: 'room-id', value: roomId });
        const existing = get().roomLibrary[roomId];
        if (!candidate || !existing) return false;

        const libraryRoom = toLibraryRoom({ ...existing, ...candidate });
        set((state) => ({
          rooms: state.rooms.map((room) => room.roomId === roomId
            ? {
                ...room,
                ...candidate,
                status: candidate.online
                  ? room.status === 'offline' ? 'playing' : room.status
                  : 'offline',
                quality: room.quality,
                volume: room.volume,
                danmakuEnabled: room.danmakuEnabled,
              }
            : room),
          roomLibrary: { ...state.roomLibrary, [roomId]: libraryRoom },
        }));
        persist();
        if (candidate.online) void get().refreshStreamAvailability(roomId);
        return true;
      } catch (error) {
        const checkedAt = now().toISOString();
        set((state) => ({
          rooms: state.rooms.map((room) => {
            if (room.roomId !== roomId || !room.online || room.status === 'offline') return room;
            return {
              ...room,
              playbackAvailabilityStatus: 'error',
              playbackCheckedAt: checkedAt,
              playbackRecovery: undefined,
              playbackError: '直播间状态检查失败',
              playbackErrorCode: error instanceof DouyuAdapterError
                ? error.code
                : 'ROOM_METADATA_CHECK_FAILED',
            };
          }),
        }));
        return false;
      } finally {
        metadataRefreshInFlight.delete(roomId);
      }
    },

    async refreshStreamAvailability(roomId) {
      const room = get().rooms.find((item) => item.roomId === roomId);
      if (!room || !room.online || room.status === 'offline') return;

      set((state) => ({
        rooms: state.rooms.map((room) => room.roomId === roomId
          ? {
              ...room,
              playbackAvailabilityStatus: 'checking',
              playbackError: undefined,
              playbackErrorCode: undefined,
              playbackRecovery: undefined,
            }
          : room),
      }));

      try {
        const availability = await adapter.getStreamAvailability(roomId);
        if (!get().rooms.some((room) => room.roomId === roomId)) return;

        set((state) => ({
          rooms: state.rooms.map((room) => room.roomId === roomId
            ? {
                ...room,
                playbackAvailabilityStatus: availability.kind,
                streamAvailability: availability,
                playbackError: undefined,
                playbackErrorCode: undefined,
                playbackCheckedAt: availability.checkedAt,
                playbackRecovery: undefined,
              }
            : room),
        }));
      } catch (error) {
        if (!get().rooms.some((room) => room.roomId === roomId)) return;

        set((state) => ({
          rooms: state.rooms.map((room) => room.roomId === roomId
            ? {
                ...room,
                playbackAvailabilityStatus: 'error',
                playbackCheckedAt: now().toISOString(),
                playbackRecovery: undefined,
                playbackError: error instanceof Error ? error.message : '播放能力检查失败',
                playbackErrorCode: error instanceof DouyuAdapterError
                  ? error.code
                  : 'PLAYBACK_SOURCE_CHECK_FAILED',
              }
            : room),
        }));
      }
    },

    reportPlaybackRecovery(roomId, report) {
      if (!get().rooms.some((room) => room.roomId === roomId)) return;
      set((state) => ({
        rooms: state.rooms.map((room) => room.roomId === roomId
          ? {
              ...room,
              playbackRecovery: report
                ? { ...report, updatedAt: now().toISOString() }
                : undefined,
            }
          : room),
      }));
    },
    });
  });

  for (const room of initialSessions.filter((item) => item.online)) {
    void store.getState().refreshStreamAvailability(room.roomId);
  }

  return store;
}
