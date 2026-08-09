import {
  DouyuAdapterError,
  type DouyuAdapter,
  type StreamAvailability,
} from '../domain/douyu-adapter';
import type { StreamgetBridge } from '../main/streamget-bridge';

export function createStreamgetDouyuAdapter(
  baseAdapter: DouyuAdapter,
  bridge: StreamgetBridge,
): DouyuAdapter {
  return {
    search: (input) => baseAdapter.search(input),

    async getStreamAvailability(roomId): Promise<StreamAvailability> {
      const observed = await baseAdapter.getStreamAvailability(roomId);
      if (observed.kind === 'blocked' && observed.reason === 'ROOM_OFFLINE') {
        return observed;
      }

      let stream;
      try {
        stream = await bridge.resolve(roomId);
      } catch (error) {
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

      return {
        kind: 'available',
        roomId,
        variants: [{
          id: 'streamget-auto-flv',
          label: 'StreamGet FLV',
          quality: 'auto',
          playbackUrl: stream.flvUrl,
          container: 'flv',
        }],
        checkedAt: new Date().toISOString(),
      };
    },
  };
}
