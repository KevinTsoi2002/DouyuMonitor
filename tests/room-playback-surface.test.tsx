import { renderToStaticMarkup } from 'react-dom/server';
import { describe, expect, it } from 'vitest';
import { RoomPlaybackSurface } from '../src/renderer/components/RoomPlaybackSurface';
import { getRoomMuted } from '../src/renderer/components/RoomTile';
import { getDanmakuMessages } from '../src/renderer/data/mock-danmaku';
import type { RoomSession } from '../src/renderer/store/workspace-store';

const baseRoom: RoomSession = {
  roomId: '63136',
  anchorName: '示例主播',
  title: '示例直播间',
  category: 'CS2',
  online: true,
  viewerLabel: '1 万',
  status: 'playing',
  quality: 'auto',
  effectiveQuality: 'auto',
  volume: 1,
  danmakuEnabled: true,
  playbackAvailabilityStatus: 'checking',
};

describe('RoomPlaybackSurface', () => {
  it('computes single and multi room audio mute policy', () => {
    expect(getRoomMuted({ ...baseRoom, playbackAvailabilityStatus: 'available' }, 'single', '63136', false, false))
      .toBe(false);
    expect(getRoomMuted({ ...baseRoom, roomId: '270888', playbackAvailabilityStatus: 'available' }, 'single', '63136', false, false))
      .toBe(true);
    expect(getRoomMuted({ ...baseRoom, playbackAvailabilityStatus: 'available' }, 'multi', '270888', false, false))
      .toBe(false);
    expect(getRoomMuted({ ...baseRoom, roomId: '270888', playbackAvailabilityStatus: 'available' }, 'multi', '63136', false, false))
      .toBe(false);
    expect(getRoomMuted({ ...baseRoom, playbackAvailabilityStatus: 'available' }, 'multi', '63136', false, true))
      .toBe(true);
    expect(getRoomMuted({ ...baseRoom, online: false, status: 'offline', playbackAvailabilityStatus: 'available' }, 'multi', '63136', false, false))
      .toBe(true);
    for (const status of ['checking', 'blocked', 'error'] as const) {
      expect(getRoomMuted({ ...baseRoom, playbackAvailabilityStatus: status }, 'multi', '63136', false, false))
        .toBe(true);
    }
    expect(getRoomMuted({ ...baseRoom, playbackAvailabilityStatus: 'available' }, 'multi', '63136', true, false))
      .toBe(true);
  });

  it('renders the truthful blocked state outside demo mode', () => {
    const room: RoomSession = {
      ...baseRoom,
      playbackAvailabilityStatus: 'blocked',
      streamAvailability: {
        kind: 'blocked',
        roomId: '63136',
        reason: 'SIGNATURE_REQUIRED',
        observedQualities: [],
        checkedAt: '2026-08-07T00:00:00.000Z',
      },
    };

    const html = renderToStaticMarkup(
      <RoomPlaybackSurface room={room} demoMode={false} onRetry={() => {}} />,
    );

    expect(html).toContain('class="danmaku-overlay"');
    for (const message of getDanmakuMessages(room.roomId)) {
      expect(html).not.toContain(message);
    }

    expect(html).toContain('暂无合规播放源');
    expect(html).toContain('斗鱼当前只提供需签名的播放接口');
    expect(html).not.toContain('模拟画面');
  });

  it('retains the explicitly labeled simulated surface in demo mode', () => {
    const room: RoomSession = {
      ...baseRoom,
      playbackAvailabilityStatus: 'available',
      streamAvailability: {
        kind: 'available',
        roomId: '63136',
        variants: [{
          id: 'mock-auto',
          label: '自动',
          quality: 'auto',
          playbackUrl: 'mock://63136/auto',
          container: 'hls',
        }],
        checkedAt: '2026-08-07T00:00:00.000Z',
      },
    };

    const html = renderToStaticMarkup(
      <RoomPlaybackSurface room={room} demoMode onRetry={() => {}} />,
    );

    expect(html).toContain('class="danmaku-overlay"');
    for (const message of getDanmakuMessages(room.roomId)) {
      expect(html).not.toContain(message);
    }

    expect(html).toContain('模拟画面');
  });

  it('offers retry only after an availability error', () => {
    const html = renderToStaticMarkup(
      <RoomPlaybackSurface
        room={{ ...baseRoom, playbackAvailabilityStatus: 'error', playbackError: '网络错误' }}
        demoMode={false}
        onRetry={() => {}}
      />,
    );

    expect(html).toContain('播放能力检查失败');
    expect(html).toContain('重新检查');
  });

  it('uses an offline icon instead of a loading spinner for rooms that are not live', () => {
    const html = renderToStaticMarkup(
      <RoomPlaybackSurface
        room={{ ...baseRoom, online: false, status: 'offline' }}
        demoMode={false}
        onRetry={() => {}}
      />,
    );

    expect(html).toContain('主播当前未开播');
    expect(html).toContain('lucide-circle-slash-2');
    expect(html).not.toContain('lucide-loader-circle');
    expect(html).not.toContain('class="playback-retry-button"');
  });
});
