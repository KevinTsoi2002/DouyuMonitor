import {
  DouyuAdapterError,
  type DouyuAdapter,
  type StreamAvailability,
  type StreamQuality,
  type StreamRequestQuality,
} from '../domain/douyu-adapter';
import type { StreamgetRawResult, StreamgetBridge } from '../main/streamget-bridge';
import type { StreamgetResolutionQueue } from '../main/streamget-resolution-queue';
import type { StreamProxyManager } from '../main/stream-proxy-manager';

const VARIANT_QUALITY: Record<StreamRequestQuality, StreamQuality> = {
  auto: 'auto',
  original: 'original',
  super: 'super',
  high: 'high',
  standard: 'standard',
  '720p': 'high',
};

const QUALITY_LABEL: Record<StreamRequestQuality, string> = {
  auto: '自动',
  original: '原画',
  super: '超清',
  high: '高清',
  standard: '标清',
  '720p': '720p',
};

export function createStreamgetDouyuAdapter(
  baseAdapter: DouyuAdapter,
  resolver: Pick<StreamgetResolutionQueue, 'resolve'> | Pick<StreamgetBridge, 'resolve'>,
  proxyManager: StreamProxyManager = {
    register: async () => { throw new Error('Local stream proxy is not configured'); },
    release: async () => undefined,
    closeAll: async () => undefined,
  },
): DouyuAdapter {
  return {
    search: (input) => baseAdapter.search(input),

    async getStreamAvailability(
      roomId,
      quality: StreamRequestQuality = 'auto',
    ): Promise<StreamAvailability> {
      const observed = await baseAdapter.getStreamAvailability(roomId, quality);
      if (observed.kind === 'blocked' && observed.reason === 'ROOM_OFFLINE') {
        return observed;
      }

      let stream;
      try {
        stream = await resolver.resolve(roomId, quality);
      } catch {
        throw new DouyuAdapterError('STREAMGET_UNAVAILABLE', 'StreamGet resolver failed');
      }

      if (!stream.isLive || !stream.flvUrl) {
        return {
          kind: 'blocked',
          roomId,
          reason: 'ROOM_OFFLINE',
          observedQualities: observed.kind === 'blocked' ? observed.observedQualities : [],
          checkedAt: new Date().toISOString(),
        };
      }

      let playbackUrl: string;
      try {
        playbackUrl = await proxyManager.register(roomId, stream.flvUrl);
      } catch {
        throw new DouyuAdapterError('LOCAL_STREAM_PROXY_FAILED', 'Local stream proxy failed');
      }

      const resolvedQuality = stream.resolvedQuality ?? quality;
      return {
        kind: 'available',
        roomId,
        variants: [{
          id: `streamget-${resolvedQuality}-flv`,
          label: QUALITY_LABEL[resolvedQuality],
          quality: VARIANT_QUALITY[resolvedQuality],
          playbackUrl,
          container: 'flv',
        }],
        checkedAt: new Date().toISOString(),
      };
    },

    async releaseStream(roomId) {
      await proxyManager.release(roomId);
    },
  };
}
