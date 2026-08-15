import { describe, expect, it, vi } from 'vitest';
import type {
  DouyuAdapter,
  RoomCandidate,
  StreamAvailability,
} from '../src/domain/douyu-adapter';
import { createMockDouyuAdapter } from '../src/infrastructure/mock-douyu-adapter';
import { createWorkspaceStore } from '../src/renderer/store/workspace-store';
import { createWorkspacePresetDraft } from '../src/renderer/store/workspace-store';
import { loadWorkspaceSnapshot } from '../src/renderer/store/workspace-persistence';

function createMemoryStorage() {
  const values = new Map<string, string>();
  return {
    getItem(key: string) { return values.get(key) ?? null; },
    setItem(key: string, value: string) { values.set(key, value); },
    removeItem(key: string) { values.delete(key); },
  };
}

function candidate(roomId: string): RoomCandidate {
  return {
    roomId,
    anchorName: `主播 ${roomId}`,
    title: `直播间 ${roomId}`,
    category: '综合直播',
    online: true,
    viewerLabel: '12.8 万',
  };
}

function blockedAvailability(roomId: string): StreamAvailability {
  return {
    kind: 'blocked',
    roomId,
    reason: 'SIGNATURE_REQUIRED',
    observedQualities: [
      { id: 'douyu-0', label: '蓝光10M', providerType: 0 },
    ],
    checkedAt: '2026-08-07T00:00:00.000Z',
  };
}

function deferredAvailability() {
  let resolve!: (value: StreamAvailability) => void;
  let reject!: (reason: unknown) => void;
  const promise = new Promise<StreamAvailability>((resolvePromise, rejectPromise) => {
    resolve = resolvePromise;
    reject = rejectPromise;
  });
  return { promise, resolve, reject };
}

const deterministicOptions = {
  now: () => new Date('2026-08-10T00:00:00.000Z'),
  createGroupId: () => 'group-1',
};

describe('createWorkspaceStore', () => {
  it('saves and updates a named workspace preset without runtime diagnostics', () => {
    const storage = createMemoryStorage();
    const store = createWorkspaceStore(createMockDouyuAdapter(), {
      storage,
      ...deterministicOptions,
      initialRooms: [candidate('101'), candidate('202')],
    });

    store.getState().setLayout('split-vertical');
    store.getState().setQuality('202', 'high');
    const presetId = store.getState().saveWorkspacePreset('比赛视角');

    expect(presetId).toBeTruthy();
    expect(store.getState().activeWorkspacePresetId).toBe(presetId);
    expect(store.getState().workspacePresets[0]).toEqual(expect.objectContaining({
      name: '比赛视角',
      layoutId: 'split-vertical',
      roomOrder: ['101', '202'],
    }));
    const { id: _id, name: _name, createdAt: _createdAt, updatedAt: _updatedAt, ...savedDraft } = store.getState().workspacePresets[0];
    expect(createWorkspacePresetDraft(store.getState())).toEqual(savedDraft);
    expect(store.getState().hasUnsavedWorkspaceChanges).toBe(false);

    store.getState().setVolume('101', 0.2);
    expect(store.getState().hasUnsavedWorkspaceChanges).toBe(true);
    expect(store.getState().updateWorkspacePreset(presetId!)).toBe(true);
    expect(store.getState().hasUnsavedWorkspaceChanges).toBe(false);
    expect(loadWorkspaceSnapshot(storage)?.workspacePresets[0]?.rooms[0]?.volume).toBe(0.2);
  });

  it('loads a preset as the active room set without changing history, favorites, or groups', async () => {
    const storage = createMemoryStorage();
    const adapter = createMockDouyuAdapter();
    const store = createWorkspaceStore(adapter, {
      storage,
      ...deterministicOptions,
      initialRooms: [candidate('101'), candidate('202')],
    });
    const groupId = store.getState().createGroup('活动')!;
    store.getState().addRoomToGroup(groupId, '101');
    store.getState().toggleFavorite('101');
    const presetId = store.getState().saveWorkspacePreset('单房间')!;
    store.getState().removeRoom('101');
    store.getState().removeRoom('202');
    store.getState().addRoom(candidate('303'));
    const historyBeforeLoad = store.getState().history;

    await expect(store.getState().loadWorkspacePreset(presetId)).resolves.toBe(true);

    expect(store.getState().rooms.map((room) => room.roomId)).toEqual(['101', '202']);
    expect(store.getState().history).toEqual(historyBeforeLoad);
    expect(store.getState().favoriteRoomIds).toEqual(['101']);
    expect(store.getState().groups).toEqual(expect.arrayContaining([
      expect.objectContaining({ id: groupId, roomIds: ['101'] }),
    ]));
    expect(store.getState().roomLibrary['303']).toBeDefined();
  });

  it('rejects duplicate or invalid preset names and rolls back failed loads', async () => {
    const store = createWorkspaceStore(createMockDouyuAdapter(), {
      ...deterministicOptions,
      initialRooms: [candidate('101')],
    });
    const firstId = store.getState().saveWorkspacePreset('默认')!;

    expect(store.getState().saveWorkspacePreset(' 默认 ')).toBeUndefined();
    expect(store.getState().saveWorkspacePreset('')).toBeUndefined();
    expect(store.getState().renameWorkspacePreset(firstId, 'x'.repeat(41))).toBe(false);

    store.getState().removeRoom('101');
    expect(await store.getState().loadWorkspacePreset('missing')).toBe(false);
    expect(store.getState().rooms).toEqual([]);
  });

  it('does not mark a preset dirty when runtime online status changes', async () => {
    let live = true;
    const adapter: DouyuAdapter = {
      ...createMockDouyuAdapter(),
      search: vi.fn(async () => [{ ...candidate('101'), online: live }]),
    };
    const store = createWorkspaceStore(adapter, {
      ...deterministicOptions,
      initialRooms: [candidate('101')],
    });
    store.getState().saveWorkspacePreset('稳定视角');
    live = false;

    await store.getState().refreshRoomMetadata('101');

    expect(store.getState().rooms[0]?.online).toBe(false);
    expect(store.getState().hasUnsavedWorkspaceChanges).toBe(false);
  });
  it('starts new workspaces in automatic layout mode and keeps it after adding rooms', () => {
    const store = createWorkspaceStore(createMockDouyuAdapter());

    expect(store.getState().layoutId).toBe('auto');
    store.getState().addRoom(candidate('101'));
    store.getState().addRoom(candidate('202'));

    expect(store.getState().layoutId).toBe('auto');
  });

  it('preserves a manually locked layout when a room is added', () => {
    const store = createWorkspaceStore(createMockDouyuAdapter(), {
      initialRooms: [candidate('101')],
    });

    store.getState().setLayout('split-vertical');
    store.getState().addRoom(candidate('202'));

    expect(store.getState().layoutId).toBe('split-vertical');
  });

  it('returns to automatic recommendations when the user selects auto', () => {
    const store = createWorkspaceStore(createMockDouyuAdapter(), {
      initialRooms: [candidate('101')],
    });

    store.getState().setLayout('primary-two');
    store.getState().setLayout('auto');

    expect(store.getState().layoutId).toBe('auto');
  });

  it('exposes the explicit runtime demo mode', () => {
    expect(createWorkspaceStore(createMockDouyuAdapter()).getState().demoMode).toBe(false);
    expect(createWorkspaceStore(createMockDouyuAdapter(), { demoMode: true }).getState().demoMode)
      .toBe(true);
  });

  it('assigns primary video and audio focus to the first room', () => {
    const store = createWorkspaceStore(createMockDouyuAdapter());

    expect(store.getState().addRoom(candidate('101'))).toBe('added');
    expect(store.getState().rooms.map((room) => room.roomId)).toEqual(['101']);
    expect(store.getState().primaryRoomId).toBe('101');
    expect(store.getState().audioRoomId).toBe('101');
  });

  it('rejects duplicates and rooms beyond the nine-room limit', () => {
    const store = createWorkspaceStore(createMockDouyuAdapter());

    expect(store.getState().addRoom(candidate('1'))).toBe('added');
    expect(store.getState().addRoom(candidate('1'))).toBe('duplicate');
    for (let index = 2; index <= 9; index += 1) {
      expect(store.getState().addRoom(candidate(String(index)))).toBe('added');
    }

    expect(store.getState().addRoom(candidate('10'))).toBe('limit');
    expect(store.getState().rooms).toHaveLength(9);
  });

  it('records successful additions and toggles favorites', () => {
    const store = createWorkspaceStore(createMockDouyuAdapter(), deterministicOptions);

    store.getState().addRoom(candidate('1'));
    store.getState().toggleFavorite('1');

    expect(store.getState().history).toEqual([
      { roomId: '1', addedAt: '2026-08-10T00:00:00.000Z' },
    ]);
    expect(store.getState().favoriteRoomIds).toEqual(['1']);
    expect(store.getState().roomLibrary['1']).toEqual(expect.objectContaining(candidate('1')));
  });

  it('updates the history timestamp when adding an existing room again', () => {
    let currentTime = '2026-08-10T00:00:00.000Z';
    const store = createWorkspaceStore(createMockDouyuAdapter(), {
      now: () => new Date(currentTime),
    });

    expect(store.getState().addRoom(candidate('1'))).toBe('added');
    currentTime = '2026-08-10T01:00:00.000Z';

    expect(store.getState().addRoom(candidate('1'))).toBe('duplicate');
    expect(store.getState().history).toEqual([
      { roomId: '1', addedAt: '2026-08-10T01:00:00.000Z' },
    ]);
  });

  it('refreshes playback state when metadata changes an offline room to live', async () => {
    const offlineRoom = { ...candidate('1'), online: false };
    const liveRoom = candidate('1');
    const pending = deferredAvailability();
    const getStreamAvailability = vi.fn(() => pending.promise);
    const adapter: DouyuAdapter = {
      search: vi.fn(async () => [liveRoom]),
      getStreamAvailability,
    };
    const store = createWorkspaceStore(adapter, { initialRooms: [offlineRoom] });

    await expect(store.getState().refreshRoomMetadata('1')).resolves.toBe(true);
    expect(getStreamAvailability).toHaveBeenCalledWith('1');

    pending.resolve(blockedAvailability('1'));
    await pending.promise;
    await Promise.resolve();

    expect(store.getState().rooms[0]).toEqual(expect.objectContaining({
      online: true,
      status: 'playing',
      playbackAvailabilityStatus: 'blocked',
    }));
  });

  it('surfaces metadata refresh failures as a current playback diagnostic', async () => {
    const pending = deferredAvailability();
    const adapter: DouyuAdapter = {
      search: vi.fn(async () => { throw new Error('network unavailable'); }),
      getStreamAvailability: vi.fn(() => pending.promise),
    };
    const store = createWorkspaceStore(adapter, {
      ...deterministicOptions,
      initialRooms: [candidate('1')],
    });

    await expect(store.getState().refreshRoomMetadata('1')).resolves.toBe(false);

    expect(store.getState().rooms[0]).toEqual(expect.objectContaining({
      playbackAvailabilityStatus: 'error',
      playbackCheckedAt: '2026-08-10T00:00:00.000Z',
      playbackErrorCode: 'ROOM_METADATA_CHECK_FAILED',
    }));
  });

  it('allows a room to belong to multiple groups', () => {
    let nextGroup = 0;
    const store = createWorkspaceStore(createMockDouyuAdapter(), {
      ...deterministicOptions,
      initialRooms: [candidate('1')],
      createGroupId: () => `group-${++nextGroup}`,
    });
    const first = store.getState().createGroup('赛事')!;
    const second = store.getState().createGroup('常用')!;

    expect(store.getState().addRoomToGroup(first, '1')).toBe('added');
    expect(store.getState().addRoomToGroup(second, '1')).toBe('added');
    expect(store.getState().groups.map((group) => group.roomIds)).toEqual([['1'], ['1']]);
  });

  it('replaces active sessions when switching groups', () => {
    const store = createWorkspaceStore(createMockDouyuAdapter(), {
      ...deterministicOptions,
      initialRooms: [candidate('1'), candidate('2')],
    });
    const groupId = store.getState().createGroup('赛事')!;
    store.getState().addRoomToGroup(groupId, '2');

    store.getState().switchGroup(groupId);

    expect(store.getState().rooms.map((room) => room.roomId)).toEqual(['2']);
    expect(store.getState()).toEqual(expect.objectContaining({
      activeGroupId: groupId,
      primaryRoomId: '2',
      audioRoomId: '2',
    }));
  });

  it('clears the active group after a temporary room change', () => {
    const store = createWorkspaceStore(createMockDouyuAdapter(), {
      ...deterministicOptions,
      initialRooms: [candidate('1'), candidate('2')],
    });
    const groupId = store.getState().createGroup('赛事')!;
    store.getState().addRoomToGroup(groupId, '1');
    store.getState().switchGroup(groupId);

    store.getState().addRoom(candidate('2'));

    expect(store.getState().activeGroupId).toBeUndefined();
  });

  it('removes a room, repairs focus, and allows the room to be added again', () => {
    const store = createWorkspaceStore(createMockDouyuAdapter(), {
      initialRooms: [candidate('a'), candidate('b')],
    });

    store.getState().removeRoom('a');

    expect(store.getState().primaryRoomId).toBe('b');
    expect(store.getState().audioRoomId).toBe('b');
    expect(store.getState().addRoom(candidate('a'))).toBe('added');
    expect(store.getState().rooms.map((room) => room.roomId)).toEqual(['b', 'a']);
  });

  it('reorders rooms without replacing their session state', () => {
    const store = createWorkspaceStore(createMockDouyuAdapter(), {
      initialRooms: [candidate('a'), candidate('b'), candidate('c')],
    });
    const originalB = store.getState().rooms[1];

    store.getState().moveRoom('b', -1);

    expect(store.getState().rooms.map((room) => room.roomId)).toEqual(['b', 'a', 'c']);
    expect(store.getState().roomPlacementOrder).toEqual(['b', 'a', 'c']);
    expect(store.getState().rooms[0]).toBe(originalB);
  });

  it('reorders rooms by drag target', () => {
    const store = createWorkspaceStore(createMockDouyuAdapter(), {
      initialRooms: [candidate('a'), candidate('b'), candidate('c')],
    });

    store.getState().reorderRooms('c', 'a');

    expect(store.getState().rooms.map((room) => room.roomId)).toEqual(['c', 'a', 'b']);
  });

  it('swaps only primary and target visual slots across consecutive changes', () => {
    const store = createWorkspaceStore(createMockDouyuAdapter(), {
      initialRooms: [candidate('a'), candidate('b'), candidate('c'), candidate('d')],
    });
    store.getState().setAudioRoom('b');
    store.getState().setQuality('c', 'high');
    store.getState().toggleDanmaku('c');
    const primarySession = store.getState().rooms.find((room) => room.roomId === 'c');

    store.getState().setPrimaryRoom('c');
    expect(store.getState().roomPlacementOrder).toEqual(['c', 'b', 'a', 'd']);
    expect(store.getState().primaryRoomId).toBe('c');
    expect(store.getState().audioRoomId).toBe('b');
    expect(store.getState().rooms.find((room) => room.roomId === 'c')).toBe(primarySession);
    expect(primarySession).toEqual(expect.objectContaining({ quality: 'high', danmakuEnabled: false }));

    store.getState().setPrimaryRoom('b');
    expect(store.getState().roomPlacementOrder).toEqual(['b', 'c', 'a', 'd']);
    expect(store.getState().primaryRoomId).toBe('b');
  });

  it('synchronizes placement order during add, sidebar reorder, and removal', () => {
    const store = createWorkspaceStore(createMockDouyuAdapter(), {
      initialRooms: [candidate('a'), candidate('b'), candidate('c')],
    });
    store.getState().setPrimaryRoom('c');
    store.getState().addRoom(candidate('d'));
    expect(store.getState().roomPlacementOrder).toEqual(['c', 'b', 'a', 'd']);

    store.getState().reorderRooms('d', 'b');
    expect(store.getState().roomPlacementOrder).toEqual(['c', 'd', 'b', 'a']);

    store.getState().removeRoom('c');
    expect(store.getState().roomPlacementOrder).toEqual(['d', 'b', 'a']);
    expect(store.getState().primaryRoomId).toBe('d');
  });

  it('keeps surviving visual slots while switching groups and appends new rooms', () => {
    const store = createWorkspaceStore(createMockDouyuAdapter(), {
      ...deterministicOptions,
      initialRooms: [candidate('a'), candidate('b'), candidate('c')],
    });
    store.getState().setPrimaryRoom('c');
    const groupId = store.getState().createGroup('event')!;
    store.getState().addRoomToGroup(groupId, 'b');
    store.getState().addRoomToGroup(groupId, 'c');
    store.getState().switchGroup(groupId);

    expect(store.getState().roomPlacementOrder).toEqual(['c', 'b']);
    expect(store.getState().primaryRoomId).toBe('c');
  });

  it('persists the selected primary ratio and restores it', () => {
    const storage = createMemoryStorage();
    const first = createWorkspaceStore(createMockDouyuAdapter(), {
      storage,
      initialRooms: [candidate('a'), candidate('b')],
    });
    first.getState().setPrimaryRoom('b');
    first.getState().setPrimaryRoomRatio(0.67);

    const restored = createWorkspaceStore(createMockDouyuAdapter(), { storage });
    expect(restored.getState().roomPlacementOrder).toEqual(['b', 'a']);
    expect(restored.getState().primaryRoomId).toBe('b');
    expect(restored.getState().primaryRoomRatio).toBe(0.67);
  });

  it('updates global controls and clamps room volume', () => {
    const store = createWorkspaceStore(createMockDouyuAdapter(), {
      initialRooms: [candidate('a')],
    });

    store.getState().setGlobalDanmakuEnabled(false);
    store.getState().setGlobalMuted(true);
    store.getState().setVolume('a', 1.4);

    expect(store.getState()).toEqual(expect.objectContaining({
      globalDanmakuEnabled: false,
      globalMuted: true,
    }));
    expect(store.getState().rooms[0].volume).toBe(1);
  });

  it('starts new rooms at fifty percent volume', () => {
    const store = createWorkspaceStore(createMockDouyuAdapter(), {
      initialRooms: [candidate('default-volume')],
    });

    expect(store.getState().rooms[0].volume).toBe(0.5);
  });

  it('persists global danmaku settings and sidebar state', () => {
    const storage = createMemoryStorage();
    const first = createWorkspaceStore(createMockDouyuAdapter(), {
      storage,
      initialSidebarOpen: true,
    });

    first.getState().setDanmakuSettings({ fontSize: 30, density: 'massive' });
    first.getState().setSidebarOpen(false);

    const restored = createWorkspaceStore(createMockDouyuAdapter(), {
      storage,
      initialSidebarOpen: true,
    });

    expect(restored.getState().danmakuSettings).toEqual(expect.objectContaining({
      fontSize: 30,
      density: 'massive',
    }));
    expect(restored.getState().sidebarOpen).toBe(false);
  });

  it('persists global governance defaults and room overrides independently', () => {
    const storage = createMemoryStorage();
    const first = createWorkspaceStore(createMockDouyuAdapter(), {
      storage,
      initialRooms: [candidate('63136')],
    });

    first.getState().setDanmakuGovernance({ keywordBlacklist: ['广告'] });
    first.getState().setRoomDanmakuGovernanceOverride('63136', {
      duplicateWindowSeconds: 5,
    });

    expect(first.getState().danmakuSettings.governance.keywordBlacklist).toEqual(['广告']);
    expect(first.getState().danmakuGovernanceOverrides['63136']).toEqual({
      duplicateWindowSeconds: 5,
    });

    const restored = createWorkspaceStore(createMockDouyuAdapter(), {
      storage,
      initialRooms: [candidate('63136')],
    });
    expect(restored.getState().danmakuSettings.governance.keywordBlacklist).toEqual(['广告']);
    expect(restored.getState().danmakuGovernanceOverrides['63136']).toEqual({
      duplicateWindowSeconds: 5,
    });

    restored.getState().clearRoomDanmakuGovernanceOverride('63136');
    expect(restored.getState().danmakuGovernanceOverrides['63136']).toBeUndefined();
  });

  it('resets only the selected danmaku slider', () => {
    const store = createWorkspaceStore(createMockDouyuAdapter());
    store.getState().setDanmakuSettings({
      durationSeconds: 5,
      fontSize: 32,
      opacity: 0.5,
      region: 'bottom',
    });

    store.getState().resetDanmakuSetting('fontSize');

    expect(store.getState().danmakuSettings).toEqual(expect.objectContaining({
      durationSeconds: 5,
      fontSize: 24,
      opacity: 0.5,
      region: 'bottom',
    }));
  });

  it('restores persisted rooms, order, and preferences before checking streams', () => {
    const storage = createMemoryStorage();
    const first = createWorkspaceStore(createMockDouyuAdapter(), {
      storage,
      initialRooms: [candidate('a'), candidate('b')],
    });
    first.getState().setLayout('split-vertical');
    first.getState().setQuality('b', 'high');
    first.getState().toggleDanmaku('b');
    first.getState().setVolume('b', 0.25);
    first.getState().reorderRooms('b', 'a');

    const restored = createWorkspaceStore(createMockDouyuAdapter(), { storage });

    expect(restored.getState().rooms.map((room) => room.roomId)).toEqual(['b', 'a']);
    expect(restored.getState().layoutId).toBe('split-vertical');
    expect(restored.getState().rooms[0]).toEqual(expect.objectContaining({
      quality: 'high',
      danmakuEnabled: false,
      volume: 0.25,
      playbackAvailabilityStatus: 'checking',
    }));
  });

  it('updates layout and room-local display controls', () => {
    const store = createWorkspaceStore(createMockDouyuAdapter(), {
      initialRooms: [candidate('a'), candidate('b')],
    });

    store.getState().setLayout('split-vertical');
    store.getState().setPrimaryRoom('b');
    store.getState().setAudioRoom('b');
    store.getState().setQuality('b', 'original');
    store.getState().toggleDanmaku('b');

    expect(store.getState().layoutId).toBe('split-vertical');
    expect(store.getState().primaryRoomId).toBe('b');
    expect(store.getState().audioRoomId).toBe('b');
    expect(store.getState().rooms.find((room) => room.roomId === 'b')).toEqual(
      expect.objectContaining({ quality: 'original', danmakuEnabled: false }),
    );
  });

  it('searches through the adapter and exposes an empty result state', async () => {
    const store = createWorkspaceStore(createMockDouyuAdapter());

    await store.getState().searchRooms('星河');
    expect(store.getState().searchStatus).toBe('success');
    expect(store.getState().searchResults[0]?.anchorName).toBe('星河');

    await store.getState().searchRooms('不存在的主播');
    expect(store.getState().searchStatus).toBe('empty');
    expect(store.getState().searchResults).toEqual([]);
  });

  it('moves a newly added room from checking to blocked', async () => {
    const pending = deferredAvailability();
    const adapter: DouyuAdapter = {
      ...createMockDouyuAdapter(),
      getStreamAvailability: () => pending.promise,
    };
    const store = createWorkspaceStore(adapter);

    expect(store.getState().addRoom(candidate('63136'))).toBe('added');
    expect(store.getState().rooms[0].playbackAvailabilityStatus).toBe('checking');

    pending.resolve(blockedAvailability('63136'));
    await pending.promise;
    await Promise.resolve();

    expect(store.getState().rooms[0]).toEqual(expect.objectContaining({
      playbackAvailabilityStatus: 'blocked',
      streamAvailability: expect.objectContaining({ kind: 'blocked' }),
    }));
  });

  it('isolates a failed availability check to its room', async () => {
    const pendingByRoom = new Map([
      ['101', deferredAvailability()],
      ['202', deferredAvailability()],
    ]);
    const adapter: DouyuAdapter = {
      ...createMockDouyuAdapter(),
      getStreamAvailability: (roomId) => pendingByRoom.get(roomId)!.promise,
    };
    const store = createWorkspaceStore(adapter);

    store.getState().addRoom(candidate('101'));
    store.getState().addRoom(candidate('202'));
    pendingByRoom.get('101')!.reject(new Error('无法连接斗鱼，请检查网络后重试'));
    await pendingByRoom.get('101')!.promise.catch(() => undefined);
    await Promise.resolve();

    expect(store.getState().rooms.find((room) => room.roomId === '101')).toEqual(
      expect.objectContaining({
        playbackAvailabilityStatus: 'error',
        playbackError: '无法连接斗鱼，请检查网络后重试',
      }),
    );
    expect(store.getState().rooms.find((room) => room.roomId === '202')).toEqual(
      expect.objectContaining({ playbackAvailabilityStatus: 'checking' }),
    );
  });

  it('ignores a late availability result after the room is removed', async () => {
    const pending = deferredAvailability();
    const adapter: DouyuAdapter = {
      ...createMockDouyuAdapter(),
      getStreamAvailability: () => pending.promise,
    };
    const store = createWorkspaceStore(adapter);

    store.getState().addRoom(candidate('63136'));
    store.getState().removeRoom('63136');
    pending.resolve(blockedAvailability('63136'));
    await pending.promise;
    await Promise.resolve();

    expect(store.getState().rooms).toEqual([]);
  });

  it('refreshes one room without replacing another room object', async () => {
    const adapter = createMockDouyuAdapter();
    const store = createWorkspaceStore(adapter);
    store.getState().addRoom(candidate('101'));
    store.getState().addRoom(candidate('202'));
    await Promise.resolve();
    await Promise.resolve();
    const originalSecondRoom = store.getState().rooms[1];

    await store.getState().refreshStreamAvailability('101');

    expect(store.getState().rooms[1]).toBe(originalSecondRoom);
  });

  it('records playback checks and isolates runtime recovery diagnostics by room', async () => {
    const adapter = createMockDouyuAdapter();
    const store = createWorkspaceStore(adapter, deterministicOptions);
    store.getState().addRoom(candidate('101'));
    store.getState().addRoom(candidate('202'));
    await Promise.resolve();
    await Promise.resolve();

    store.getState().reportPlaybackRecovery('101', {
      attempt: 2,
      exhausted: false,
      errorCode: 'NETWORK_ERROR',
    });

    expect(store.getState().rooms.find((room) => room.roomId === '101')).toEqual(
      expect.objectContaining({
        playbackCheckedAt: expect.any(String),
        playbackRecovery: {
          attempt: 2,
          exhausted: false,
          errorCode: 'NETWORK_ERROR',
          updatedAt: '2026-08-10T00:00:00.000Z',
        },
      }),
    );
    expect(store.getState().rooms.find((room) => room.roomId === '202')?.playbackRecovery).toBeUndefined();

    await store.getState().refreshStreamAvailability('101');

    expect(store.getState().rooms.find((room) => room.roomId === '101')).toEqual(
      expect.objectContaining({
        playbackCheckedAt: expect.any(String),
        playbackRecovery: undefined,
      }),
    );
    const refreshedRoom = store.getState().rooms.find((room) => room.roomId === '101');
    expect(refreshedRoom?.playbackCheckedAt).toBe(refreshedRoom?.streamAvailability?.checkedAt);
  });

  it('ignores availability refreshes for unknown room ids', async () => {
    const getStreamAvailability = vi.fn(createMockDouyuAdapter().getStreamAvailability);
    const adapter: DouyuAdapter = {
      ...createMockDouyuAdapter(),
      getStreamAvailability,
    };
    const store = createWorkspaceStore(adapter);

    await store.getState().refreshStreamAvailability('404');

    expect(getStreamAvailability).not.toHaveBeenCalled();
  });

  it('does not probe playback sources for offline rooms', async () => {
    const getStreamAvailability = vi.fn(createMockDouyuAdapter().getStreamAvailability);
    const store = createWorkspaceStore({
      ...createMockDouyuAdapter(),
      getStreamAvailability,
    }, {
      initialRooms: [{ ...candidate('offline'), online: false }],
    });

    await store.getState().refreshStreamAvailability('offline');

    expect(getStreamAvailability).not.toHaveBeenCalled();
  });

  it('refreshes missing room metadata from the room detail lookup', async () => {
    const avatarUrl = 'https://apic.douyucdn.cn/upload/avatar/room-1.jpg';
    const search = vi.fn(async (input) => input.type === 'room-id'
      ? [{ ...candidate(input.value), avatarUrl }]
      : []);
    const store = createWorkspaceStore({
      ...createMockDouyuAdapter(),
      search,
    }, { initialRooms: [candidate('1')] });

    await expect(store.getState().refreshRoomMetadata('1')).resolves.toBe(true);

    expect(search).toHaveBeenCalledWith({ type: 'room-id', value: '1' });
    expect(store.getState().rooms[0]).toEqual(expect.objectContaining({ avatarUrl }));
    expect(store.getState().roomLibrary['1']).toEqual(expect.objectContaining({ avatarUrl }));
  });
});
