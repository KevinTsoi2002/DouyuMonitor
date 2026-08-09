import { renderToStaticMarkup } from 'react-dom/server';
import { describe, expect, it } from 'vitest';
import * as playbackSurfaceModule from '../src/renderer/components/RoomPlaybackSurface';
import { RoomPlaybackSurface } from '../src/renderer/components/RoomPlaybackSurface';
import type { RoomSession } from '../src/renderer/store/workspace-store';

describe('RoomPlaybackSurface live playback', () => {
  it('clears a player error when retrying the same playback URL', () => {
    const reducePlayerError = Reflect.get(
      playbackSurfaceModule,
      'reducePlayerError',
    ) as undefined | ((
      error: { playbackUrl: string; code: string } | undefined,
      action: { type: 'clear' },
    ) => { playbackUrl: string; code: string } | undefined);

    expect(typeof reducePlayerError).toBe('function');
    expect(reducePlayerError?.({
      playbackUrl: 'https://live.douyucdn.cn/live.flv',
      code: 'NETWORK_ERROR',
    }, { type: 'clear' })).toBeUndefined();
  });

  it('scopes player errors to the playback URL that produced them', () => {
    const getPlayerErrorForUrl = Reflect.get(
      playbackSurfaceModule,
      'getPlayerErrorForUrl',
    ) as undefined | ((
      error: { playbackUrl: string; code: string } | undefined,
      playbackUrl: string | undefined,
    ) => string | undefined);

    expect(typeof getPlayerErrorForUrl).toBe('function');
    expect(getPlayerErrorForUrl?.({
      playbackUrl: 'https://live.douyucdn.cn/first.flv',
      code: 'NETWORK_ERROR',
    }, 'https://live.douyucdn.cn/second.flv')).toBeUndefined();
  });

  it('renders a real FLV video for an available production stream', () => {
    const room: RoomSession = {
      roomId: '63136',
      anchorName: 'Live Room',
      title: 'Live title',
      category: 'Game',
      online: true,
      viewerLabel: '1',
      status: 'playing',
      quality: 'auto',
      volume: 1,
      danmakuEnabled: true,
      playbackAvailabilityStatus: 'available',
      streamAvailability: {
        kind: 'available',
        roomId: '63136',
        variants: [{
          id: 'streamget-auto-flv',
          label: 'StreamGet FLV',
          quality: 'auto',
          playbackUrl: 'https://openflv-hw.douyucdn2.cn/live/demo.flv?wsAuth=secret',
          container: 'flv',
        }],
        checkedAt: '2026-08-08T00:00:00.000Z',
      },
    };

    const html = renderToStaticMarkup(
      <RoomPlaybackSurface
        room={room}
        demoMode={false}
        muted
        onRetry={() => {}}
      />,
    );

    expect(html).toContain('<video');
    expect(html).toContain('class="live-video"');
    expect(html).toContain('class="danmaku-overlay"');
    expect(html).not.toContain('wsAuth');
    expect(html).not.toContain('secret');
  });
});
