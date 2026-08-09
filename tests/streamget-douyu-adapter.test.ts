import { describe, expect, it, vi } from 'vitest';
import type { DouyuAdapter, StreamAvailability } from '../src/domain/douyu-adapter';
import { createStreamgetDouyuAdapter } from '../src/infrastructure/streamget-douyu-adapter';
import type { StreamgetBridge } from '../src/main/streamget-bridge';

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

describe('StreamGet Douyu adapter', () => {
  it('maps a live app-path FLV to an available variant', async () => {
    const bridge: StreamgetBridge = {
      resolve: vi.fn(async () => ({
        roomId: '63136',
        isLive: true,
        flvUrl: 'https://openflv-hw.douyucdn2.cn/live/63136_demo.flv?wsAuth=redacted',
      })),
    };
    const adapter = createStreamgetDouyuAdapter(onlineBaseAdapter(), bridge);

    await expect(adapter.getStreamAvailability('63136')).resolves.toEqual(expect.objectContaining({
      kind: 'available',
      roomId: '63136',
      variants: [expect.objectContaining({
        quality: 'auto',
        container: 'flv',
        playbackUrl: expect.stringContaining('openflv-hw.douyucdn2.cn'),
      })],
    }));
  });

  it('does not invoke StreamGet for an offline room', async () => {
    const bridge: StreamgetBridge = { resolve: vi.fn() };
    const adapter = createStreamgetDouyuAdapter({
      ...onlineBaseAdapter(),
      getStreamAvailability: async () => ({
        kind: 'blocked',
        roomId: '63136',
        reason: 'ROOM_OFFLINE',
        observedQualities: [],
        checkedAt: '2026-08-08T00:00:00.000Z',
      }),
    }, bridge);

    await expect(adapter.getStreamAvailability('63136')).resolves.toMatchObject({
      kind: 'blocked',
      reason: 'ROOM_OFFLINE',
    });
    expect(bridge.resolve).not.toHaveBeenCalled();
  });
});
