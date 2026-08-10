import { Crown, LoaderCircle, MessageCircle, MessageCircleOff, MoreHorizontal, RotateCw, ShieldAlert, Volume2, VolumeX } from 'lucide-react';
import { useCallback, useEffect, useRef, useState, type CSSProperties } from 'react';
import type { LayoutSlot } from '../../domain/layout-engine';
import type { RoomSession } from '../store/workspace-store';
import { useWorkspace } from '../store/workspace-context';
import { useDanmakuControls, useDanmakuRoom } from '../store/danmaku-context';
import { getPlaybackPresentation, getRoomActionSummary, getRoomTone } from '../ui-model';
import { scheduleControlsHide } from '../player-controls-visibility';
import { RoomPlaybackSurface } from './RoomPlaybackSurface';

interface RoomTileProps {
  room: RoomSession;
  slot: LayoutSlot;
  index: number;
  controlsLocked?: boolean;
}

const STATUS_LABELS = {
  playing: '直播中',
  offline: '离线',
  reconnecting: '重连中',
  error: '信号异常',
} as const;

export function RoomTile({ room, slot, index, controlsLocked = false }: RoomTileProps) {
  const primaryRoomId = useWorkspace((state) => state.primaryRoomId);
  const audioRoomId = useWorkspace((state) => state.audioRoomId);
  const globalDanmakuEnabled = useWorkspace((state) => state.globalDanmakuEnabled);
  const danmakuSettings = useWorkspace((state) => state.danmakuSettings);
  const globalMuted = useWorkspace((state) => state.globalMuted);
  const setPrimaryRoom = useWorkspace((state) => state.setPrimaryRoom);
  const setAudioRoom = useWorkspace((state) => state.setAudioRoom);
  const setQuality = useWorkspace((state) => state.setQuality);
  const setVolume = useWorkspace((state) => state.setVolume);
  const toggleDanmaku = useWorkspace((state) => state.toggleDanmaku);
  const removeRoom = useWorkspace((state) => state.removeRoom);
  const refreshRoomMetadata = useWorkspace((state) => state.refreshRoomMetadata);
  const refreshStreamAvailability = useWorkspace((state) => state.refreshStreamAvailability);
  const demoMode = useWorkspace((state) => state.demoMode);
  const danmakuView = useDanmakuRoom(room.roomId);
  const { retryRoom } = useDanmakuControls();
  const [menuOpen, setMenuOpen] = useState(false);
  const [controlsVisible, setControlsVisible] = useState(true);
  const [focusWithin, setFocusWithin] = useState(false);
  const hideCleanupRef = useRef<() => void>(() => {});
  const isPrimary = room.roomId === primaryRoomId;
  const isAudio = room.roomId === audioRoomId;
  const presentation = getPlaybackPresentation(room);
  const actionSummary = getRoomActionSummary(room, danmakuView.status.state);
  const disabledQualityOptions = room.streamAvailability?.kind === 'blocked'
    && room.streamAvailability.observedQualities.length > 0
    ? room.streamAvailability.observedQualities.map((quality) => ({
        value: quality.id,
        label: quality.label,
      }))
    : [{ value: 'unavailable', label: '不可用' }];
  const qualityOptions = room.streamAvailability?.kind === 'available'
    ? presentation.qualityOptions
    : disabledQualityOptions;
  const selectedQuality = room.streamAvailability?.kind === 'available'
    ? qualityOptions.find((option) => option.value === room.quality)?.value
      ?? qualityOptions[0]?.value
      ?? room.quality
    : disabledQualityOptions[0].value;
  const hasAudioFocus = isAudio && !presentation.audioDisabled;
  const tone = getRoomTone(index);
  const tileStyle = {
    gridColumn: `${slot.column} / span ${slot.columnSpan}`,
    gridRow: `${slot.row} / span ${slot.rowSpan}`,
    '--room-tone': `var(--tone-${getRoomTone(index)})`,
  } as CSSProperties;
  const resetControlsHide = useCallback(() => {
    hideCleanupRef.current();
    hideCleanupRef.current = scheduleControlsHide({
      locked: controlsLocked || focusWithin || menuOpen,
      onHide: () => setControlsVisible(false),
    });
  }, [controlsLocked, focusWithin, menuOpen]);
  const showControls = useCallback(() => {
    setControlsVisible(true);
    resetControlsHide();
  }, [resetControlsHide]);

  useEffect(() => {
    if (controlsLocked) setControlsVisible(true);
  }, [controlsLocked]);

  useEffect(() => {
    resetControlsHide();
    return () => hideCleanupRef.current();
  }, [resetControlsHide, room.roomId]);

  return (
    <article
      data-room-id={room.roomId}
      className={`room-tile ${isPrimary ? 'is-primary' : ''} ${controlsVisible ? 'controls-visible' : 'controls-hidden'}`}
      style={tileStyle}
      aria-label={`${room.anchorName} 的直播画面`}
      onPointerMove={showControls}
      onPointerDown={showControls}
      onTouchStart={showControls}
      onFocusCapture={() => { setFocusWithin(true); setControlsVisible(true); }}
      onBlurCapture={(event) => {
        if (!event.currentTarget.contains(event.relatedTarget as Node | null)) setFocusWithin(false);
      }}
    >
      <RoomPlaybackSurface
        room={room}
        demoMode={demoMode}
        muted={!hasAudioFocus || globalMuted}
        globalDanmakuEnabled={globalDanmakuEnabled}
        danmakuSettings={danmakuSettings}
        tone={tone}
        onRetry={() => { void refreshStreamAvailability(room.roomId); }}
      />
      <div className="tile-topbar">
        <div className="tile-room-meta">
          <span className="live-pill"><span className={`status-dot ${room.online && room.status !== 'offline' ? 'status-dot-live' : 'status-dot-offline'}`} />{room.online && room.status !== 'offline' ? STATUS_LABELS[room.status] : '未开播'}</span>
          <span className="tile-category">{room.category}</span>
        </div>
        <div className="tile-menu-wrap">
          <button
            className="tiny-icon-button tile-more"
            type="button"
            aria-label={`${room.anchorName} 更多操作`}
            aria-expanded={menuOpen}
            title="更多操作"
            onClick={() => setMenuOpen((open) => !open)}
          >
            <MoreHorizontal size={16} />
          </button>
          {menuOpen ? (
            <div className="tile-menu" role="menu" aria-label={`${room.anchorName} 操作菜单`}>
              <div className="tile-menu-status" role="status">
                <strong>{actionSummary.playbackLabel}</strong>
                <span>{actionSummary.playbackDetail}</span>
                <span>{actionSummary.danmakuLabel}</span>
              </div>
              <button
                type="button"
                role="menuitem"
                onClick={() => {
                  setMenuOpen(false);
                  void refreshRoomMetadata(room.roomId);
                }}
              >
                <RotateCw size={14} />
                <span>刷新房间资料</span>
              </button>
              <button
                type="button"
                role="menuitem"
                onClick={() => {
                  setMenuOpen(false);
                  void refreshStreamAvailability(room.roomId);
                }}
              >
                <RotateCw size={14} />
                <span>重新检查播放源</span>
              </button>
              <button
                type="button"
                role="menuitem"
                onClick={() => {
                  setMenuOpen(false);
                  setPrimaryRoom(room.roomId);
                }}
              >
                <Crown size={14} />
                <span>{isPrimary ? '当前主画面' : '设为主画面'}</span>
              </button>
              <button
                type="button"
                role="menuitem"
                className="is-danger"
                onClick={() => {
                  setMenuOpen(false);
                  removeRoom(room.roomId);
                }}
              >
                <ShieldAlert size={14} />
                <span>移除房间</span>
              </button>
            </div>
          ) : null}
        </div>
      </div>
      <div className="tile-bottom-bar">
        <div className="tile-title-wrap">
          <strong>{room.anchorName}</strong>
          <span>{room.title}</span>
        </div>
        <div className="tile-actions">
          <button className={`tile-action-button ${isPrimary ? 'is-active' : ''}`} type="button" aria-label={isPrimary ? '当前主画面' : `设 ${room.anchorName} 为主画面`} title={isPrimary ? '当前主画面' : '设为主画面'} onClick={() => setPrimaryRoom(room.roomId)}><Crown size={15} /></button>
          <button
            className={`tile-action-button ${hasAudioFocus ? 'is-active is-audio' : ''}`}
            type="button"
            aria-label={presentation.audioDisabled ? '暂无可用音频' : hasAudioFocus ? '关闭声音焦点' : `播放 ${room.anchorName} 声音`}
            title={presentation.audioDisabled ? '暂无可用音频' : hasAudioFocus ? '关闭声音' : '播放声音'}
            disabled={presentation.audioDisabled}
            onClick={() => setAudioRoom(hasAudioFocus ? undefined : room.roomId)}
          >
            {hasAudioFocus ? <Volume2 size={15} /> : <VolumeX size={15} />}
          </button>
          {danmakuView.status.state === 'connecting' || danmakuView.status.state === 'reconnecting' ? (
            <span
              className="danmaku-status"
              role="status"
              aria-label={danmakuView.status.state === 'connecting' ? '弹幕连接中' : '弹幕重连中'}
              title={danmakuView.status.state === 'connecting' ? '弹幕连接中' : '弹幕重连中'}
            >
              <LoaderCircle className="spin" size={15} />
            </span>
          ) : null}
          {danmakuView.status.state === 'failed' ? (
            <button
              className="tile-action-button"
              type="button"
              aria-label="重试弹幕连接"
              title="重试弹幕连接"
              onClick={() => { void retryRoom(room.roomId); }}
            >
              <RotateCw size={15} />
            </button>
          ) : null}
          {danmakuView.status.state === 'platform-blocked' ? (
            <span
              className="danmaku-status is-blocked"
              role="status"
              aria-label="弹幕平台阻塞"
              title="弹幕平台阻塞"
            >
              <ShieldAlert size={15} />
            </span>
          ) : null}
          <button
            className={`tile-action-button ${room.danmakuEnabled ? 'is-active' : ''} ${danmakuView.status.state === 'connected' ? 'is-danmaku-connected' : ''}`}
            type="button"
            aria-label={room.danmakuEnabled ? '隐藏弹幕' : '显示弹幕'}
            title={`${room.danmakuEnabled ? '隐藏弹幕' : '显示弹幕'}${danmakuView.status.state === 'connected' ? '，弹幕已连接' : ''}`}
            onClick={() => toggleDanmaku(room.roomId)}
          >
            {room.danmakuEnabled ? <MessageCircle size={15} /> : <MessageCircleOff size={15} />}
          </button>
          <label className="quality-control">
            <span className="sr-only">{room.anchorName} 清晰度</span>
            <select
              value={selectedQuality}
              disabled={presentation.qualityDisabled}
              onChange={(event) => setQuality(room.roomId, event.target.value as RoomSession['quality'])}
            >
              {qualityOptions.map((option) => <option value={option.value} key={option.value}>{option.label}</option>)}
            </select>
          </label>
          <label className="volume-control">
            <Volume2 size={13} aria-hidden="true" />
            <span className="sr-only">{room.anchorName} 音量</span>
            <input
              type="range"
              min="0"
              max="1"
              step="0.05"
              value={room.volume}
              aria-label={`${room.anchorName} 音量`}
              onChange={(event) => setVolume(room.roomId, Number(event.target.value))}
            />
          </label>
        </div>
      </div>
    </article>
  );
}
