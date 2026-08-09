import { describe, expect, it, vi } from 'vitest';
import { DouyuAdapterError, type DouyuAdapter } from '../src/domain/douyu-adapter';
import { createDouyuHttpAdapter } from '../src/infrastructure/douyu-http-adapter';
import betardFixture from './fixtures/douyu-betard-api.json';
import roomFixture from './fixtures/douyu-room-api.json';
import searchFixture from './fixtures/douyu-search-api.json';

describe('createDouyuHttpAdapter room lookup', () => {
  it('maps a public room response to a room candidate', async () => {
    const fetchRoom = vi.fn(async () => Response.json(roomFixture));
    const adapter = createDouyuHttpAdapter({ fetch: fetchRoom });

    await expect(adapter.search({ type: 'room-id', value: '63136' })).resolves.toEqual([
      {
        roomId: '63136',
        anchorName: '示例主播',
        avatarUrl: 'https://apic.douyucdn.cn/upload/avatar/example_big.jpg',
        title: '示例直播间',
        category: 'CS2',
        online: true,
        viewerLabel: '18.6 万',
      },
    ]);
    expect(fetchRoom).toHaveBeenCalledWith(
      'https://open.douyucdn.cn/api/RoomApi/room/63136',
      expect.objectContaining({ headers: { Accept: 'application/json' } }),
    );
  });

  it('omits unsafe avatar URLs from room candidates', async () => {
    const adapter = createDouyuHttpAdapter({
      fetch: async () => Response.json({
        ...roomFixture,
        data: { ...roomFixture.data, avatar: 'javascript:alert(1)' },
      }),
    });

    const [candidate] = await adapter.search({ type: 'room-id', value: '63136' });

    expect(candidate).not.toHaveProperty('avatarUrl');
  });

  it('reports a missing room with a stable adapter error', async () => {
    const adapter = createDouyuHttpAdapter({
      fetch: async () => Response.json({ error: 101, data: null }),
    });

    await expect(adapter.search({ type: 'room-id', value: '404' })).rejects.toMatchObject({
      code: 'ROOM_NOT_FOUND',
    });
  });

  it('reports a changed response contract without leaking the response', async () => {
    const adapter = createDouyuHttpAdapter({
      fetch: async () => Response.json({ error: 0, data: { token: 'secret-token' } }),
    });

    const error = await adapter
      .search({ type: 'room-id', value: '63136' })
      .catch((reason: unknown) => reason);

    expect(error).toBeInstanceOf(DouyuAdapterError);
    expect(error).toMatchObject({ code: 'PROTOCOL_CHANGED' });
    expect(String(error)).not.toContain('secret-token');
  });

  it('maps non-success HTTP responses to a retryable network error', async () => {
    const adapter = createDouyuHttpAdapter({
      fetch: async () => new Response('upstream unavailable', { status: 503 }),
    });

    await expect(adapter.search({ type: 'room-id', value: '63136' })).rejects.toMatchObject({
      code: 'NETWORK_UNAVAILABLE',
    });
  });

  it('maps rejected fetch calls without leaking transport details', async () => {
    const adapter = createDouyuHttpAdapter({
      fetch: async () => {
        throw new Error('connect failed token=secret-token');
      },
    });

    const error = await adapter
      .search({ type: 'room-id', value: '63136' })
      .catch((reason: unknown) => reason);

    expect(error).toBeInstanceOf(DouyuAdapterError);
    expect(error).toMatchObject({ code: 'NETWORK_UNAVAILABLE' });
    expect(String(error)).not.toContain('secret-token');
  });
});

describe('createDouyuHttpAdapter anchor search', () => {
  it('maps, encodes, and deduplicates public anchor search results', async () => {
    const searchFetch = vi.fn(async (_url: string, _init?: RequestInit) =>
      Response.json(searchFixture),
    );
    const adapter = createDouyuHttpAdapter({ fetch: searchFetch });

    const result = await adapter.search({ type: 'anchor-name', value: '示例 主播' });

    const requestUrl = new URL(searchFetch.mock.calls[0][0]);
    expect(requestUrl.origin + requestUrl.pathname).toBe(
      'https://www.douyu.com/japi/search/api/searchShow',
    );
    expect(requestUrl.searchParams.get('kw')).toBe('示例 主播');
    expect(requestUrl.searchParams.get('page')).toBe('1');
    expect(requestUrl.searchParams.get('pageSize')).toBe('20');
    expect(result).toEqual([
      {
        roomId: '6846643',
        anchorName: '示例主播',
        avatarUrl: 'https://apic.douyucdn.cn/upload/avatar/search_big.jpg',
        title: '示例直播间',
        category: '经典单机',
        online: true,
        viewerLabel: '18.6 万',
      },
    ]);
  });

  it('returns an empty list when the public search has no candidates', async () => {
    const adapter = createDouyuHttpAdapter({
      fetch: async () => Response.json({ error: 0, data: { relateShow: [] } }),
    });

    await expect(adapter.search({ type: 'anchor-name', value: '无人直播' })).resolves.toEqual(
      [],
    );
  });

  it('reports a changed anchor-search response contract', async () => {
    const adapter = createDouyuHttpAdapter({
      fetch: async () => Response.json({ error: 0, data: { relateShow: 'secret-token' } }),
    });

    const error = await adapter
      .search({ type: 'anchor-name', value: '示例主播' })
      .catch((reason: unknown) => reason);

    expect(error).toBeInstanceOf(DouyuAdapterError);
    expect(error).toMatchObject({ code: 'PROTOCOL_CHANGED' });
    expect(String(error)).not.toContain('secret-token');
  });
});

describe('createDouyuHttpAdapter availability', () => {
  const now = () => new Date('2026-08-07T00:00:00.000Z');

  function adapterFor(payload: unknown): DouyuAdapter {
    return createDouyuHttpAdapter({
      fetch: async () => Response.json(payload),
      now,
    });
  }

  it('reports signed-only qualities without returning playback URLs', async () => {
    const fetchBetard = vi.fn(async (_url: string, _init?: RequestInit) =>
      Response.json(betardFixture),
    );
    const adapter = createDouyuHttpAdapter({ fetch: fetchBetard, now });

    await expect(adapter.getStreamAvailability('63136')).resolves.toEqual({
      kind: 'blocked',
      roomId: '63136',
      reason: 'SIGNATURE_REQUIRED',
      observedQualities: [
        { id: 'douyu-0', label: '蓝光10M', providerType: 0 },
        { id: 'douyu-2', label: '超清', providerType: 2 },
      ],
      checkedAt: '2026-08-07T00:00:00.000Z',
    });
    expect(fetchBetard.mock.calls[0][0]).toBe('https://www.douyu.com/betard/63136');
    expect(fetchBetard.mock.calls.flat().join(' ')).not.toContain('getH5Play');
  });

  it('reports an offline room as blocked', async () => {
    await expect(adapterFor({
      room: { room_id: 63136, show_status: 2, multirates: [] },
    }).getStreamAvailability('63136')).resolves.toMatchObject({
      kind: 'blocked',
      reason: 'ROOM_OFFLINE',
      observedQualities: [],
    });
  });

  it('reports an online room without qualities as having no public source', async () => {
    await expect(adapterFor({
      room: { room_id: 63136, show_status: 1, multirates: [] },
    }).getStreamAvailability('63136')).resolves.toMatchObject({
      kind: 'blocked',
      reason: 'NO_PUBLIC_SOURCE',
      observedQualities: [],
    });
  });

  it('rejects a changed response contract', async () => {
    await expect(adapterFor({ room: { show_status: 1 } })
      .getStreamAvailability('63136')).rejects.toMatchObject({
      code: 'PROTOCOL_CHANGED',
    });
  });

  it('maps availability transport failures to a stable network error', async () => {
    const adapter = createDouyuHttpAdapter({
      fetch: async () => new Response('unavailable', { status: 503 }),
      now,
    });

    await expect(adapter.getStreamAvailability('63136')).rejects.toMatchObject({
      code: 'NETWORK_UNAVAILABLE',
    });
  });
});
