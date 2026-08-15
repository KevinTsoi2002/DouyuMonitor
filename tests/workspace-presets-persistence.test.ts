import { describe, expect, it } from 'vitest';
import {
  WORKSPACE_SCHEMA_VERSION,
  WORKSPACE_STORAGE_KEY,
  loadWorkspaceSnapshot,
  saveWorkspaceSnapshot,
} from '../src/renderer/store/workspace-persistence';
import { DEFAULT_DANMAKU_SETTINGS } from '../src/renderer/danmaku/danmaku-settings';

function createMemoryStorage() {
  const values = new Map<string, string>();
  return {
    getItem(key: string) { return values.get(key) ?? null; },
    setItem(key: string, value: string) { values.set(key, value); },
    removeItem(key: string) { values.delete(key); },
  };
}

const room = {
  roomId: '63136',
  anchorName: '星河',
  title: '夜航电台',
  category: '聊天',
  online: true,
  status: 'playing',
  quality: 'high',
  volume: 0.5,
  danmakuEnabled: true,
};

const baseSnapshot = {
  schemaVersion: 5,
  roomLibrary: {
    '63136': {
      ...room,
      viewerLabel: '100',
    },
  },
  activeRoomIds: ['63136'],
  history: [],
  favoriteRoomIds: [],
  groups: [],
  layoutId: 'primary-two',
  primaryRoomId: '63136',
  roomPlacementOrder: ['63136'],
  primaryRoomRatio: 0.6,
  audioRoomId: '63136',
  globalDanmakuEnabled: true,
  globalMuted: false,
  danmakuSettings: DEFAULT_DANMAKU_SETTINGS,
  danmakuGovernanceOverrides: {},
  workspacePresets: [{
    id: 'preset-1',
    name: '赛事视角',
    createdAt: '2026-08-15T00:00:00.000Z',
    updatedAt: '2026-08-15T00:01:00.000Z',
    rooms: [room],
    roomOrder: ['63136'],
    layoutId: 'single',
    primaryRoomId: '63136',
    primaryRoomRatio: 0.6,
    audioRoomId: '63136',
    globalDanmakuEnabled: true,
    globalMuted: false,
    danmakuSettings: DEFAULT_DANMAKU_SETTINGS,
    danmakuGovernanceOverrides: {
      '63136': { duplicateWindowSeconds: 5 },
    },
  }],
  activeWorkspacePresetId: 'preset-1',
};

describe('workspace preset persistence', () => {
  it('loads and round-trips a valid v5 preset snapshot', () => {
    const storage = createMemoryStorage();
    storage.setItem(WORKSPACE_STORAGE_KEY, JSON.stringify(baseSnapshot));

    const loaded = loadWorkspaceSnapshot(storage);
    expect(loaded?.schemaVersion).toBe(WORKSPACE_SCHEMA_VERSION);
    expect(loaded?.workspacePresets).toEqual(baseSnapshot.workspacePresets);
    expect(loaded?.activeWorkspacePresetId).toBe('preset-1');

    saveWorkspaceSnapshot(storage, loaded!);
    expect(loadWorkspaceSnapshot(storage)?.workspacePresets).toEqual(baseSnapshot.workspacePresets);
  });

  it('migrates a v4 snapshot with no presets or active preset', () => {
    const storage = createMemoryStorage();
    const { workspacePresets: _presets, activeWorkspacePresetId: _active, ...v4 } = baseSnapshot;
    storage.setItem(WORKSPACE_STORAGE_KEY, JSON.stringify({ ...v4, schemaVersion: 4 }));

    expect(loadWorkspaceSnapshot(storage)).toEqual(expect.objectContaining({
      schemaVersion: WORKSPACE_SCHEMA_VERSION,
      workspacePresets: [],
      activeWorkspacePresetId: undefined,
    }));
  });

  it('filters invalid, duplicate, overlong, and over-limit presets without rejecting the workspace', () => {
    const storage = createMemoryStorage();
    const presets = [
      baseSnapshot.workspacePresets[0],
      { ...baseSnapshot.workspacePresets[0], id: 'preset-1', name: '重复 id' },
      { ...baseSnapshot.workspacePresets[0], id: 'preset-2', name: '   ' },
      { ...baseSnapshot.workspacePresets[0], id: 'preset-3', name: 'x'.repeat(41) },
      { ...baseSnapshot.workspacePresets[0], id: 'preset-4', name: '坏房间', rooms: [], roomOrder: ['63136'] },
      ...Array.from({ length: 20 }, (_, index) => ({
        ...baseSnapshot.workspacePresets[0],
        id: `preset-${index + 10}`,
        name: `预设 ${index + 10}`,
      })),
    ];
    storage.setItem(WORKSPACE_STORAGE_KEY, JSON.stringify({
      ...baseSnapshot,
      workspacePresets: presets,
      activeWorkspacePresetId: 'missing',
    }));

    const loaded = loadWorkspaceSnapshot(storage);
    expect(loaded?.workspacePresets).toHaveLength(20);
    expect(loaded?.workspacePresets.every((preset) => preset.name.length <= 40)).toBe(true);
    expect(new Set(loaded?.workspacePresets.map((preset) => preset.id)).size).toBe(20);
    expect(loaded?.activeWorkspacePresetId).toBeUndefined();
  });
});
