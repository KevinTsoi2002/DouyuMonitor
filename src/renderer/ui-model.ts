import type { LayoutId } from '../domain/layout-engine';
import type { StreamQuality } from '../domain/douyu-adapter';
import type { DanmakuConnectionState } from '../shared/danmaku-contract';
import type { RoomSession } from './store/workspace-store';

export interface LayoutOption {
  id: LayoutId;
  label: string;
  shortLabel: string;
  hint: string;
}

export const LAYOUT_OPTIONS: LayoutOption[] = [
  { id: 'auto', label: '自动推荐', shortLabel: '自动', hint: '新增房间时自动适配布局' },
  { id: 'single', label: '单画面', shortLabel: '单', hint: '突出当前主画面' },
  { id: 'grid-2x2', label: '2 × 2 网格', shortLabel: '2×2', hint: '适合四路同时监看' },
  { id: 'grid-3x2', label: '3 × 2 网格', shortLabel: '3×2', hint: '适合六路同时监看' },
  { id: 'grid-3x3', label: '3 × 3 网格', shortLabel: '3×3', hint: '最多九路同时监看' },
  { id: 'primary-two', label: '主画面布局', shortLabel: '主画面', hint: '拖动分隔线调整主画面比例' },
  { id: 'split-horizontal', label: '横向分屏', shortLabel: '横向', hint: '左右并排对比' },
  { id: 'split-vertical', label: '纵向分屏', shortLabel: '纵向', hint: '上下堆叠对比' },
];

export const QUALITY_OPTIONS: Array<{ value: StreamQuality; label: string }> = [
  { value: 'auto', label: '自动' },
  { value: 'original', label: '原画' },
  { value: 'super', label: '超清' },
  { value: 'high', label: '高清' },
  { value: 'standard', label: '标清' },
];

export interface PlaybackPresentation {
  title: string;
  detail: string;
  qualityLabels: string[];
  qualityOptions: Array<{ value: StreamQuality; label: string }>;
  qualityDisabled: boolean;
  audioDisabled: boolean;
  canRetry: boolean;
}

export interface RoomActionSummary {
  playbackLabel: string;
  playbackDetail: string;
  danmakuLabel: string;
}

const BLOCKED_DETAILS = {
  ROOM_OFFLINE: '主播当前未开播',
  NO_PUBLIC_SOURCE: '斗鱼当前未提供公开直连',
  SIGNATURE_REQUIRED: '斗鱼当前只提供需签名的播放接口',
} as const;

const ROOM_TONES = ['coral', 'teal', 'violet', 'gold', 'blue', 'lime'];
const DANMAKU_STATE_LABELS: Record<DanmakuConnectionState, string> = {
  idle: '弹幕未连接',
  connecting: '弹幕连接中',
  connected: '弹幕已连接',
  reconnecting: '弹幕重连中',
  failed: '弹幕连接失败',
  'platform-blocked': '弹幕平台阻塞',
};

export function getRoomInitials(anchorName: string): string {
  const words = anchorName.trim().split(/\s+/).filter(Boolean);
  if (words.length > 1 && words.every((word) => /^[A-Za-z]/.test(word))) {
    return words.map((word) => word[0]).join('').slice(0, 2).toUpperCase();
  }
  return Array.from(anchorName.trim()).slice(0, 2).join('') || '--';
}

export function getRoomTone(index: number): string {
  return ROOM_TONES[((index % ROOM_TONES.length) + ROOM_TONES.length) % ROOM_TONES.length];
}

export function getLayoutOption(layoutId: LayoutId): LayoutOption {
  return LAYOUT_OPTIONS.find((option) => option.id === layoutId)
    ?? LAYOUT_OPTIONS.find((option) => option.id === 'single')
    ?? LAYOUT_OPTIONS[0];
}

export function getPlaybackPresentation(room: RoomSession): PlaybackPresentation {
  const availableVariants = room.streamAvailability?.kind === 'available'
    ? room.streamAvailability.variants
    : [];
  const qualityLabels = availableVariants.length > 0
    ? availableVariants.map((variant) => variant.label)
    : room.streamAvailability?.kind === 'blocked'
      ? room.streamAvailability.observedQualities.map((quality) => quality.label)
      : [];
  const qualityOptions = availableVariants.map((variant) => ({
    value: variant.quality,
    label: variant.label,
  }));
  const common = {
    qualityLabels,
    qualityOptions,
    qualityDisabled: room.playbackAvailabilityStatus !== 'available' || qualityOptions.length <= 1,
    audioDisabled: room.playbackAvailabilityStatus !== 'available',
    canRetry: room.playbackAvailabilityStatus === 'error',
  };

  if (!room.online || room.status === 'offline') {
    return {
      ...common,
      title: '主播当前未开播',
      detail: '开播后再检查播放源',
      canRetry: false,
    };
  }

  if (room.playbackAvailabilityStatus === 'checking') {
    return {
      ...common,
      title: '正在检查播放源',
      detail: '正在读取斗鱼公开播放能力',
    };
  }

  if (room.playbackAvailabilityStatus === 'blocked') {
    const reason = room.streamAvailability?.kind === 'blocked'
      ? room.streamAvailability.reason
      : 'NO_PUBLIC_SOURCE';
    return {
      ...common,
      title: '暂无合规播放源',
      detail: BLOCKED_DETAILS[reason],
    };
  }

  if (room.playbackAvailabilityStatus === 'error') {
    return {
      ...common,
      title: '播放能力检查失败',
      detail: room.playbackError ?? '请稍后重新检查',
    };
  }

  return {
    ...common,
    title: '播放源已就绪',
    detail: '当前来源已通过合规校验',
  };
}

export function getRoomActionSummary(
  room: RoomSession,
  danmakuState: DanmakuConnectionState,
): RoomActionSummary {
  const playback = getPlaybackPresentation(room);
  return {
    playbackLabel: playback.title,
    playbackDetail: playback.detail,
    danmakuLabel: DANMAKU_STATE_LABELS[danmakuState],
  };
}
