import {
  DEFAULT_PRIMARY_ROOM_RATIO,
  PRIMARY_ROOM_RATIOS,
  type LayoutId,
  type PrimaryRoomRatio,
} from '../../domain/layout-engine';
import type { RoomStatus, StreamQuality } from '../../domain/douyu-adapter';
import {
  parseDanmakuSettings,
  parseDanmakuGovernanceSettings,
  type DanmakuGovernanceOverride,
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
import { normalizeRoomPlacementOrder } from './room-placement';

export const WORKSPACE_STORAGE_KEY = 'douyu-monitor.workspace.v1';
export const WORKSPACE_SCHEMA_VERSION = 5;
export const MAX_WORKSPACE_PRESETS = 20;
export const MAX_PRESET_ROOMS = 9;
export const MAX_WORKSPACE_PRESET_NAME_LENGTH = 40;

export type AudioMode = 'single' | 'multi';

export interface WorkspaceStorage {
  getItem(key: string): string | null;
  setItem(key: string, value: string): void;
  removeItem(key: string): void;
}

export type PersistedRoom = LibraryRoom;

export interface WorkspacePresetRoom {
  roomId: string;
  anchorName: string;
  title: string;
  category: string;
  avatarUrl?: string;
  online: boolean;
  status: RoomStatus;
  quality: StreamQuality;
  volume: number;
  danmakuEnabled: boolean;
}

export interface WorkspacePreset {
  id: string;
  name: string;
  createdAt: string;
  updatedAt: string;
  rooms: WorkspacePresetRoom[];
  roomOrder: string[];
  layoutId: LayoutId;
  primaryRoomId?: string;
  primaryRoomRatio: PrimaryRoomRatio;
  audioRoomId?: string;
  audioMode: AudioMode;
  mutedRoomIds: string[];
  globalDanmakuEnabled: boolean;
  globalMuted: boolean;
  danmakuSettings: DanmakuSettings;
  danmakuGovernanceOverrides: Record<string, DanmakuGovernanceOverride>;
}

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
  roomPlacementOrder: string[];
  primaryRoomRatio: PrimaryRoomRatio;
  audioRoomId?: string;
  audioMode: AudioMode;
  mutedRoomIds: string[];
  globalDanmakuEnabled: boolean;
  globalMuted: boolean;
  danmakuSettings: DanmakuSettings;
  danmakuGovernanceOverrides: Record<string, DanmakuGovernanceOverride>;
  workspacePresets: WorkspacePreset[];
  activeWorkspacePresetId?: string;
  sidebarOpen?: boolean;
}

const QUALITY_VALUES = new Set<StreamQuality>(['auto', 'original', 'super', 'high', 'standard']);
const STATUS_VALUES = new Set<RoomStatus>(['playing', 'offline', 'reconnecting', 'error']);
const AUDIO_MODE_VALUES = new Set<AudioMode>(['single', 'multi']);
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

function readTrimmedString(value: unknown): string | undefined {
  if (typeof value !== 'string') return undefined;
  const trimmed = value.trim();
  return trimmed.length > 0 ? trimmed : undefined;
}

function isTimestamp(value: unknown): value is string {
  return typeof value === 'string' && value.trim().length > 0 && Number.isFinite(Date.parse(value));
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

function validRoomIdSubset(value: unknown, allowedRoomIds: readonly string[]): string[] {
  if (!Array.isArray(value)) return [];
  const allowed = new Set(allowedRoomIds);
  const roomIds: string[] = [];
  for (const item of value) {
    const roomId = readString(item);
    if (!roomId || !allowed.has(roomId) || roomIds.includes(roomId)) continue;
    roomIds.push(roomId);
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

function parseGovernanceOverrides(
  value: unknown,
  roomLibrary: RoomLibrary,
): Record<string, DanmakuGovernanceOverride> {
  return parseGovernanceOverridesForRooms(value, new Set(Object.keys(roomLibrary)));
}

function parseGovernanceOverridesForRooms(
  value: unknown,
  roomIds: ReadonlySet<string>,
): Record<string, DanmakuGovernanceOverride> {
  if (!isRecord(value)) return {};
  const overrides: Record<string, DanmakuGovernanceOverride> = {};
  for (const [roomId, rawOverride] of Object.entries(value)) {
    if (!roomIds.has(roomId) || !isRecord(rawOverride)) continue;
    const parsed = parseDanmakuGovernanceSettings(rawOverride);
    const override: DanmakuGovernanceOverride = {};
    if ('enabled' in rawOverride) override.enabled = parsed.enabled;
    if ('keywordBlacklist' in rawOverride) override.keywordBlacklist = parsed.keywordBlacklist;
    if ('duplicateWindowSeconds' in rawOverride) {
      override.duplicateWindowSeconds = parsed.duplicateWindowSeconds;
    }
    if ('peakProtectionEnabled' in rawOverride) {
      override.peakProtectionEnabled = parsed.peakProtectionEnabled;
    }
    if (Object.keys(override).length > 0) overrides[roomId] = override;
  }
  return overrides;
}

function parsePresetRoom(value: unknown): WorkspacePresetRoom | undefined {
  if (!isRecord(value)) return undefined;
  const roomId = readString(value.roomId);
  const anchorName = readString(value.anchorName);
  const title = readString(value.title);
  const category = readString(value.category);
  if (
    !roomId ||
    !anchorName ||
    !title ||
    !category ||
    typeof value.online !== 'boolean' ||
    !STATUS_VALUES.has(value.status as RoomStatus) ||
    !QUALITY_VALUES.has(value.quality as StreamQuality) ||
    typeof value.volume !== 'number' ||
    !Number.isFinite(value.volume) ||
    value.volume < 0 ||
    value.volume > 1 ||
    typeof value.danmakuEnabled !== 'boolean'
  ) {
    return undefined;
  }
  const avatarUrl = readHttpUrl(value.avatarUrl);
  return {
    roomId,
    anchorName,
    title,
    category,
    ...(avatarUrl ? { avatarUrl } : {}),
    online: value.online,
    status: value.status as RoomStatus,
    quality: value.quality as StreamQuality,
    volume: value.volume,
    danmakuEnabled: value.danmakuEnabled,
  };
}

function parsePreset(value: unknown): WorkspacePreset | undefined {
  if (!isRecord(value)) return undefined;
  const id = readString(value.id);
  const name = readTrimmedString(value.name);
  const createdAt = value.createdAt;
  const updatedAt = value.updatedAt;
  if (
    !id ||
    !name ||
    Array.from(name).length > MAX_WORKSPACE_PRESET_NAME_LENGTH ||
    !isTimestamp(createdAt) ||
    !isTimestamp(updatedAt) ||
    !Array.isArray(value.rooms) ||
    value.rooms.length > MAX_PRESET_ROOMS ||
    !Array.isArray(value.roomOrder) ||
    !value.roomOrder.every((roomId) => typeof roomId === 'string')
  ) return undefined;

  const rooms = value.rooms.map(parsePresetRoom);
  if (rooms.some((room): room is undefined => room === undefined)) return undefined;
  const parsedRooms = rooms as WorkspacePresetRoom[];
  const roomIds = parsedRooms.map((room) => room.roomId);
  if (new Set(roomIds).size !== roomIds.length) return undefined;

  const roomOrder = value.roomOrder as string[];
  if (
    roomOrder.length > roomIds.length ||
    new Set(roomOrder).size !== roomOrder.length ||
    roomOrder.some((roomId) => !roomIds.includes(roomId))
  ) return undefined;

  if (typeof value.layoutId !== 'string' || !LAYOUT_VALUES.has(value.layoutId)) return undefined;
  if (
    value.primaryRoomId !== undefined &&
    (typeof value.primaryRoomId !== 'string' || !roomIds.includes(value.primaryRoomId))
  ) return undefined;
  if (
    value.audioRoomId !== undefined &&
    (typeof value.audioRoomId !== 'string' || !roomIds.includes(value.audioRoomId))
  ) return undefined;
  if (
    typeof value.primaryRoomRatio !== 'number' ||
    !PRIMARY_ROOM_RATIOS.includes(value.primaryRoomRatio as PrimaryRoomRatio)
  ) return undefined;
  if (typeof value.globalDanmakuEnabled !== 'boolean' || typeof value.globalMuted !== 'boolean') return undefined;

  const roomIdSet = new Set(roomIds);
  return {
    id,
    name,
    createdAt,
    updatedAt,
    rooms: parsedRooms,
    roomOrder: [...roomOrder],
    layoutId: value.layoutId as LayoutId,
    ...(value.primaryRoomId !== undefined ? { primaryRoomId: value.primaryRoomId } : {}),
    primaryRoomRatio: value.primaryRoomRatio as PrimaryRoomRatio,
    ...(value.audioRoomId !== undefined ? { audioRoomId: value.audioRoomId } : {}),
    audioMode: parseAudioMode(value.audioMode),
    mutedRoomIds: validRoomIdSubset(value.mutedRoomIds, roomIds),
    globalDanmakuEnabled: value.globalDanmakuEnabled,
    globalMuted: value.globalMuted,
    danmakuSettings: parseDanmakuSettings(value.danmakuSettings),
    danmakuGovernanceOverrides: parseGovernanceOverridesForRooms(value.danmakuGovernanceOverrides, roomIdSet),
  };
}

function parseWorkspacePresets(value: unknown): WorkspacePreset[] {
  if (!Array.isArray(value)) return [];
  const presets: WorkspacePreset[] = [];
  const ids = new Set<string>();
  const names = new Set<string>();
  for (const item of value) {
    const preset = parsePreset(item);
    if (!preset || ids.has(preset.id) || names.has(preset.name)) continue;
    ids.add(preset.id);
    names.add(preset.name);
    presets.push(preset);
    if (presets.length >= MAX_WORKSPACE_PRESETS) break;
  }
  return presets;
}

function parseLayoutId(value: unknown): LayoutId {
  return typeof value === 'string' && LAYOUT_VALUES.has(value) ? value as LayoutId : 'single';
}

function parsePrimaryRoomRatio(value: unknown): PrimaryRoomRatio {
  return typeof value === 'number' && PRIMARY_ROOM_RATIOS.includes(value as PrimaryRoomRatio)
    ? value as PrimaryRoomRatio
    : DEFAULT_PRIMARY_ROOM_RATIO;
}

function parseAudioMode(value: unknown): AudioMode {
  return AUDIO_MODE_VALUES.has(value as AudioMode) ? value as AudioMode : 'single';
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
    if (!isRecord(parsed)) return undefined;
    const schemaVersion = Number(parsed.schemaVersion);
    if (![1, 2, 3, 4, WORKSPACE_SCHEMA_VERSION].includes(schemaVersion)) {
      return undefined;
    }

    const structuredSnapshot = schemaVersion >= 3;
    const migrated: ParsedWorkspaceData | undefined = structuredSnapshot
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
    const workspacePresets = schemaVersion >= 5 ? parseWorkspacePresets(parsed.workspacePresets) : [];
    const requestedActiveGroupId = structuredSnapshot
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
      roomPlacementOrder: structuredSnapshot
        ? normalizeRoomPlacementOrder(migrated.activeRoomIds, parsed.roomPlacementOrder)
        : [...migrated.activeRoomIds],
      primaryRoomRatio: structuredSnapshot
        ? parsePrimaryRoomRatio(parsed.primaryRoomRatio)
        : DEFAULT_PRIMARY_ROOM_RATIO,
      audioRoomId: readString(parsed.audioRoomId),
      audioMode: parseAudioMode(parsed.audioMode),
      mutedRoomIds: validRoomIdSubset(parsed.mutedRoomIds, migrated.activeRoomIds),
      globalDanmakuEnabled: parsed.globalDanmakuEnabled !== false,
      globalMuted: parsed.globalMuted === true,
      danmakuSettings: parseDanmakuSettings(parsed.danmakuSettings),
      danmakuGovernanceOverrides: schemaVersion >= 4
        ? parseGovernanceOverrides(parsed.danmakuGovernanceOverrides, migrated.roomLibrary)
        : {},
      workspacePresets,
      activeWorkspacePresetId: schemaVersion >= 5
        ? (() => {
            const activeId = readString(parsed.activeWorkspacePresetId);
            return activeId && workspacePresets.some((preset) => preset.id === activeId)
              ? activeId
              : undefined;
          })()
        : undefined,
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
