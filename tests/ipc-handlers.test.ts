import { describe, expect, it, vi } from 'vitest';
import type { DouyuAdapter } from '../src/domain/douyu-adapter';
import { createMockDouyuAdapter } from '../src/infrastructure/mock-douyu-adapter';
import type { DanmakuSessionManager } from '../src/main/danmaku-session-manager';
import { registerIpcHandlers, type IpcMainLike } from '../src/main/ipc-handlers';
import { invalidRoomIdError, IPC_CHANNELS } from '../src/shared/ipc-contract';

function createFakeIpcMain() {
  const handlers = new Map<string, (event: unknown, request: unknown) => Promise<unknown>>();
  const ipcMain: IpcMainLike = {
    handle(channel, listener) {
      handlers.set(
        channel,
        listener as (event: unknown, request: unknown) => Promise<unknown>,
      );
    },
  };
  return { ipcMain, handlers };
}

function createFakeManager(): DanmakuSessionManager {
  return {
    start: vi.fn<DanmakuSessionManager['start']>(() => 'started'),
    stop: vi.fn<DanmakuSessionManager['stop']>(),
    stopOwner: vi.fn<DanmakuSessionManager['stopOwner']>(),
    stopAll: vi.fn<DanmakuSessionManager['stopAll']>(),
  };
}

const sender = {
  id: 7,
  isDestroyed: () => false,
  send: vi.fn<(channel: string, event: unknown) => void>(),
};

describe('registerIpcHandlers', () => {
  it('registers a search handler that resolves a room id through the adapter', async () => {
    const { ipcMain, handlers } = createFakeIpcMain();
    registerIpcHandlers(ipcMain, createMockDouyuAdapter(), createFakeManager());

    const result = await handlers.get(IPC_CHANNELS.searchRooms)?.({}, { input: '63136' });

    expect(result).toEqual(expect.objectContaining({ ok: true }));
    expect((result as { ok: true; data: Array<{ roomId: string }> }).data[0].roomId).toBe('63136');
  });

  it('rejects malformed input before calling the adapter', async () => {
    const { ipcMain, handlers } = createFakeIpcMain();
    let calls = 0;
    const adapter: DouyuAdapter = {
      ...createMockDouyuAdapter(),
      search: async () => { calls += 1; return []; },
    };
    registerIpcHandlers(ipcMain, adapter, createFakeManager());

    const result = await handlers.get(IPC_CHANNELS.searchRooms)?.({}, { input: '   ' });

    expect(result).toEqual({
      ok: false,
      error: { code: 'INVALID_INPUT', message: '请输入有效的直播间号或主播名字', retryable: false },
    });
    expect(calls).toBe(0);
  });

  it('maps adapter failures to a retryable sanitized error', async () => {
    const { ipcMain, handlers } = createFakeIpcMain();
    const adapter: DouyuAdapter = {
      ...createMockDouyuAdapter(),
      search: async () => { throw new Error('cookie=secret'); },
    };
    registerIpcHandlers(ipcMain, adapter, createFakeManager());

    const result = await handlers.get(IPC_CHANNELS.searchRooms)?.({}, { input: '星河' });

    expect(result).toEqual({
      ok: false,
      error: { code: 'UNKNOWN', message: '操作失败，请稍后重试', retryable: true },
    });
  });

  it('registers a health ping without exposing process details', async () => {
    const { ipcMain, handlers } = createFakeIpcMain();
    registerIpcHandlers(ipcMain, createMockDouyuAdapter(), createFakeManager());

    await expect(handlers.get(IPC_CHANNELS.ping)?.({}, undefined)).resolves.toEqual({ ok: true, data: { status: 'ok' } });
  });

  it('registers a typed stream availability handler', async () => {
    const { ipcMain, handlers } = createFakeIpcMain();
    registerIpcHandlers(ipcMain, createMockDouyuAdapter(), createFakeManager());

    const result = await handlers.get(IPC_CHANNELS.getStreamAvailability)?.(
      {},
      { roomId: '63136' },
    );

    expect(result).toEqual(expect.objectContaining({
      ok: true,
      data: expect.objectContaining({ kind: 'available', roomId: '63136' }),
    }));
  });

  it('rejects malformed room ids before checking availability', async () => {
    const { ipcMain, handlers } = createFakeIpcMain();
    let calls = 0;
    const adapter: DouyuAdapter = {
      ...createMockDouyuAdapter(),
      getStreamAvailability: async (roomId) => {
        calls += 1;
        return createMockDouyuAdapter().getStreamAvailability(roomId);
      },
    };
    registerIpcHandlers(ipcMain, adapter, createFakeManager());

    const result = await handlers.get(IPC_CHANNELS.getStreamAvailability)?.(
      {},
      { roomId: 'abc' },
    );

    expect(result).toEqual({
      ok: false,
      error: { code: 'INVALID_INPUT', message: '请输入有效的直播间号', retryable: false },
    });
    expect(calls).toBe(0);
  });

  it('starts and stops a validated room for the sending owner', async () => {
    const { ipcMain, handlers } = createFakeIpcMain();
    const manager = createFakeManager();
    registerIpcHandlers(ipcMain, createMockDouyuAdapter(), manager);

    await expect(
      handlers.get(IPC_CHANNELS.startDanmaku)?.({ sender }, { roomId: '63136' }),
    ).resolves.toEqual({ ok: true, data: undefined });
    expect(manager.start).toHaveBeenCalledWith(sender, '63136');

    await expect(
      handlers.get(IPC_CHANNELS.stopDanmaku)?.({ sender }, { roomId: '63136' }),
    ).resolves.toEqual({ ok: true, data: undefined });
    expect(manager.stop).toHaveBeenCalledWith(sender.id, '63136');
  });

  it('rejects malformed danmaku room ids before calling the manager', async () => {
    const { ipcMain, handlers } = createFakeIpcMain();
    const manager = createFakeManager();
    registerIpcHandlers(ipcMain, createMockDouyuAdapter(), manager);

    await expect(
      handlers.get(IPC_CHANNELS.startDanmaku)?.({ sender }, { roomId: 'abc' }),
    ).resolves.toEqual(invalidRoomIdError());
    await expect(
      handlers.get(IPC_CHANNELS.stopDanmaku)?.({ sender }, { roomId: 'abc' }),
    ).resolves.toEqual(invalidRoomIdError());
    expect(manager.start).not.toHaveBeenCalled();
    expect(manager.stop).not.toHaveBeenCalled();
  });

  it('maps the tenth distinct room to a fixed non-retryable error', async () => {
    const { ipcMain, handlers } = createFakeIpcMain();
    const manager = createFakeManager();
    vi.mocked(manager.start).mockReturnValue('limit');
    registerIpcHandlers(ipcMain, createMockDouyuAdapter(), manager);

    await expect(
      handlers.get(IPC_CHANNELS.startDanmaku)?.({ sender }, { roomId: '63136' }),
    ).resolves.toEqual({
      ok: false,
      error: {
        code: 'ROOM_LIMIT',
        message: '\u6700\u591a\u540c\u65f6\u8fde\u63a5 9 \u4e2a\u76f4\u64ad\u95f4\u5f39\u5e55',
        retryable: false,
      },
    });
  });
});
