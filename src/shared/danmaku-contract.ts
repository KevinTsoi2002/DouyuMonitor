export type DanmakuConnectionState =
  | 'idle'
  | 'connecting'
  | 'connected'
  | 'reconnecting'
  | 'failed'
  | 'platform-blocked';

export type DanmakuErrorCode =
  | 'NETWORK_UNAVAILABLE'
  | 'HANDSHAKE_TIMEOUT'
  | 'PROTOCOL_CHANGED'
  | 'AUTH_REQUIRED'
  | 'RETRY_EXHAUSTED';

export interface DanmakuMessage {
  id: string;
  roomId: string;
  nickname: string;
  text: string;
  receivedAt: string;
}

export interface DanmakuStatus {
  roomId: string;
  state: DanmakuConnectionState;
  attempt?: number;
  errorCode?: DanmakuErrorCode;
}

export type DanmakuEvent =
  | {
      type: 'messages';
      roomId: string;
      messages: DanmakuMessage[];
      dropped: number;
    }
  | { type: 'status'; status: DanmakuStatus };

export interface DanmakuRoomRequest {
  roomId: string;
}

const ROOM_ID_PATTERN = /^\d{1,20}$/;

const CONNECTION_STATES: readonly DanmakuConnectionState[] = [
  'idle',
  'connecting',
  'connected',
  'reconnecting',
  'failed',
  'platform-blocked',
];

const ERROR_CODES: readonly DanmakuErrorCode[] = [
  'NETWORK_UNAVAILABLE',
  'HANDSHAKE_TIMEOUT',
  'PROTOCOL_CHANGED',
  'AUTH_REQUIRED',
  'RETRY_EXHAUSTED',
];

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object';
}

function isRoomId(value: unknown): value is string {
  return typeof value === 'string' && ROOM_ID_PATTERN.test(value);
}

function hasAtMostCodePoints(value: unknown, limit: number): value is string {
  return typeof value === 'string' && Array.from(value).length <= limit;
}

function isDanmakuMessage(value: unknown, roomId: string): value is DanmakuMessage {
  if (!isRecord(value)) return false;

  return (
    typeof value.id === 'string' &&
    value.id.length >= 1 &&
    value.id.length <= 200 &&
    value.roomId === roomId &&
    hasAtMostCodePoints(value.nickname, 40) &&
    hasAtMostCodePoints(value.text, 200) &&
    typeof value.receivedAt === 'string' &&
    !Number.isNaN(Date.parse(value.receivedAt))
  );
}

function isDanmakuStatus(value: unknown): value is DanmakuStatus {
  if (!isRecord(value)) return false;

  return (
    isRoomId(value.roomId) &&
    typeof value.state === 'string' &&
    CONNECTION_STATES.includes(value.state as DanmakuConnectionState) &&
    (value.attempt === undefined ||
      (Number.isInteger(value.attempt) && (value.attempt as number) >= 0)) &&
    (value.errorCode === undefined ||
      (typeof value.errorCode === 'string' &&
        ERROR_CODES.includes(value.errorCode as DanmakuErrorCode)))
  );
}

export function isValidDanmakuRoomRequest(value: unknown): value is DanmakuRoomRequest {
  return isRecord(value) && isRoomId(value.roomId);
}

export function isDanmakuEvent(value: unknown): value is DanmakuEvent {
  if (!isRecord(value)) return false;

  if (value.type === 'messages') {
    return (
      isRoomId(value.roomId) &&
      Array.isArray(value.messages) &&
      value.messages.length <= 10 &&
      value.messages.every((message) => isDanmakuMessage(message, value.roomId as string)) &&
      Number.isInteger(value.dropped) &&
      (value.dropped as number) >= 0
    );
  }

  if (value.type === 'status') {
    return isDanmakuStatus(value.status);
  }

  return false;
}
