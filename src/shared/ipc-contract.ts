import {
  DouyuAdapterError,
  STREAM_REQUEST_QUALITIES,
  type DouyuAdapterErrorCode,
  type RoomCandidate,
  type StreamAvailability,
  type StreamRequestQuality,
} from '../domain/douyu-adapter';

export const IPC_CHANNELS = {
  searchRooms: 'rooms.search',
  getStreamAvailability: 'playback.getAvailability',
  releaseStreamProxy: 'playback.releaseProxy',
  ping: 'app.ping',
  startDanmaku: 'danmaku.start',
  stopDanmaku: 'danmaku.stop',
  danmakuEvent: 'danmaku.event',
  windowMinimize: 'window.minimize',
  windowToggleMaximize: 'window.toggleMaximize',
  windowClose: 'window.close',
  windowMaximizedChanged: 'window.maximizedChanged',
  getSystemNotificationSupport: 'notifications.getSupport',
  showSystemNotification: 'notifications.show',
} as const;

export interface SearchRoomsRequest {
  input: string;
}

export interface GetStreamAvailabilityRequest {
  roomId: string;
  quality: StreamRequestQuality;
}

export interface ReleaseStreamProxyRequest {
  roomId: string;
}

export interface SystemNotificationRequest {
  title: string;
  body: string;
}

export interface IpcError {
  code:
    | 'INVALID_INPUT'
    | 'UNKNOWN'
    | 'ROOM_LIMIT'
    | 'NOTIFICATION_UNSUPPORTED'
    | 'NOTIFICATION_FAILED'
    | DouyuAdapterErrorCode;
  message: string;
  retryable: boolean;
}

export type IpcResult<T> = { ok: true; data: T } | { ok: false; error: IpcError };

export type SearchRoomsResult = IpcResult<RoomCandidate[]>;
export type GetStreamAvailabilityResult = IpcResult<StreamAvailability>;
export type SystemNotificationSupportResult = IpcResult<{ supported: boolean }>;

export function isValidSearchRoomsRequest(value: unknown): value is SearchRoomsRequest {
  if (!value || typeof value !== 'object' || !('input' in value)) return false;
  const input = (value as { input?: unknown }).input;
  return typeof input === 'string' && input.trim().length > 0 && input.trim().length <= 200;
}

export function isValidGetStreamAvailabilityRequest(
  value: unknown,
): value is GetStreamAvailabilityRequest {
  if (!isValidReleaseStreamProxyRequest(value) || !('quality' in value)) return false;
  const quality = (value as { quality?: unknown }).quality;
  return (
    typeof quality === 'string'
    && STREAM_REQUEST_QUALITIES.includes(quality as StreamRequestQuality)
  );
}

export function isValidReleaseStreamProxyRequest(
  value: unknown,
): value is ReleaseStreamProxyRequest {
  if (!value || typeof value !== 'object' || !('roomId' in value)) return false;
  const roomId = (value as { roomId?: unknown }).roomId;
  return typeof roomId === 'string' && /^\d{1,20}$/.test(roomId);
}

export function isValidSystemNotificationRequest(
  value: unknown,
): value is SystemNotificationRequest {
  if (!value || typeof value !== 'object') return false;
  const candidate = value as { title?: unknown; body?: unknown };
  return (
    typeof candidate.title === 'string'
    && candidate.title.trim().length > 0
    && candidate.title.trim().length <= 80
    && typeof candidate.body === 'string'
    && candidate.body.trim().length > 0
    && candidate.body.trim().length <= 240
  );
}

export function ok<T>(data: T): IpcResult<T> {
  return { ok: true, data };
}

export function toIpcError(error: unknown): IpcError {
  if (error && typeof error === 'object' && 'code' in error && (error as { code?: unknown }).code === 'INVALID_INPUT') {
    return { code: 'INVALID_INPUT', message: '请输入有效的直播间号或主播名字', retryable: false };
  }

  if (error instanceof DouyuAdapterError) {
    const details: Record<
      DouyuAdapterErrorCode,
      Pick<IpcError, 'message' | 'retryable'>
    > = {
      STREAMGET_UNAVAILABLE: {
        message: '\u65e0\u6cd5\u542f\u52a8 StreamGet\uff0c\u8bf7\u68c0\u67e5 Python \u73af\u5883',
        retryable: true,
      },
      LOCAL_STREAM_PROXY_FAILED: {
        message: '无法创建本地播放通道，请重试',
        retryable: true,
      },
      ROOM_NOT_FOUND: { message: '未找到对应直播间', retryable: false },
      NETWORK_UNAVAILABLE: {
        message: '无法连接斗鱼，请检查网络后重试',
        retryable: true,
      },
      PROTOCOL_CHANGED: {
        message: '斗鱼接口响应异常，请稍后重试',
        retryable: true,
      },
    };

    return { code: error.code, ...details[error.code] };
  }

  return { code: 'UNKNOWN', message: '操作失败，请稍后重试', retryable: true };
}

export function invalidInputError(): IpcResult<never> {
  return {
    ok: false,
    error: { code: 'INVALID_INPUT', message: '请输入有效的直播间号或主播名字', retryable: false },
  };
}

export function invalidRoomIdError(): IpcResult<never> {
  return {
    ok: false,
    error: { code: 'INVALID_INPUT', message: '请输入有效的直播间号', retryable: false },
  };
}

export function failed<T>(error: unknown): IpcResult<T> {
  return { ok: false, error: toIpcError(error) };
}
