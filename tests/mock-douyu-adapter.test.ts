import { describe, expect, it } from 'vitest';
import { createMockDouyuAdapter } from '../src/infrastructure/mock-douyu-adapter';

describe('createMockDouyuAdapter', () => {
  it('creates a deterministic candidate for a numeric room id', async () => {
    const adapter = createMockDouyuAdapter();

    await expect(adapter.search({ type: 'room-id', value: '2048' })).resolves.toEqual([
      expect.objectContaining({ roomId: '2048', anchorName: '主播 2048' }),
    ]);
  });

  it('finds known rooms by a partial anchor name', async () => {
    const adapter = createMockDouyuAdapter();

    const results = await adapter.search({ type: 'anchor-name', value: '星河' });

    expect(results).toHaveLength(1);
    expect(results[0]).toEqual(expect.objectContaining({ roomId: '63136', anchorName: '星河' }));
  });

  it('returns no candidates for an unknown anchor', async () => {
    const adapter = createMockDouyuAdapter();

    await expect(
      adapter.search({ type: 'anchor-name', value: '不存在的主播' }),
    ).resolves.toEqual([]);
  });

  it('provides explicit demo-only stream variants', async () => {
    const adapter = createMockDouyuAdapter();

    const availability = await adapter.getStreamAvailability('63136');

    expect(availability).toEqual(expect.objectContaining({
      kind: 'available',
      roomId: '63136',
    }));
    if (availability.kind !== 'available') {
      throw new Error('expected available demo stream');
    }
    expect(availability.variants.map((variant) => variant.quality)).toEqual([
      'auto',
      'original',
      'super',
      'high',
      'standard',
    ]);
    expect(
      availability.variants.every((variant) => variant.playbackUrl.startsWith('mock:')),
    ).toBe(true);
  });
});
