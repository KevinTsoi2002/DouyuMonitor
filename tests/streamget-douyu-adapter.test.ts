import { describe, expect, it, vi } from 'vitest';
import type { DouyuAdapter, StreamAvailability } from '../src/domain/douyu-adapter';
import { createStreamgetDouyuAdapter } from '../src/infrastructure/streamget-douyu-adapter';
import type { StreamgetRawResult } from '../src/main/streamget-bridge';
import type { StreamgetResolutionQueue } from '../src/main/streamget-resolution-queue';
import type { StreamProxyManager } from '../src/main/stream-proxy-manager';

function onlineBaseAdapter(): DouyuAdapter {
  const availability: StreamAvailability = {
    kind: 'blocked',
    roomId: '63136',
    reason: 'SIGNATURE_REQUIRED',
    observedQualities: [{ id: 'douyu-0', label: 'Original', providerType: 0 }],
    checkedAt: '2026-08-08T00:00:00.000Z',
  };
  return {
    search: async () => [],
    getStreamAvailability: async () => availability,
  };
}

function createProxy(overrides: Partial<StreamProxyManager> = {}): StreamProxyManager {
  return {
    register: vi.fn(async () => 'http://127.0.0.1:41001/stream/token.flv'),
    release: vi.fn(async () => undefined),
    closeAll: vi.fn(async () => undefined),
    ...overrides,
  };
}

function createResolver(result: StreamgetRawResult = {
  roomId: '63136',
  isLive: true,
  flvUrl: 'https://openflv-hw.douyucdn2.cn/live/63136_demo.flv?wsAuth=redacted',
  resolvedQuality: '720p' as const,
  source: 'web-h5' as const,
}): StreamgetResolutionQueue {
  return {
    resolve: vi.fn(async () => result),
    cancel: vi.fn(),
    cancelAll: vi.fn(),
  };
}

function deferredResult<T = StreamgetRawResult>() {
  let resolve!: (value: T) => void;
  const promise = new Promise<T>((resolvePromise) => { resolve = resolvePromise; });
  return { promise, resolve };
}

describe('StreamGet Douyu adapter', () => {
  it('maps a live H5 FLV to a local 720p variant', async () => {
    const resolver = createResolver();
    const proxy = createProxy();
    const adapter = createStreamgetDouyuAdapter(onlineBaseAdapter(), resolver, proxy);

    const availability = await adapter.getStreamAvailability('63136', '720p');

    expect(resolver.resolve).toHaveBeenCalledWith('63136', '720p');
    expect(proxy.register).toHaveBeenCalledWith(
      '63136',
      'https://openflv-hw.douyucdn2.cn/live/63136_demo.flv?wsAuth=redacted',
    );
    expect(availability).toMatchObject({
      kind: 'available',
      roomId: '63136',
      variants: [{
        quality: 'high',
        label: '720p',
        container: 'flv',
        playbackUrl: 'http://127.0.0.1:41001/stream/token.flv',
      }],
    });
    expect(JSON.stringify(availability)).not.toContain('wsAuth');
  });

  it('labels an App fallback as original without exposing its upstream URL', async () => {
    const resolver = createResolver({
      roomId: '63136',
      isLive: true,
      flvUrl: 'https://live.douyucdn.cn/live/63136_app.flv?token=redacted',
      resolvedQuality: 'original',
      source: 'app-fallback',
    });
    const proxy = createProxy();
    const adapter = createStreamgetDouyuAdapter(onlineBaseAdapter(), resolver, proxy);

    await expect(adapter.getStreamAvailability('63136', '720p')).resolves.toMatchObject({
      variants: [{ quality: 'original', label: '原画' }],
    });
    expect(JSON.stringify(await adapter.getStreamAvailability('63136', '720p')))
      .not.toContain('token=redacted');
  });

  it('does not invoke resolution or proxy for an offline room', async () => {
    const resolver = createResolver();
    const proxy = createProxy();
    const adapter = createStreamgetDouyuAdapter({
      ...onlineBaseAdapter(),
      getStreamAvailability: async () => ({
        kind: 'blocked',
        roomId: '63136',
        reason: 'ROOM_OFFLINE',
        observedQualities: [],
        checkedAt: '2026-08-08T00:00:00.000Z',
      }),
    }, resolver, proxy);

    await expect(adapter.getStreamAvailability('63136', '720p')).resolves.toMatchObject({
      kind: 'blocked',
      reason: 'ROOM_OFFLINE',
    });
    expect(resolver.resolve).not.toHaveBeenCalled();
    expect(proxy.register).not.toHaveBeenCalled();
  });

  it('releases an existing proxy when StreamGet confirms the room is offline', async () => {
    const resolver = createResolver({ roomId: '63136', isLive: false });
    const proxy = createProxy();
    const adapter = createStreamgetDouyuAdapter(onlineBaseAdapter(), resolver, proxy);

    await expect(adapter.getStreamAvailability('63136', 'original')).resolves.toMatchObject({
      kind: 'blocked',
      reason: 'ROOM_OFFLINE',
    });
    expect(proxy.release).toHaveBeenCalledWith('63136');
  });

  it('maps resolver and proxy failures to safe domain errors', async () => {
    const resolver = createResolver();
    vi.mocked(resolver.resolve).mockRejectedValueOnce(new Error('sidecar secret'));
    const adapter = createStreamgetDouyuAdapter(onlineBaseAdapter(), resolver, createProxy());
    await expect(adapter.getStreamAvailability('63136', 'original'))
      .rejects.toMatchObject({ code: 'STREAMGET_UNAVAILABLE' });

    const proxy = createProxy({
      register: vi.fn(async () => { throw new Error('bind secret'); }),
    });
    const second = createStreamgetDouyuAdapter(onlineBaseAdapter(), createResolver(), proxy);
    await expect(second.getStreamAvailability('63136', 'original'))
      .rejects.toMatchObject({ code: 'LOCAL_STREAM_PROXY_FAILED' });
  });

  it('releases the room proxy through the adapter lifecycle', async () => {
    const proxy = createProxy();
    const adapter = createStreamgetDouyuAdapter(onlineBaseAdapter(), createResolver(), proxy);

    await adapter.releaseStream?.('63136');

    expect(proxy.release).toHaveBeenCalledWith('63136');
  });

  it('does not publish a late older quality over a newer request', async () => {
    const oldRequest = deferredResult();
    const resolver = createResolver();
    vi.mocked(resolver.resolve)
      .mockReturnValueOnce(oldRequest.promise)
      .mockResolvedValueOnce({
        roomId: '63136',
        isLive: true,
        flvUrl: 'https://live.douyucdn.cn/live/new.flv',
        resolvedQuality: 'original',
        source: 'web-h5',
      });
    const proxy = createProxy();
    const adapter = createStreamgetDouyuAdapter(onlineBaseAdapter(), resolver, proxy);

    const older = adapter.getStreamAvailability('63136', '720p');
    await adapter.getStreamAvailability('63136', 'original');
    oldRequest.resolve({
      roomId: '63136',
      isLive: true,
      flvUrl: 'https://live.douyucdn.cn/live/old.flv',
      resolvedQuality: '720p',
      source: 'web-h5',
    });
    await older.catch(() => undefined);

    expect(proxy.register).toHaveBeenCalledTimes(1);
    expect(proxy.register).toHaveBeenCalledWith('63136', expect.stringContaining('/new.flv'));
  });

  it('does not recreate a proxy when an active resolution finishes after release', async () => {
    const pending = deferredResult();
    const resolver = createResolver();
    vi.mocked(resolver.resolve).mockReturnValueOnce(pending.promise);
    const proxy = createProxy();
    const adapter = createStreamgetDouyuAdapter(onlineBaseAdapter(), resolver, proxy);

    const availability = adapter.getStreamAvailability('63136', 'original');
    await adapter.releaseStream?.('63136');
    pending.resolve({
      roomId: '63136',
      isLive: true,
      flvUrl: 'https://live.douyucdn.cn/live/late.flv',
      resolvedQuality: 'original',
      source: 'web-h5',
    });
    await availability.catch(() => undefined);

    expect(proxy.register).not.toHaveBeenCalled();
    expect(proxy.release).toHaveBeenCalledWith('63136');
  });

  it('does not let an older base-offline result release a newer proxy', async () => {
    const olderBase = deferredResult<StreamAvailability>();
    const baseGetAvailability = vi.fn()
      .mockReturnValueOnce(olderBase.promise)
      .mockResolvedValueOnce({
        kind: 'blocked',
        roomId: '63136',
        reason: 'SIGNATURE_REQUIRED',
        observedQualities: [],
        checkedAt: '2026-08-08T00:00:00.000Z',
      });
    const proxy = createProxy();
    const adapter = createStreamgetDouyuAdapter({
      ...onlineBaseAdapter(),
      getStreamAvailability: baseGetAvailability,
    }, createResolver(), proxy);

    const older = adapter.getStreamAvailability('63136', '720p');
    await adapter.getStreamAvailability('63136', 'original');
    olderBase.resolve({
      kind: 'blocked',
      roomId: '63136',
      reason: 'ROOM_OFFLINE',
      observedQualities: [],
      checkedAt: '2026-08-08T00:00:01.000Z',
    });
    await older.catch(() => undefined);

    expect(proxy.register).toHaveBeenCalledTimes(1);
    expect(proxy.release).not.toHaveBeenCalled();
  });
});
