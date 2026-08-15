import { AlertCircle, CircleSlash2, LoaderCircle } from 'lucide-react';
import { useCallback, useEffect, useReducer, useRef, useState } from 'react';
import {
  DEFAULT_DANMAKU_SETTINGS,
  type DanmakuSettings,
} from '../danmaku/danmaku-settings';
import type { RoomSession } from '../store/workspace-store';
import {
  createPlaybackRecoveryController,
  PLAYBACK_RECOVERY_MAX_ATTEMPTS,
  type PlaybackRecoveryController,
  type PlaybackRecoveryState,
} from '../playback-recovery';
import { getPlaybackPresentation, getRoomInitials } from '../ui-model';
import { DanmakuOverlay } from './DanmakuOverlay';
import { FlvVideo } from './FlvVideo';

interface RoomPlaybackSurfaceProps {
  room: RoomSession;
  demoMode: boolean;
  muted?: boolean;
  globalDanmakuEnabled?: boolean;
  danmakuSettings?: DanmakuSettings;
  onRetry: () => void;
  onRecoveryChange?: (state: PlaybackRecoveryState | undefined, errorCode?: string) => void;
  tone?: string;
}

interface PlayerErrorState {
  playbackUrl: string;
  code: string;
}

type PlayerErrorAction =
  | { type: 'report'; playbackUrl: string; code: string }
  | { type: 'clear' };

export function reducePlayerError(
  _state: PlayerErrorState | undefined,
  action: PlayerErrorAction,
): PlayerErrorState | undefined {
  return action.type === 'clear'
    ? undefined
    : { playbackUrl: action.playbackUrl, code: action.code };
}

export function getPlayerErrorForUrl(
  error: PlayerErrorState | undefined,
  playbackUrl: string | undefined,
): string | undefined {
  return error && error.playbackUrl === playbackUrl ? error.code : undefined;
}

export function RoomPlaybackSurface({
  room,
  demoMode,
  muted = true,
  globalDanmakuEnabled = true,
  danmakuSettings = DEFAULT_DANMAKU_SETTINGS,
  onRetry,
  onRecoveryChange,
  tone = 'coral',
}: RoomPlaybackSurfaceProps) {
  const variant = room.streamAvailability?.kind === 'available'
    ? room.streamAvailability.variants.find((item) => item.quality === room.quality)
      ?? room.streamAvailability.variants[0]
    : undefined;
  const playbackUrl = variant?.playbackUrl;
  const [playerError, dispatchPlayerError] = useReducer(reducePlayerError, undefined);
  const [recovery, setRecovery] = useState<PlaybackRecoveryState | undefined>();
  const previousPlaybackUrlRef = useRef(playbackUrl);
  const onRetryRef = useRef(onRetry);
  const onRecoveryChangeRef = useRef(onRecoveryChange);
  const playerErrorCodeRef = useRef<string | undefined>(undefined);
  onRetryRef.current = onRetry;
  onRecoveryChangeRef.current = onRecoveryChange;
  const recoveryControllerRef = useRef<PlaybackRecoveryController | undefined>(undefined);
  if (!recoveryControllerRef.current) {
    recoveryControllerRef.current = createPlaybackRecoveryController({
      onRetry: () => onRetryRef.current(),
      onStateChange: (state) => {
        setRecovery(state);
        onRecoveryChangeRef.current?.(state, playerErrorCodeRef.current);
      },
    });
  }
  const recoveryController = recoveryControllerRef.current;
  const resetRecovery = useCallback(() => {
    playerErrorCodeRef.current = undefined;
    recoveryController.markPlaying();
    dispatchPlayerError({ type: 'clear' });
  }, [recoveryController]);
  const retryNow = useCallback(() => {
    playerErrorCodeRef.current = undefined;
    recoveryController.retryNow();
    dispatchPlayerError({ type: 'clear' });
  }, [recoveryController]);
  const handlePlayerError = useCallback((code: string) => {
    if (playbackUrl) {
      playerErrorCodeRef.current = code;
      dispatchPlayerError({ type: 'report', playbackUrl, code });
      recoveryController.reportFailure();
    }
  }, [playbackUrl, recoveryController]);
  const handleRetry = useCallback(() => {
    retryNow();
  }, [retryNow]);
  const handlePlayerPlaying = useCallback(() => {
    resetRecovery();
  }, [resetRecovery]);
  const currentPlayerError = getPlayerErrorForUrl(playerError, playbackUrl);

  useEffect(() => {
    const previousPlaybackUrl = previousPlaybackUrlRef.current;
    previousPlaybackUrlRef.current = playbackUrl;
    if (playbackUrl && previousPlaybackUrl && playbackUrl !== previousPlaybackUrl) {
      resetRecovery();
    }
  }, [playbackUrl, resetRecovery]);

  useEffect(() => () => recoveryController.dispose(), [recoveryController]);

  if (demoMode && room.playbackAvailabilityStatus === 'available') {
    return (
      <div className={`signal-scene scene-${tone}`}>
        <div className="scene-block scene-block-one" />
        <div className="scene-block scene-block-two" />
        <div className="scene-grid-lines" aria-hidden="true" />
        <div className="signal-watermark">DOUYU / LIVE</div>
        <div className="signal-center-mark">
          <span>{getRoomInitials(room.anchorName)}</span>
          <small>模拟画面</small>
        </div>
        <DanmakuOverlay roomId={room.roomId} enabled={globalDanmakuEnabled && room.danmakuEnabled} settings={danmakuSettings} />
      </div>
    );
  }

  if (!demoMode
    && room.playbackAvailabilityStatus === 'available'
    && variant?.container === 'flv') {
    return (
      <div className="live-video-surface">
        <FlvVideo
          key={variant.playbackUrl}
          url={variant.playbackUrl}
          muted={muted}
          volume={room.volume}
          onError={handlePlayerError}
          onPlaying={handlePlayerPlaying}
        />
        {currentPlayerError ? (
          <div className="live-video-error" role="alert">
            {recovery?.exhausted ? <AlertCircle size={24} aria-hidden="true" /> : <LoaderCircle className="spin" size={24} aria-hidden="true" />}
            <strong>{recovery?.exhausted ? '自动恢复失败' : '正在恢复播放'}</strong>
            <span>
              {recovery?.exhausted
                ? '已达到自动重试上限，请手动重新获取直播流'
                : `将在 ${Math.ceil((recovery?.delayMs ?? 0) / 1000)} 秒后自动重试（${recovery?.attempt ?? 1}/${PLAYBACK_RECOVERY_MAX_ATTEMPTS}）`}
            </span>
            <button className="button playback-retry-button" type="button" onClick={handleRetry}>
              {'\u7acb\u5373\u91cd\u8bd5'}
            </button>
          </div>
        ) : null}
        <DanmakuOverlay roomId={room.roomId} enabled={globalDanmakuEnabled && room.danmakuEnabled} settings={danmakuSettings} />
      </div>
    );
  }

  const presentation = getPlaybackPresentation(room);
  const isOffline = !room.online || room.status === 'offline';
  const StateIcon = isOffline
    ? CircleSlash2
    : room.playbackAvailabilityStatus === 'checking'
    ? LoaderCircle
    : room.playbackAvailabilityStatus === 'error'
      ? AlertCircle
      : CircleSlash2;

  return (
    <div className="playback-state-surface" aria-live="polite">
      <StateIcon
        className={`playback-state-icon ${!isOffline && room.playbackAvailabilityStatus === 'checking' ? 'spin' : ''}`}
        size={27}
        aria-hidden="true"
      />
      <div className="playback-state-copy">
        <strong>{presentation.title}</strong>
        <span>{presentation.detail}</span>
      </div>
      {presentation.canRetry ? (
        <button className="button playback-retry-button" type="button" onClick={handleRetry}>
          重新检查
        </button>
      ) : null}
      <DanmakuOverlay roomId={room.roomId} enabled={globalDanmakuEnabled && room.danmakuEnabled} settings={danmakuSettings} />
    </div>
  );
}
