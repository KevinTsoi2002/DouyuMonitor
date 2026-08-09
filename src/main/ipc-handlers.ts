import type { DouyuAdapter } from '../domain/douyu-adapter';
import { resolveRoomInput } from '../domain/input-resolver';
import { isValidDanmakuRoomRequest } from '../shared/danmaku-contract';
import type {
  DanmakuEventTarget,
  DanmakuSessionManager,
} from './danmaku-session-manager';
import {
  IPC_CHANNELS,
  failed,
  invalidInputError,
  invalidRoomIdError,
  isValidGetStreamAvailabilityRequest,
  isValidSearchRoomsRequest,
  ok,
  type GetStreamAvailabilityResult,
  type IpcResult,
  type SearchRoomsRequest,
  type SearchRoomsResult,
} from '../shared/ipc-contract';

export interface IpcEventLike {
  sender: DanmakuEventTarget;
}

export type IpcListener = (event: IpcEventLike, request: unknown) => Promise<unknown>;

export interface IpcMainLike {
  handle(channel: string, listener: IpcListener): void;
}

export function registerIpcHandlers(
  ipcMain: IpcMainLike,
  adapter: DouyuAdapter,
  danmakuManager: DanmakuSessionManager,
): void {
  ipcMain.handle(IPC_CHANNELS.searchRooms, async (_event, request): Promise<SearchRoomsResult> => {
    if (!isValidSearchRoomsRequest(request)) return invalidInputError();

    try {
      const input = resolveRoomInput((request as SearchRoomsRequest).input);
      return ok(await adapter.search(input));
    } catch (error) {
      return failed(error);
    }
  });

  ipcMain.handle(
    IPC_CHANNELS.getStreamAvailability,
    async (_event, request): Promise<GetStreamAvailabilityResult> => {
      if (!isValidGetStreamAvailabilityRequest(request)) return invalidRoomIdError();

      try {
        return ok(await adapter.getStreamAvailability(request.roomId));
      } catch (error) {
        return failed(error);
      }
    },
  );

  ipcMain.handle(IPC_CHANNELS.ping, async (): Promise<IpcResult<{ status: 'ok' }>> => {
    return ok({ status: 'ok' });
  });

  ipcMain.handle(
    IPC_CHANNELS.startDanmaku,
    async (event, request): Promise<IpcResult<void>> => {
      if (!isValidDanmakuRoomRequest(request)) return invalidRoomIdError();
      if (danmakuManager.start(event.sender, request.roomId) === 'limit') {
        return {
          ok: false,
          error: {
            code: 'ROOM_LIMIT',
            message: '\u6700\u591a\u540c\u65f6\u8fde\u63a5 9 \u4e2a\u76f4\u64ad\u95f4\u5f39\u5e55',
            retryable: false,
          },
        };
      }
      return ok(undefined);
    },
  );

  ipcMain.handle(
    IPC_CHANNELS.stopDanmaku,
    async (event, request): Promise<IpcResult<void>> => {
      if (!isValidDanmakuRoomRequest(request)) return invalidRoomIdError();
      danmakuManager.stop(event.sender.id, request.roomId);
      return ok(undefined);
    },
  );
}
