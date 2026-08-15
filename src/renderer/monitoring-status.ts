import type { DanmakuConnectionState, DanmakuStatus } from '../shared/danmaku-contract';
import type { RoomSession } from './store/workspace-store';

export type PlaybackMonitoringState =
  | 'offline'
  | 'checking'
  | 'playing'
  | 'blocked'
  | 'error'
  | 'recovering'
  | 'recovery-exhausted';

export type MonitoringTone = 'healthy' | 'pending' | 'danger' | 'muted';

export interface MonitoringSummary {
  online: number;
  playing: number;
  playbackIssues: number;
  danmakuIssues: number;
}

export interface RoomMonitoringView {
  roomId: string;
  online: boolean;
  playbackState: PlaybackMonitoringState;
  playbackTone: MonitoringTone;
  playbackAttempt: number;
  danmakuState: DanmakuConnectionState;
  danmakuTone: MonitoringTone;
  lastCheckedAt?: string;
  lastErrorType?: string;
}

const PLAYBACK_ISSUE_STATES = new Set<PlaybackMonitoringState>([
  'blocked',
  'error',
  'recovering',
  'recovery-exhausted',
]);

function getPlaybackState(room: RoomSession): PlaybackMonitoringState {
  if (!room.online || room.status === 'offline') return 'offline';
  if (room.playbackRecovery?.exhausted) return 'recovery-exhausted';
  if (room.playbackRecovery) return 'recovering';
  if (room.playbackAvailabilityStatus === 'available') return 'playing';
  return room.playbackAvailabilityStatus;
}

function getPlaybackTone(state: PlaybackMonitoringState): MonitoringTone {
  if (state === 'playing') return 'healthy';
  if (state === 'checking' || state === 'recovering') return 'pending';
  if (state === 'blocked' || state === 'error' || state === 'recovery-exhausted') return 'danger';
  return 'muted';
}

function getDanmakuTone(state: DanmakuConnectionState): MonitoringTone {
  if (state === 'connected') return 'healthy';
  if (state === 'connecting' || state === 'reconnecting') return 'pending';
  if (state === 'failed' || state === 'platform-blocked') return 'danger';
  return 'muted';
}

function getFallbackErrorType(room: RoomSession): string | undefined {
  if (!room.online || room.status === 'offline') return undefined;
  if (room.playbackErrorCode) return room.playbackErrorCode;
  if (room.playbackAvailabilityStatus === 'error') return 'PLAYBACK_SOURCE_CHECK_FAILED';
  if (room.streamAvailability?.kind === 'blocked') return room.streamAvailability.reason;
  return undefined;
}

export function getRoomMonitoringView(
  room: RoomSession,
  danmakuStatus?: DanmakuStatus,
): RoomMonitoringView {
  const playbackState = getPlaybackState(room);
  const online = room.online && room.status !== 'offline';
  const danmakuState = online ? danmakuStatus?.state ?? 'idle' : 'idle';

  return {
    roomId: room.roomId,
    online,
    playbackState,
    playbackTone: getPlaybackTone(playbackState),
    playbackAttempt: room.playbackRecovery?.attempt ?? 0,
    danmakuState,
    danmakuTone: getDanmakuTone(danmakuState),
    lastCheckedAt: room.playbackCheckedAt ?? room.streamAvailability?.checkedAt,
    lastErrorType: room.playbackRecovery?.errorCode
      ?? (online ? danmakuStatus?.errorCode : undefined)
      ?? getFallbackErrorType(room),
  };
}

export function getMonitoringSummary(
  rooms: readonly RoomSession[],
  danmakuStatuses: Readonly<Record<string, DanmakuStatus | undefined>> = {},
): MonitoringSummary {
  const views = rooms.map((room) => getRoomMonitoringView(room, danmakuStatuses[room.roomId]));
  return {
    online: views.filter((view) => view.online).length,
    playing: views.filter((view) => view.playbackState === 'playing').length,
    playbackIssues: views.filter((view) => view.online && PLAYBACK_ISSUE_STATES.has(view.playbackState)).length,
    danmakuIssues: views.filter((view) => view.online && view.danmakuTone === 'danger').length,
  };
}

export function formatMonitoringTime(value: string | undefined, now = new Date()): string {
  if (!value) return '未检查';
  const timestamp = Date.parse(value);
  if (Number.isNaN(timestamp)) return '未检查';

  const elapsedSeconds = Math.max(0, Math.floor((now.getTime() - timestamp) / 1000));
  if (elapsedSeconds < 60) return '刚刚';
  const minutes = Math.floor(elapsedSeconds / 60);
  if (minutes < 60) return `${minutes} 分钟前`;
  const hours = Math.floor(minutes / 60);
  if (hours < 24) return `${hours} 小时前`;

  const date = new Date(timestamp);
  const pad = (part: number) => String(part).padStart(2, '0');
  return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())} ${pad(date.getHours())}:${pad(date.getMinutes())}`;
}
