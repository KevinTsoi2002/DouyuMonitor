import { useEffect, useRef } from 'react';
import { DEFAULT_ROOM_VOLUME } from '../store/room-library';

export interface FlvPlayerLike {
  attachMediaElement(element: HTMLMediaElement): void;
  load(): void;
  play(): Promise<void> | void;
  on(event: string, listener: (...args: unknown[]) => void): void;
  off(event: string, listener: (...args: unknown[]) => void): void;
  unload(): void;
  detachMediaElement(): void;
  destroy(): void;
}

export interface FlvRuntime {
  isSupported(): boolean;
  errorEvent: string;
  createPlayer(
    source: { type: 'flv'; isLive: true; url: string; cors: true; withCredentials: false },
    config: Record<string, boolean | number>,
  ): FlvPlayerLike;
}

export interface AttachFlvPlayerOptions {
  runtime: FlvRuntime;
  video: HTMLVideoElement;
  url: string;
  onError: (code: string) => void;
  onPlaying?: () => void;
}

export function applyVideoAudioState(video: HTMLVideoElement, muted: boolean, volume: number): void {
  video.muted = muted;
  video.volume = Math.min(1, Math.max(0, Number.isFinite(volume) ? volume : DEFAULT_ROOM_VOLUME));
}

export function attachFlvPlayer({
  runtime,
  video,
  url,
  onError,
  onPlaying,
}: AttachFlvPlayerOptions): () => void {
  if (!runtime.isSupported()) {
    onError('MSE_UNSUPPORTED');
    return () => {};
  }

  const player = runtime.createPlayer(
    { type: 'flv', isLive: true, url, cors: true, withCredentials: false },
    {
      // Electron's packaged file:// renderer cannot execute mpegts.js's
      // blob-backed worker reliably (the worker fails before demuxing starts),
      // so keep demuxing and MSE on the renderer thread for a real first frame.
      enableWorker: false,
      enableWorkerForMSE: false,
      enableStashBuffer: true,
      stashInitialSize: 128 * 1024,
      lazyLoad: false,
      liveSync: true,
      liveSyncMaxLatency: 3,
      liveSyncTargetLatency: 1.5,
      autoCleanupSourceBuffer: true,
      autoCleanupMaxBackwardDuration: 30,
      autoCleanupMinBackwardDuration: 15,
    },
  );
  let active = true;
  let playbackReady = false;
  let failureReported = false;
  let firstFrameTimer: ReturnType<typeof setTimeout> | undefined;
  const reportError = (code: string) => {
    if (!active || failureReported) return;
    failureReported = true;
    if (firstFrameTimer) {
      clearTimeout(firstFrameTimer);
      firstFrameTimer = undefined;
    }
    onError(code);
  };
  const handleError = (errorType: unknown) => {
    reportError(typeof errorType === 'string' ? errorType : 'PLAYER_ERROR');
  };
  const handlePlaying = () => {
    if (!active || playbackReady) return;
    playbackReady = true;
    if (firstFrameTimer) {
      clearTimeout(firstFrameTimer);
      firstFrameTimer = undefined;
    }
    onPlaying?.();
  };

  player.on(runtime.errorEvent, handleError);
  // `playing` can fire once the media pipeline starts, before a decoded frame
  // reaches the surface. Recovery must wait for `loadeddata` so a black tile
  // is not incorrectly considered healthy.
  video.addEventListener?.('loadeddata', handlePlaying);
  player.attachMediaElement(video);
  player.load();
  firstFrameTimer = setTimeout(() => reportError('FIRST_FRAME_TIMEOUT'), 10_000);
  const playResult = player.play();
  if (playResult && typeof playResult.catch === 'function') {
    void playResult.catch(() => {
      reportError('PLAYBACK_START_FAILED');
    });
  }

  return () => {
    active = false;
    if (firstFrameTimer) clearTimeout(firstFrameTimer);
    player.off(runtime.errorEvent, handleError);
    video.removeEventListener?.('loadeddata', handlePlaying);
    player.unload();
    player.detachMediaElement();
    player.destroy();
  };
}

export function FlvVideo({
  url,
  muted,
  volume = DEFAULT_ROOM_VOLUME,
  onError,
  onPlaying,
}: {
  url: string;
  muted: boolean;
  volume?: number;
  onError: (code: string) => void;
  onPlaying?: () => void;
}) {
  const videoRef = useRef<HTMLVideoElement>(null);

  useEffect(() => {
    const video = videoRef.current;
    if (!video) return;
    applyVideoAudioState(video, muted, volume);
  }, [muted, volume]);

  useEffect(() => {
    const video = videoRef.current;
    if (!video) return undefined;

    let detach = () => {};
    let cancelled = false;
    void import('mpegts.js')
      .then(({ default: runtimeModule }) => {
        if (cancelled) return;
        detach = attachFlvPlayer({
          runtime: {
            isSupported: () => runtimeModule.isSupported(),
            errorEvent: runtimeModule.Events.ERROR,
            createPlayer: (source, config) => runtimeModule.createPlayer(source, config),
          },
          video,
          url,
          onError,
          onPlaying,
        });
      })
      .catch(() => onError('PLAYER_LOAD_FAILED'));

    return () => {
      cancelled = true;
      detach();
    };
  }, [onError, onPlaying, url]);

  return (
    <video
      ref={videoRef}
      className="live-video"
      autoPlay
      muted={muted}
      playsInline
    />
  );
}
