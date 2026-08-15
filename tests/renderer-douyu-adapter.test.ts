import { afterEach, describe, expect, it, vi } from 'vitest';
import type { RoomCandidate } from '../src/domain/douyu-adapter';
import { createRendererDouyuAdapter } from '../src/infrastructure/renderer-douyu-adapter';
import type { AppApi } from '../src/preload/bridge';
import { ok } from '../src/shared/ipc-contract';

const candidate: RoomCandidate = {
  roomId: '63136',
  anchorName: 'Test Anchor',
  title: 'Test Room',
  category: 'Game',
  online: true,
  viewerLabel: '1.2万',
};

function installAppApi(appApi: AppApi): void {
  Object.defineProperty(globalThis, 'window', {
    configurable: true,
    value: { appApi },
  });
}

function createTestAppApi(overrides: Partial<AppApi>): AppApi {
  const unavailable = async (): Promise<never> => {
    throw new Error('Unused test API method');
  };
  return {
    searchRooms: unavailable,
    getStreamAvailability: unavailable,
    startDanmaku: async () => ok(undefined),
    stopDanmaku: async () => ok(undefined),
    onDanmakuEvent: () => () => {},
    minimizeWindow: async () => {},
    toggleMaximizeWindow: async () => {},
    closeWindow: async () => {},
    onMaximizedChanged: () => () => {},
    getSystemNotificationSupport: unavailable,
    showSystemNotification: unavailable,
    ping: async () => ok({ status: 'ok' as const }),
    ...overrides,
  };
}

afterEach(() => {
  vi.restoreAllMocks();
  Reflect.deleteProperty(globalThis, 'window');
});

describe('createRendererDouyuAdapter', () => {
  it('searches through the preload API when the Electron bridge is available', async () => {
    const searchRooms = vi.fn(async () => ok([candidate]));
    installAppApi(createTestAppApi({ searchRooms }));

    const adapter = createRendererDouyuAdapter();

    await expect(adapter.search({ type: 'room-id', value: '63136' })).resolves.toEqual([candidate]);
    expect(searchRooms).toHaveBeenCalledWith('63136');
  });

  it('throws the user-facing IPC error message', async () => {
    installAppApi(createTestAppApi({
      searchRooms: vi.fn(async () => ({
        ok: false as const,
        error: { code: 'UNKNOWN' as const, message: '搜索失败，请稍后重试', retryable: true },
      })),
    }));

    const adapter = createRendererDouyuAdapter();

    await expect(adapter.search({ type: 'anchor-name', value: '星河' })).rejects.toThrow(
      '搜索失败，请稍后重试',
    );
  });

  it('falls back to the mock adapter outside Electron', async () => {
    const adapter = createRendererDouyuAdapter();

    const result = await adapter.search({ type: 'room-id', value: '12345' });

    expect(result[0].roomId).toBe('12345');
  });

  it('checks availability through the preload API in Electron', async () => {
    const availability = {
      kind: 'blocked' as const,
      roomId: '63136',
      reason: 'SIGNATURE_REQUIRED' as const,
      observedQualities: [],
      checkedAt: '2026-08-07T00:00:00.000Z',
    };
    const getStreamAvailability = vi.fn(async () => ok(availability));
    installAppApi(createTestAppApi({
      getStreamAvailability,
    }));

    const adapter = createRendererDouyuAdapter();

    await expect(adapter.getStreamAvailability('63136')).resolves.toEqual(availability);
    expect(getStreamAvailability).toHaveBeenCalledWith('63136');
  });

  it('throws the availability IPC error without falling back to mock data', async () => {
    installAppApi(createTestAppApi({
      getStreamAvailability: vi.fn(async () => ({
        ok: false as const,
        error: {
          code: 'NETWORK_UNAVAILABLE' as const,
          message: '无法连接斗鱼，请检查网络后重试',
          retryable: true,
        },
      })),
    }));

    const adapter = createRendererDouyuAdapter();

    await expect(adapter.getStreamAvailability('63136')).rejects.toThrow(
      '无法连接斗鱼，请检查网络后重试',
    );
  });
});
