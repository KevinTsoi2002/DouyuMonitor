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
}: AttachFlvPlayerOptions): () => void {
  if (!runtime.isSupported()) {
    onError('MSE_UNSUPPORTED');
    return () => {};
  }

  const player = runtime.createPlayer(
    { type: 'flv', isLive: true, url, cors: true, withCredentials: false },
    {
      enableStashBuffer: false,
      lazyLoad: false,
      liveSync: true,
      liveSyncMaxLatency: 1.5,
      liveSyncTargetLatency: 0.8,
      autoCleanupSourceBuffer: true,
      autoCleanupMaxBackwardDuration: 30,
      autoCleanupMinBackwardDuration: 15,
    },
  );
  let active = true;
  const handleError = (errorType: unknown) => {
    if (active) {
      onError(typeof errorType === 'string' ? errorType : 'PLAYER_ERROR');
    }
  };

  player.on(runtime.errorEvent, handleError);
  player.attachMediaElement(video);
  player.load();
  const playResult = player.play();
  if (playResult && typeof playResult.catch === 'function') {
    void playResult.catch(() => {
      if (active) onError('PLAYBACK_START_FAILED');
    });
  }

  return () => {
    active = false;
    player.off(runtime.errorEvent, handleError);
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
}: {
  url: string;
  muted: boolean;
  volume?: number;
  onError: (code: string) => void;
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
        });
      })
      .catch(() => onError('PLAYER_LOAD_FAILED'));

    return () => {
      cancelled = true;
      detach();
    };
  }, [onError, url]);

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
