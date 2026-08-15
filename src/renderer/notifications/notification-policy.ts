export type PlaybackNotificationState =
  | 'offline'
  | 'checking'
  | 'playing'
  | 'blocked'
  | 'error'
  | 'recovering'
  | 'recovery-exhausted';

export interface RoomNotificationSnapshot {
  roomId: string;
  anchorName: string;
  online: boolean;
  playbackState: PlaybackNotificationState;
  playbackErrorCode?: string;
}

export type NotificationEventType =
  | 'playback-failed'
  | 'playback-recovered'
  | 'room-online'
  | 'room-offline';

export interface NotificationEvent {
  type: NotificationEventType;
  roomId: string;
  anchorName: string;
  title: string;
  body: string;
  dedupeKey: string;
  createdAt: number;
}

export interface NotificationPolicy {
  update(rooms: RoomNotificationSnapshot[]): NotificationEvent[];
  reset(): void;
}

export interface NotificationPolicyOptions {
  now?: () => number;
  dedupeWindowMs?: number;
  rateLimitWindowMs?: number;
  maxEventsPerWindow?: number;
}

const DEFAULT_DEDUPE_WINDOW_MS = 5 * 60 * 1000;
const DEFAULT_RATE_LIMIT_WINDOW_MS = 60 * 1000;
const DEFAULT_MAX_EVENTS_PER_WINDOW = 6;

const PLAYBACK_FAILURE_STATES = new Set<PlaybackNotificationState>([
  'blocked',
  'error',
  'recovery-exhausted',
]);
const PLAYBACK_RECOVERY_STATES = new Set<PlaybackNotificationState>([
  ...PLAYBACK_FAILURE_STATES,
  'recovering',
]);

function eventText(
  room: RoomNotificationSnapshot,
  type: NotificationEventType,
  diagnosticCode?: string,
): Pick<NotificationEvent, 'title' | 'body'> {
  if (type === 'playback-failed') {
    return {
      title: `${room.anchorName} 播放异常`,
      body: `房间 ${room.roomId}：${diagnosticCode ?? '播放状态异常'}`,
    };
  }
  if (type === 'playback-recovered') {
    return {
      title: `${room.anchorName} 播放已恢复`,
      body: `房间 ${room.roomId} 已恢复播放`,
    };
  }
  if (type === 'room-online') {
    return {
      title: `${room.anchorName} 正在直播`,
      body: `房间 ${room.roomId} 已开播`,
    };
  }
  return {
    title: `${room.anchorName} 已下播`,
    body: `房间 ${room.roomId} 已离线`,
  };
}

export function createNotificationPolicy(options: NotificationPolicyOptions = {}): NotificationPolicy {
  const now = options.now ?? Date.now;
  const dedupeWindowMs = options.dedupeWindowMs ?? DEFAULT_DEDUPE_WINDOW_MS;
  const rateLimitWindowMs = options.rateLimitWindowMs ?? DEFAULT_RATE_LIMIT_WINDOW_MS;
  const maxEventsPerWindow = options.maxEventsPerWindow ?? DEFAULT_MAX_EVENTS_PER_WINDOW;
  let previous = new Map<string, RoomNotificationSnapshot>();
  let initialized = false;
  const lastSentByKey = new Map<string, number>();
  let sentAt: number[] = [];

  function reset() {
    previous = new Map();
    initialized = false;
    lastSentByKey.clear();
    sentAt = [];
  }

  function update(rooms: RoomNotificationSnapshot[]): NotificationEvent[] {
    const timestamp = now();
    const current = new Map(rooms.map((room) => [room.roomId, room]));
    if (!initialized) {
      previous = current;
      initialized = true;
      return [];
    }

    sentAt = sentAt.filter((sentTimestamp) => sentTimestamp > timestamp - rateLimitWindowMs);
    const events: NotificationEvent[] = [];
    const emit = (
      room: RoomNotificationSnapshot,
      type: NotificationEventType,
      diagnosticCode?: string,
    ) => {
      const key = `${room.roomId}:${type}:${diagnosticCode ?? type}`;
      const lastSentAt = lastSentByKey.get(key);
      if (lastSentAt !== undefined && timestamp - lastSentAt < dedupeWindowMs) return;
      if (sentAt.length >= maxEventsPerWindow) return;
      const text = eventText(room, type, diagnosticCode);
      lastSentByKey.set(key, timestamp);
      sentAt.push(timestamp);
      events.push({
        type,
        roomId: room.roomId,
        anchorName: room.anchorName,
        ...text,
        dedupeKey: key,
        createdAt: timestamp,
      });
    };

    for (const room of rooms) {
      const before = previous.get(room.roomId);
      if (!before) continue;

      if (!before.online && room.online) emit(room, 'room-online');
      if (before.online && !room.online) emit(room, 'room-offline');

      const wasFailure = PLAYBACK_FAILURE_STATES.has(before.playbackState);
      const isFailure = PLAYBACK_FAILURE_STATES.has(room.playbackState);
      const wasRecoverable = PLAYBACK_RECOVERY_STATES.has(before.playbackState);
      if (!wasFailure && isFailure) {
        emit(room, 'playback-failed', room.playbackErrorCode ?? room.playbackState);
      } else if (wasRecoverable && room.playbackState === 'playing') {
        emit(room, 'playback-recovered');
      }
    }

    previous = current;
    return events;
  }

  return { update, reset };
}
