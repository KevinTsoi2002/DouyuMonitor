import type { LayoutId } from '../../domain/layout-engine';
import type { StreamQuality } from '../../domain/douyu-adapter';
import {
  parseDanmakuSettings,
  type DanmakuSettings,
} from '../danmaku/danmaku-settings';
import {
  MAX_GROUP_ROOMS,
  MAX_HISTORY_ROOMS,
  DEFAULT_ROOM_VOLUME,
  type LibraryRoom,
  type RoomGroup,
  type RoomHistoryEntry,
  type RoomLibrary,
} from './room-library';

export const WORKSPACE_STORAGE_KEY = 'douyu-monitor.workspace.v1';
export const WORKSPACE_SCHEMA_VERSION = 3;

export interface WorkspaceStorage {
  getItem(key: string): string | null;
  setItem(key: string, value: string): void;
  removeItem(key: string): void;
}

export type PersistedRoom = LibraryRoom;

export interface WorkspaceSnapshot {
  schemaVersion: typeof WORKSPACE_SCHEMA_VERSION;
  roomLibrary: RoomLibrary;
  activeRoomIds: string[];
  history: RoomHistoryEntry[];
  favoriteRoomIds: string[];
  groups: RoomGroup[];
  activeGroupId?: string;
  layoutId: LayoutId;
  primaryRoomId?: string;
  audioRoomId?: string;
  globalDanmakuEnabled: boolean;
  globalMuted: boolean;
  danmakuSettings: DanmakuSettings;
  sidebarOpen?: boolean;
}

const QUALITY_VALUES = new Set<StreamQuality>(['auto', 'original', 'super', 'high', 'standard']);
const LAYOUT_VALUES = new Set([
  'auto',
  'single',
  'grid-2x2',
  'grid-3x2',
  'grid-3x3',
  'primary-two',
  'split-horizontal',
  'split-vertical',
]);

function getDefaultStorage(): WorkspaceStorage | undefined {
  if (typeof globalThis.localStorage === 'undefined') return undefined;
  return globalThis.localStorage;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return Boolean(value) && typeof value === 'object' && !Array.isArray(value);
}

function readString(value: unknown): string | undefined {
  return typeof value === 'string' && value.trim().length > 0 ? value : undefined;
}

function readHttpUrl(value: unknown): string | undefined {
  const text = readString(value);
  if (!text) return undefined;
  try {
    const url = new URL(text);
    return url.protocol === 'http:' || url.protocol === 'https:' ? url.toString() : undefined;
  } catch {
    return undefined;
  }
}

function parseRoom(value: unknown): PersistedRoom | undefined {
  if (!isRecord(value)) return undefined;
  const roomId = readString(value.roomId);
  const anchorName = readString(value.anchorName);
  const title = readString(value.title);
  const category = readString(value.category);
  const viewerLabel = readString(value.viewerLabel);
  if (!roomId || !anchorName || !title || !category || !viewerLabel || typeof value.online !== 'boolean') {
    return undefined;
  }
  const quality = QUALITY_VALUES.has(value.quality as StreamQuality)
    ? value.quality as StreamQuality
    : 'auto';
  const volume = typeof value.volume === 'number' && Number.isFinite(value.volume)
    ? Math.min(1, Math.max(0, value.volume))
    : DEFAULT_ROOM_VOLUME;
  const avatarUrl = readHttpUrl(value.avatarUrl);
  return {
    roomId,
    anchorName,
    ...(avatarUrl ? { avatarUrl } : {}),
    title,
    category,
    online: value.online,
    viewerLabel,
    quality,
    danmakuEnabled: value.danmakuEnabled !== false,
    volume,
  };
}

function toRoomLibrary(value: unknown): RoomLibrary | undefined {
  if (!isRecord(value)) return undefined;
  const roomLibrary: RoomLibrary = {};
  for (const item of Object.values(value)) {
    const room = parseRoom(item);
    if (room && !(room.roomId in roomLibrary)) roomLibrary[room.roomId] = room;
  }
  return roomLibrary;
}

function roomsToLibrary(value: unknown): { roomLibrary: RoomLibrary; activeRoomIds: string[] } | undefined {
  if (!Array.isArray(value)) return undefined;
  const roomLibrary: RoomLibrary = {};
  const activeRoomIds: string[] = [];
  for (const item of value.slice(0, 9)) {
    const room = parseRoom(item);
    if (!room || room.roomId in roomLibrary) continue;
    roomLibrary[room.roomId] = room;
    activeRoomIds.push(room.roomId);
  }
  return { roomLibrary, activeRoomIds };
}

function validRoomIds(value: unknown, roomLibrary: RoomLibrary, limit = Number.POSITIVE_INFINITY): string[] {
  if (!Array.isArray(value)) return [];
  const roomIds: string[] = [];
  for (const item of value) {
    const roomId = readString(item);
    if (!roomId || !roomLibrary[roomId] || roomIds.includes(roomId)) continue;
    roomIds.push(roomId);
    if (roomIds.length >= limit) break;
  }
  return roomIds;
}

function parseHistory(value: unknown, roomLibrary: RoomLibrary): RoomHistoryEntry[] {
  if (!Array.isArray(value)) return [];
  const history: RoomHistoryEntry[] = [];
  for (const item of value) {
    if (!isRecord(item)) continue;
    const roomId = readString(item.roomId);
    const addedAt = readString(item.addedAt);
    if (!roomId || !addedAt || !roomLibrary[roomId] || history.some((entry) => entry.roomId === roomId)) {
      continue;
    }
    history.push({ roomId, addedAt });
    if (history.length >= MAX_HISTORY_ROOMS) break;
  }
  return history;
}

function parseGroups(value: unknown, roomLibrary: RoomLibrary): RoomGroup[] {
  if (!Array.isArray(value)) return [];
  const groups: RoomGroup[] = [];
  for (const item of value) {
    if (!isRecord(item)) continue;
    const id = readString(item.id);
    const name = readString(item.name);
    const createdAt = readString(item.createdAt);
    if (!id || !name || !createdAt || groups.some((group) => group.id === id)) continue;
    groups.push({
      id,
      name,
      roomIds: validRoomIds(item.roomIds, roomLibrary, MAX_GROUP_ROOMS),
      createdAt,
    });
  }
  return groups;
}

function parseLayoutId(value: unknown): LayoutId {
  return typeof value === 'string' && LAYOUT_VALUES.has(value) ? value as LayoutId : 'single';
}

function sameRoomIds(left: string[], right: string[]): boolean {
  return left.length === right.length && left.every((roomId, index) => roomId === right[index]);
}

interface ParsedWorkspaceData {
  roomLibrary: RoomLibrary;
  activeRoomIds: string[];
  history: RoomHistoryEntry[];
  favoriteRoomIds: string[];
  groups: RoomGroup[];
}

export function loadWorkspaceSnapshot(storage: WorkspaceStorage | undefined = getDefaultStorage()): WorkspaceSnapshot | undefined {
  if (!storage) return undefined;
  try {
    const raw = storage.getItem(WORKSPACE_STORAGE_KEY);
    if (!raw) return undefined;
    const parsed: unknown = JSON.parse(raw);
    if (!isRecord(parsed) || ![1, 2, WORKSPACE_SCHEMA_VERSION].includes(Number(parsed.schemaVersion))) {
      return undefined;
    }

    const migrated: ParsedWorkspaceData | undefined = parsed.schemaVersion === WORKSPACE_SCHEMA_VERSION
      ? (() => {
          const roomLibrary = toRoomLibrary(parsed.roomLibrary);
          if (!roomLibrary || !Array.isArray(parsed.activeRoomIds)) return undefined;
          return {
            roomLibrary,
            activeRoomIds: validRoomIds(parsed.activeRoomIds, roomLibrary, 9),
            history: parseHistory(parsed.history, roomLibrary),
            favoriteRoomIds: validRoomIds(parsed.favoriteRoomIds, roomLibrary),
            groups: parseGroups(parsed.groups, roomLibrary),
          };
        })()
      : (() => {
          const legacy = roomsToLibrary(parsed.rooms);
          return legacy ? { ...legacy, history: [], favoriteRoomIds: [], groups: [] } : undefined;
        })();
    if (!migrated) return undefined;

    const groups = migrated.groups;
    const requestedActiveGroupId = parsed.schemaVersion === WORKSPACE_SCHEMA_VERSION
      ? readString(parsed.activeGroupId)
      : undefined;
    const activeGroup = groups.find((group) => group.id === requestedActiveGroupId);
    const activeGroupId = activeGroup && sameRoomIds(activeGroup.roomIds, migrated.activeRoomIds)
      ? activeGroup.id
      : undefined;
    return {
      schemaVersion: WORKSPACE_SCHEMA_VERSION,
      roomLibrary: migrated.roomLibrary,
      activeRoomIds: migrated.activeRoomIds,
      history: migrated.history,
      favoriteRoomIds: migrated.favoriteRoomIds,
      groups,
      activeGroupId,
      layoutId: parseLayoutId(parsed.layoutId),
      primaryRoomId: readString(parsed.primaryRoomId),
      audioRoomId: readString(parsed.audioRoomId),
      globalDanmakuEnabled: parsed.globalDanmakuEnabled !== false,
      globalMuted: parsed.globalMuted === true,
      danmakuSettings: parseDanmakuSettings(parsed.danmakuSettings),
      sidebarOpen: typeof parsed.sidebarOpen === 'boolean' ? parsed.sidebarOpen : undefined,
    };
  } catch {
    return undefined;
  }
}

export function saveWorkspaceSnapshot(
  storage: WorkspaceStorage | undefined,
  snapshot: WorkspaceSnapshot,
): void {
  if (!storage) return;
  try {
    storage.setItem(WORKSPACE_STORAGE_KEY, JSON.stringify(snapshot));
  } catch {
    // Storage can be unavailable in locked-down or quota-exhausted environments.
  }
}

export function clearPersistedWorkspace(storage: WorkspaceStorage | undefined = getDefaultStorage()): void {
  if (!storage) return;
  try {
    storage.removeItem(WORKSPACE_STORAGE_KEY);
  } catch {
    // Ignore storage cleanup failures; the in-memory workspace remains usable.
  }
}
