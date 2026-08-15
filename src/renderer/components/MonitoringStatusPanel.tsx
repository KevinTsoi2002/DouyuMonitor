import {
  Activity,
  AlertCircle,
  CheckCircle2,
  CircleSlash2,
  Clock3,
  LoaderCircle,
  MessageCircle,
  RefreshCw,
  ShieldAlert,
  X,
} from 'lucide-react';
import { useEffect, useMemo } from 'react';
import type { DanmakuConnectionState } from '../../shared/danmaku-contract';
import {
  formatMonitoringTime,
  getMonitoringSummary,
  getRoomMonitoringView,
  type PlaybackMonitoringState,
  type MonitoringTone,
} from '../monitoring-status';
import { PLAYBACK_RECOVERY_MAX_ATTEMPTS } from '../playback-recovery';
import { useDanmakuControls, useDanmakuIssueCount, useDanmakuStatus } from '../store/danmaku-context';
import { useWorkspace } from '../store/workspace-context';
import type { RoomSession } from '../store/workspace-store';

interface MonitoringStatusPanelProps {
  open: boolean;
  onClose: () => void;
}

const PLAYBACK_LABELS: Record<PlaybackMonitoringState, string> = {
  offline: '未开播',
  checking: '检查中',
  playing: '播放正常',
  blocked: '平台阻塞',
  error: '播放检查失败',
  recovering: '自动恢复中',
  'recovery-exhausted': '播放失败',
};

const DANMAKU_LABELS: Record<DanmakuConnectionState, string> = {
  idle: '未连接',
  connecting: '连接中',
  connected: '已连接',
  reconnecting: '重连中',
  failed: '连接失败',
  'platform-blocked': '平台阻塞',
};

const TONE_ICONS: Record<MonitoringTone, typeof CheckCircle2> = {
  healthy: CheckCircle2,
  pending: LoaderCircle,
  danger: AlertCircle,
  muted: CircleSlash2,
};

function StatusLine({
  icon: Icon,
  label,
  tone,
}: {
  icon: typeof CheckCircle2;
  label: string;
  tone: MonitoringTone;
}) {
  return (
    <span className={`monitor-status-line is-${tone}`}>
      <Icon className={tone === 'pending' ? 'spin' : undefined} size={13} aria-hidden="true" />
      <span>{label}</span>
    </span>
  );
}

function RoomMonitorRow({
  room,
  onRefresh,
}: {
  room: RoomSession;
  onRefresh: () => void;
}) {
  const danmakuStatus = useDanmakuStatus(room.roomId);
  const { retryRoom } = useDanmakuControls();
  const view = getRoomMonitoringView(room, danmakuStatus);
  const PlaybackIcon = TONE_ICONS[view.playbackTone];
  const DanmakuIcon = TONE_ICONS[view.danmakuTone];
  const canRetryDanmaku = danmakuStatus.state === 'failed' || danmakuStatus.state === 'platform-blocked';

  return (
    <li className="monitor-room-row" data-room-id={room.roomId}>
      <div className="monitor-room-heading">
        <div className="monitor-room-title">
          <span className={`status-dot ${view.online ? 'status-dot-live' : 'status-dot-offline'}`} />
          <strong title={room.anchorName}>{room.anchorName}</strong>
          <span>#{room.roomId}</span>
        </div>
        <button
          className="tiny-icon-button"
          type="button"
          aria-label={`${room.anchorName} 刷新播放源`}
          title="刷新播放源"
          disabled={!view.online}
          onClick={onRefresh}
        >
          <RefreshCw size={14} />
        </button>
      </div>
      <div className="monitor-room-details">
        <StatusLine icon={PlaybackIcon} label={`播放：${PLAYBACK_LABELS[view.playbackState]}`} tone={view.playbackTone} />
        <StatusLine icon={DanmakuIcon} label={`弹幕：${DANMAKU_LABELS[view.danmakuState]}`} tone={view.danmakuTone} />
        <span className="monitor-room-meta"><Clock3 size={12} aria-hidden="true" />最近检查：{formatMonitoringTime(view.lastCheckedAt)}</span>
        <span className="monitor-room-meta"><Activity size={12} aria-hidden="true" />自动恢复：{view.playbackAttempt ? `${view.playbackAttempt}/${PLAYBACK_RECOVERY_MAX_ATTEMPTS}` : '未发生'}</span>
      </div>
      <div className="monitor-room-footer">
        <span className={view.lastErrorType ? 'monitor-room-error' : 'monitor-room-meta'}>
          {view.lastErrorType ? `最近错误：${view.lastErrorType}` : '最近错误：无'}
        </span>
        {canRetryDanmaku ? (
          <button
            className="tiny-icon-button"
            type="button"
            aria-label={`${room.anchorName} 重试弹幕`}
            title="重试弹幕"
            onClick={() => { void retryRoom(room.roomId); }}
          >
            <MessageCircle size={14} />
          </button>
        ) : null}
      </div>
    </li>
  );
}

export function MonitoringStatusPanel({ open, onClose }: MonitoringStatusPanelProps) {
  const rooms = useWorkspace((state) => state.rooms);
  const refreshStreamAvailability = useWorkspace((state) => state.refreshStreamAvailability);
  const roomIdsKey = useWorkspace((state) => state.rooms.map((room) => room.roomId).join('|'));
  const danmakuIssueCount = useDanmakuIssueCount(roomIdsKey);
  const summary = useMemo(() => {
    const playbackSummary = getMonitoringSummary(rooms);
    return { ...playbackSummary, danmakuIssues: danmakuIssueCount };
  }, [danmakuIssueCount, rooms]);

  useEffect(() => {
    if (!open) return undefined;
    const handleKeyDown = (event: KeyboardEvent) => {
      if (event.key === 'Escape') onClose();
    };
    document.addEventListener('keydown', handleKeyDown);
    return () => document.removeEventListener('keydown', handleKeyDown);
  }, [onClose, open]);

  if (!open) return null;

  const issueCount = summary.playbackIssues + summary.danmakuIssues;
  return (
    <section
      className="monitoring-status-panel"
      id="monitoring-status-panel"
      role="dialog"
      aria-modal="false"
      aria-labelledby="monitoring-status-title"
    >
      <div className="monitoring-panel-heading">
        <div>
          <span className="section-kicker">LIVE HEALTH</span>
          <h2 id="monitoring-status-title">监控状态</h2>
        </div>
        <div className="monitoring-panel-heading-actions">
          {issueCount ? <span className="monitoring-issue-count">{issueCount} 个异常</span> : null}
          <button className="icon-button" type="button" aria-label="关闭监控状态" title="关闭" onClick={onClose}>
            <X size={16} />
          </button>
        </div>
      </div>

      <div className="monitor-summary-grid" aria-label="监控汇总">
        <div className="monitor-summary-item"><strong>{summary.online}</strong><span>在线</span></div>
        <div className="monitor-summary-item is-good"><strong>{summary.playing}</strong><span>播放中</span></div>
        <div className={`monitor-summary-item ${summary.playbackIssues ? 'is-danger' : ''}`}><strong>{summary.playbackIssues}</strong><span>播放异常</span></div>
        <div className={`monitor-summary-item ${summary.danmakuIssues ? 'is-danger' : ''}`}><strong>{summary.danmakuIssues}</strong><span>弹幕异常</span></div>
      </div>

      {rooms.length ? (
        <ul className="monitor-room-list">
          {rooms.map((room) => (
            <RoomMonitorRow
              key={room.roomId}
              room={room}
              onRefresh={() => { void refreshStreamAvailability(room.roomId); }}
            />
          ))}
        </ul>
      ) : (
        <div className="monitoring-empty">
          <ShieldAlert size={21} aria-hidden="true" />
          <strong>暂无直播间</strong>
          <span>添加直播间后，这里会显示播放和弹幕状态。</span>
        </div>
      )}
    </section>
  );
}
