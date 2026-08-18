import { renderToStaticMarkup } from 'react-dom/server';
import { describe, expect, it, vi } from 'vitest';
import {
  FlvVideo,
  applyVideoAudioState,
  attachFlvPlayer,
  type FlvRuntime,
} from '../src/renderer/components/FlvVideo';

describe('FLV video', () => {
  it('applies bounded mute and volume state to the media element', () => {
    const video = { muted: false, volume: 0 } as HTMLVideoElement;

    applyVideoAudioState(video, true, 1.5);
    expect(video.muted).toBe(true);
    expect(video.volume).toBe(1);

    applyVideoAudioState(video, false, -1);
    expect(video.muted).toBe(false);
    expect(video.volume).toBe(0);
  });

  it('attaches, loads, plays, and destroys one live player', () => {
    const listeners = new Map<string, (...args: unknown[]) => void>();
    const player = {
      attachMediaElement: vi.fn(),
      load: vi.fn(),
      play: vi.fn(async () => undefined),
      on: vi.fn((event: string, listener: (...args: unknown[]) => void) => {
        listeners.set(event, listener);
      }),
      off: vi.fn(),
      unload: vi.fn(),
      detachMediaElement: vi.fn(),
      destroy: vi.fn(),
    };
    const runtime: FlvRuntime = {
      isSupported: () => true,
      errorEvent: 'error',
      createPlayer: vi.fn(() => player),
    };
    const onError = vi.fn();
    const video = {} as HTMLVideoElement;

    const detach = attachFlvPlayer({ runtime, video, url: 'https://cdn.example/live.flv', onError });

    expect(runtime.createPlayer).toHaveBeenCalledWith(
      expect.objectContaining({ type: 'flv', isLive: true }),
      expect.objectContaining({
        enableWorker: false,
        enableWorkerForMSE: false,
        enableStashBuffer: true,
        stashInitialSize: 128 * 1024,
        liveSync: true,
        liveSyncMaxLatency: 3,
        liveSyncTargetLatency: 1.5,
      }),
    );
    expect(player.attachMediaElement).toHaveBeenCalledWith(video);
    expect(player.load).toHaveBeenCalledOnce();
    expect(player.play).toHaveBeenCalledOnce();

    listeners.get('error')?.('NETWORK_ERROR', 'NETWORK_TIMEOUT');
    expect(onError).toHaveBeenCalledWith('NETWORK_ERROR');

    detach();
    expect(player.unload).toHaveBeenCalledOnce();
    expect(player.detachMediaElement).toHaveBeenCalledOnce();
    expect(player.destroy).toHaveBeenCalledOnce();
  });

  it('keeps MediaSource on the renderer thread for Electron multi-room playback', () => {
    const player = {
      attachMediaElement: vi.fn(),
      load: vi.fn(),
      play: vi.fn(),
      on: vi.fn(),
      off: vi.fn(),
      unload: vi.fn(),
      detachMediaElement: vi.fn(),
      destroy: vi.fn(),
    };
    const runtime: FlvRuntime = {
      isSupported: () => true,
      errorEvent: 'error',
      createPlayer: vi.fn(() => player),
    };

    const detach = attachFlvPlayer({
      runtime,
      video: {} as HTMLVideoElement,
      url: 'https://live.douyucdn.cn/live.flv',
      onError: vi.fn(),
    });

    expect(runtime.createPlayer).toHaveBeenCalledWith(
      expect.objectContaining({ type: 'flv', isLive: true }),
      expect.objectContaining({
        enableWorker: false,
        enableWorkerForMSE: false,
      }),
    );
    detach();
  });

  it('ignores player errors fired after cleanup', () => {
    const listeners = new Map<string, (...args: unknown[]) => void>();
    const player = {
      attachMediaElement: vi.fn(),
      load: vi.fn(),
      play: vi.fn(),
      on: vi.fn((event: string, listener: (...args: unknown[]) => void) => {
        listeners.set(event, listener);
      }),
      off: vi.fn(),
      unload: vi.fn(),
      detachMediaElement: vi.fn(),
      destroy: vi.fn(),
    };
    const runtime: FlvRuntime = {
      isSupported: () => true,
      errorEvent: 'error',
      createPlayer: vi.fn(() => player),
    };
    const onError = vi.fn();

    const detach = attachFlvPlayer({
      runtime,
      video: {} as HTMLVideoElement,
      url: 'https://live.douyucdn.cn/live.flv',
      onError,
    });
    const lateError = listeners.get('error');
    detach();
    lateError?.('NETWORK_ERROR');

    expect(onError).not.toHaveBeenCalled();
  });

  it('reports media playback recovery only after the first decoded frame is available', () => {
    const videoListeners = new Map<string, () => void>();
    const player = {
      attachMediaElement: vi.fn(),
      load: vi.fn(),
      play: vi.fn(),
      on: vi.fn(),
      off: vi.fn(),
      unload: vi.fn(),
      detachMediaElement: vi.fn(),
      destroy: vi.fn(),
    };
    const runtime: FlvRuntime = {
      isSupported: () => true,
      errorEvent: 'error',
      createPlayer: vi.fn(() => player),
    };
    const onPlaying = vi.fn();
    const video = {
      addEventListener: vi.fn((event: string, listener: () => void) => {
        videoListeners.set(event, listener);
      }),
      removeEventListener: vi.fn(),
    } as unknown as HTMLVideoElement;

    const detach = attachFlvPlayer({
      runtime,
      video,
      url: 'https://live.douyucdn.cn/live.flv',
      onError: vi.fn(),
      onPlaying,
    });

    videoListeners.get('playing')?.();
    expect(onPlaying).not.toHaveBeenCalled();

    videoListeners.get('loadeddata')?.();
    expect(onPlaying).toHaveBeenCalledOnce();

    detach();
    expect(video.removeEventListener).toHaveBeenCalledWith('loadeddata', expect.any(Function));
  });

  it('reports a failure when no decoded frame arrives after player startup', () => {
    vi.useFakeTimers();
    const player = {
      attachMediaElement: vi.fn(),
      load: vi.fn(),
      play: vi.fn(),
      on: vi.fn(),
      off: vi.fn(),
      unload: vi.fn(),
      detachMediaElement: vi.fn(),
      destroy: vi.fn(),
    };
    const runtime: FlvRuntime = {
      isSupported: () => true,
      errorEvent: 'error',
      createPlayer: vi.fn(() => player),
    };
    const onError = vi.fn();

    const detach = attachFlvPlayer({
      runtime,
      video: {} as HTMLVideoElement,
      url: 'https://live.douyucdn.cn/live.flv',
      onError,
    });

    vi.advanceTimersByTime(10_000);
    expect(onError).toHaveBeenCalledWith('FIRST_FRAME_TIMEOUT');

    detach();
    vi.useRealTimers();
  });

  it('ignores a rejected play promise after cleanup', async () => {
    let rejectPlay!: (reason?: unknown) => void;
    const playPromise = new Promise<void>((_resolve, reject) => {
      rejectPlay = reject;
    });
    const player = {
      attachMediaElement: vi.fn(),
      load: vi.fn(),
      play: vi.fn(() => playPromise),
      on: vi.fn(),
      off: vi.fn(),
      unload: vi.fn(),
      detachMediaElement: vi.fn(),
      destroy: vi.fn(),
    };
    const runtime: FlvRuntime = {
      isSupported: () => true,
      errorEvent: 'error',
      createPlayer: vi.fn(() => player),
    };
    const onError = vi.fn();

    const detach = attachFlvPlayer({
      runtime,
      video: {} as HTMLVideoElement,
      url: 'https://live.douyucdn.cn/live.flv',
      onError,
    });
    detach();
    rejectPlay(new Error('late rejection'));
    await playPromise.catch(() => undefined);

    expect(onError).not.toHaveBeenCalled();
  });

  it('renders a video element without exposing the stream URL as an attribute', () => {
    const html = renderToStaticMarkup(
      <FlvVideo
        url="https://openflv-hw.douyucdn2.cn/live/demo.flv?wsAuth=secret"
        muted
        onError={() => {}}
      />,
    );

    expect(html).toContain('<video');
    expect(html).toContain('class="live-video"');
    expect(html).not.toContain('wsAuth');
    expect(html).not.toContain('secret');
  });
});
