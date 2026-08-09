import { describe, expect, it } from 'vitest';
import {
  WORKSPACE_STORAGE_KEY,
  loadWorkspaceSnapshot,
  saveWorkspaceSnapshot,
  type WorkspaceSnapshot,
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

const legacySnapshot = {
  schemaVersion: 1,
  rooms: [{
    roomId: '63136',
    anchorName: '星河',
    title: '夜航电台',
    category: '聊天',
    online: true,
    viewerLabel: '18.6 万',
    quality: 'high',
    danmakuEnabled: false,
    volume: 0.65,
  }],
  layoutId: 'primary-two',
  primaryRoomId: '63136',
  audioRoomId: '63136',
  globalDanmakuEnabled: true,
  globalMuted: false,
};

const legacyV2Snapshot = {
  ...legacySnapshot,
  schemaVersion: 2,
  danmakuSettings: DEFAULT_DANMAKU_SETTINGS,
  sidebarOpen: true,
};

const persistedRoom: WorkspaceSnapshot['roomLibrary'][string] = {
  ...legacySnapshot.rooms[0],
  quality: 'high',
  avatarUrl: 'https://example.com/avatar.jpg',
};

const snapshot: WorkspaceSnapshot = {
  schemaVersion: 3,
  roomLibrary: { '63136': persistedRoom },
  activeRoomIds: ['63136'],
  history: [{ roomId: '63136', addedAt: '2026-08-10T00:00:00.000Z' }],
  favoriteRoomIds: ['63136'],
  groups: [{
    id: 'group-1',
    name: '赛事',
    roomIds: ['63136'],
    createdAt: '2026-08-10T00:00:00.000Z',
  }],
  activeGroupId: 'group-1',
  layoutId: legacySnapshot.layoutId as WorkspaceSnapshot['layoutId'],
  primaryRoomId: '63136',
  audioRoomId: '63136',
  globalDanmakuEnabled: true,
  globalMuted: false,
  danmakuSettings: DEFAULT_DANMAKU_SETTINGS,
  sidebarOpen: true,
};

describe('workspace persistence', () => {
  it('round-trips the automatic layout mode', () => {
    const storage = createMemoryStorage();
    saveWorkspaceSnapshot(storage, { ...snapshot, layoutId: 'auto' });

    expect(loadWorkspaceSnapshot(storage)?.layoutId).toBe('auto');
  });

  it('round-trips a non-sensitive workspace snapshot', () => {
    const storage = createMemoryStorage();
    saveWorkspaceSnapshot(storage, snapshot);

    expect(loadWorkspaceSnapshot(storage)).toEqual(snapshot);
    expect(storage.getItem(WORKSPACE_STORAGE_KEY)).not.toContain('playbackUrl');
    expect(storage.getItem(WORKSPACE_STORAGE_KEY)).not.toContain('token');
  });

  it('migrates a v1 snapshot to the room library schema', () => {
    const storage = createMemoryStorage();
    storage.setItem(WORKSPACE_STORAGE_KEY, JSON.stringify(legacySnapshot));

    expect(loadWorkspaceSnapshot(storage)).toEqual({
      schemaVersion: 3,
      roomLibrary: { '63136': legacySnapshot.rooms[0] },
      activeRoomIds: ['63136'],
      history: [],
      favoriteRoomIds: [],
      groups: [],
      activeGroupId: undefined,
      layoutId: 'primary-two',
      primaryRoomId: '63136',
      audioRoomId: '63136',
      globalDanmakuEnabled: true,
      globalMuted: false,
      danmakuSettings: DEFAULT_DANMAKU_SETTINGS,
      sidebarOpen: undefined,
    });
  });

  it('migrates a v2 snapshot while preserving settings and sidebar state', () => {
    const storage = createMemoryStorage();
    storage.setItem(WORKSPACE_STORAGE_KEY, JSON.stringify(legacyV2Snapshot));

    expect(loadWorkspaceSnapshot(storage)).toEqual(expect.objectContaining({
      schemaVersion: 3,
      roomLibrary: { '63136': legacySnapshot.rooms[0] },
      activeRoomIds: ['63136'],
      danmakuSettings: DEFAULT_DANMAKU_SETTINGS,
      sidebarOpen: true,
    }));
  });

  it('falls back invalid v3 settings field by field', () => {
    const storage = createMemoryStorage();
    storage.setItem(WORKSPACE_STORAGE_KEY, JSON.stringify({
      ...snapshot,
      danmakuSettings: {
        durationSeconds: 99,
        fontSize: 30,
        opacity: 'opaque',
        region: 'top',
        density: 'unknown',
        fontFamily: 'simhei',
        rendering: 'advanced',
      },
      sidebarOpen: 'yes',
    }));

    expect(loadWorkspaceSnapshot(storage)).toEqual(expect.objectContaining({
      schemaVersion: 3,
      danmakuSettings: {
        durationSeconds: 15,
        fontSize: 30,
        opacity: 0.9,
        region: 'top',
        density: 'normal',
        fontFamily: 'simhei',
        rendering: 'advanced',
      },
      sidebarOpen: undefined,
    }));
  });

  it('filters unsafe avatars and dangling or duplicate room references', () => {
    const storage = createMemoryStorage();
    storage.setItem(WORKSPACE_STORAGE_KEY, JSON.stringify({
      ...snapshot,
      roomLibrary: {
        '63136': { ...persistedRoom, avatarUrl: 'javascript:alert(1)' },
      },
      activeRoomIds: ['63136', 'missing', '63136'],
      history: [
        { roomId: '63136', addedAt: '2026-08-10T00:00:00.000Z' },
        { roomId: 'missing', addedAt: '2026-08-10T00:01:00.000Z' },
        { roomId: '63136', addedAt: '2026-08-10T00:02:00.000Z' },
      ],
      favoriteRoomIds: ['63136', 'missing', '63136'],
      groups: [{
        id: 'group-1',
        name: '赛事',
        roomIds: ['63136', 'missing', '63136'],
        createdAt: '2026-08-10T00:00:00.000Z',
      }],
    }));

    expect(loadWorkspaceSnapshot(storage)).toEqual(expect.objectContaining({
      roomLibrary: {
        '63136': expect.not.objectContaining({ avatarUrl: expect.anything() }),
      },
      activeRoomIds: ['63136'],
      history: [{ roomId: '63136', addedAt: '2026-08-10T00:00:00.000Z' }],
      favoriteRoomIds: ['63136'],
      groups: [expect.objectContaining({ roomIds: ['63136'] })],
    }));
  });

  it('restores legacy concrete layouts and falls back from unknown layout ids', () => {
    const storage = createMemoryStorage();
    saveWorkspaceSnapshot(storage, { ...snapshot, layoutId: 'grid-3x2' });

    expect(loadWorkspaceSnapshot(storage)?.layoutId).toBe('grid-3x2');

    storage.setItem(WORKSPACE_STORAGE_KEY, JSON.stringify({
      ...snapshot,
      layoutId: 'unknown-layout',
    }));
    expect(loadWorkspaceSnapshot(storage)?.layoutId).toBe('single');
  });

  it('returns undefined for malformed or unsupported snapshots', () => {
    const storage = createMemoryStorage();
    storage.setItem(WORKSPACE_STORAGE_KEY, '{"schemaVersion":999}');
    expect(loadWorkspaceSnapshot(storage)).toBeUndefined();
    storage.setItem(WORKSPACE_STORAGE_KEY, 'not-json');
    expect(loadWorkspaceSnapshot(storage)).toBeUndefined();
  });
});
