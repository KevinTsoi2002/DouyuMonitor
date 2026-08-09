import { AlertCircle, CircleSlash2, LoaderCircle } from 'lucide-react';
import { useCallback, useReducer } from 'react';
import {
  DEFAULT_DANMAKU_SETTINGS,
  type DanmakuSettings,
} from '../danmaku/danmaku-settings';
import type { RoomSession } from '../store/workspace-store';
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
  tone = 'coral',
}: RoomPlaybackSurfaceProps) {
  const variant = room.streamAvailability?.kind === 'available'
    ? room.streamAvailability.variants.find((item) => item.quality === room.quality)
      ?? room.streamAvailability.variants[0]
    : undefined;
  const playbackUrl = variant?.playbackUrl;
  const [playerError, dispatchPlayerError] = useReducer(reducePlayerError, undefined);
  const handlePlayerError = useCallback((code: string) => {
    if (playbackUrl) {
      dispatchPlayerError({ type: 'report', playbackUrl, code });
    }
  }, [playbackUrl]);
  const handleRetry = useCallback(() => {
    dispatchPlayerError({ type: 'clear' });
    onRetry();
  }, [onRetry]);
  const currentPlayerError = getPlayerErrorForUrl(playerError, playbackUrl);

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
        />
        {currentPlayerError ? (
          <div className="live-video-error" role="alert">
            <AlertCircle size={24} aria-hidden="true" />
            <strong>{'\u64ad\u653e\u5931\u8d25'}</strong>
            <span>{'\u8bf7\u91cd\u65b0\u83b7\u53d6\u76f4\u64ad\u6d41'}</span>
            <button className="button playback-retry-button" type="button" onClick={handleRetry}>
              {'\u91cd\u8bd5'}
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
        <button className="button playback-retry-button" type="button" onClick={onRetry}>
          重新检查
        </button>
      ) : null}
      <DanmakuOverlay roomId={room.roomId} enabled={globalDanmakuEnabled && room.danmakuEnabled} settings={danmakuSettings} />
    </div>
  );
}
