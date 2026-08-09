import { describe, expect, it } from 'vitest';
import type { RoomSession } from '../src/renderer/store/workspace-store';
import {
  LAYOUT_OPTIONS,
  QUALITY_OPTIONS,
  getPlaybackPresentation,
  getLayoutOption,
  getRoomActionSummary,
  getRoomInitials,
  getRoomTone,
} from '../src/renderer/ui-model';

const baseRoom: RoomSession = {
  roomId: '63136',
  anchorName: '示例主播',
  title: '示例直播间',
  category: 'CS2',
  online: true,
  viewerLabel: '1 万',
  status: 'playing',
  quality: 'auto',
  volume: 1,
  danmakuEnabled: true,
  playbackAvailabilityStatus: 'checking',
};

describe('ui model metadata', () => {
  it('exposes all approved layout choices with stable ids and labels', () => {
    expect(LAYOUT_OPTIONS.map((option) => option.id)).toEqual([
      'auto',
      'single',
      'grid-2x2',
      'grid-3x2',
      'grid-3x3',
      'primary-two',
      'split-horizontal',
      'split-vertical',
    ]);
    expect(LAYOUT_OPTIONS.every((option) => option.label && option.shortLabel)).toBe(true);
  });

  it('describes automatic layout mode as a selectable option', () => {
    expect(LAYOUT_OPTIONS[0]).toEqual(expect.objectContaining({
      id: 'auto',
      label: expect.any(String),
      hint: expect.any(String),
    }));
  });

  it('labels unknown layouts with the existing single-layout fallback', () => {
    expect(getLayoutOption('unknown-layout').id).toBe('single');
  });

  it('keeps quality choices ordered from adaptive to standard', () => {
    expect(QUALITY_OPTIONS.map((option) => option.value)).toEqual([
      'auto',
      'original',
      'super',
      'high',
      'standard',
    ]);
  });

  it('creates compact room initials and repeatable visual tones', () => {
    expect(getRoomInitials('星河')).toBe('星河');
    expect(getRoomInitials('Orange Soda')).toBe('OS');
    expect(getRoomTone(7)).toBe(getRoomTone(1));
  });

  it('presents checking and error states with disabled playback controls', () => {
    expect(getPlaybackPresentation(baseRoom)).toEqual(expect.objectContaining({
      title: '正在检查播放源',
      qualityDisabled: true,
      audioDisabled: true,
      canRetry: false,
    }));

    expect(getPlaybackPresentation({
      ...baseRoom,
      playbackAvailabilityStatus: 'error',
      playbackError: '无法连接斗鱼，请检查网络后重试',
    })).toEqual(expect.objectContaining({
      title: '播放能力检查失败',
      detail: '无法连接斗鱼，请检查网络后重试',
      canRetry: true,
    }));
  });

  it('presents offline rooms without a playback source check state', () => {
    expect(getPlaybackPresentation({
      ...baseRoom,
      online: false,
      status: 'offline',
    })).toEqual(expect.objectContaining({
      title: '主播当前未开播',
      detail: '开播后再检查播放源',
      canRetry: false,
    }));
  });

  it('summarizes room playback and danmaku state without exposing source details', () => {
    expect(getRoomActionSummary(baseRoom, 'connected')).toEqual({
      playbackLabel: '正在检查播放源',
      playbackDetail: '正在读取斗鱼公开播放能力',
      danmakuLabel: '弹幕已连接',
    });

    expect(getRoomActionSummary({
      ...baseRoom,
      playbackAvailabilityStatus: 'blocked',
      streamAvailability: {
        kind: 'blocked',
        roomId: '63136',
        reason: 'SIGNATURE_REQUIRED',
        observedQualities: [],
        checkedAt: '2026-08-07T00:00:00.000Z',
      },
    }, 'platform-blocked')).toEqual({
      playbackLabel: '暂无合规播放源',
      playbackDetail: '斗鱼当前只提供需签名的播放接口',
      danmakuLabel: '弹幕平台阻塞',
    });
  });

  it('explains each blocked reason and exposes observed quality labels', () => {
    const blockedRoom: RoomSession = {
      ...baseRoom,
      playbackAvailabilityStatus: 'blocked',
      streamAvailability: {
        kind: 'blocked',
        roomId: '63136',
        reason: 'SIGNATURE_REQUIRED',
        observedQualities: [
          { id: 'douyu-0', label: '蓝光10M', providerType: 0 },
        ],
        checkedAt: '2026-08-07T00:00:00.000Z',
      },
    };

    expect(getPlaybackPresentation(blockedRoom)).toEqual(expect.objectContaining({
      title: '暂无合规播放源',
      detail: '斗鱼当前只提供需签名的播放接口',
      qualityLabels: ['蓝光10M'],
      qualityDisabled: true,
      audioDisabled: true,
    }));
    if (blockedRoom.streamAvailability?.kind !== 'blocked') {
      throw new Error('expected blocked availability');
    }
    expect(getPlaybackPresentation({
      ...blockedRoom,
      streamAvailability: { ...blockedRoom.streamAvailability, reason: 'ROOM_OFFLINE' },
    })).toEqual(expect.objectContaining({ detail: '主播当前未开播' }));
    expect(getPlaybackPresentation({
      ...blockedRoom,
      streamAvailability: { ...blockedRoom.streamAvailability, reason: 'NO_PUBLIC_SOURCE' },
    })).toEqual(expect.objectContaining({ detail: '斗鱼当前未提供公开直连' }));
  });

  it('disables quality selection when an available source has one variant', () => {
    const presentation = getPlaybackPresentation({
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
    });

    expect(presentation).toEqual(expect.objectContaining({
      qualityDisabled: true,
      audioDisabled: false,
      qualityOptions: [{ value: 'auto', label: '自动' }],
      qualityLabels: ['自动'],
    }));
  });
  it('keeps all five mock variants enabled and in provider order', () => {
    const variants = QUALITY_OPTIONS.map(({ value, label }) => ({
      id: `mock-${value}`,
      label,
      quality: value,
      playbackUrl: `mock://63136/${value}`,
      container: 'hls' as const,
    }));
    const presentation = getPlaybackPresentation({
      ...baseRoom,
      playbackAvailabilityStatus: 'available',
      streamAvailability: {
        kind: 'available',
        roomId: '63136',
        variants,
        checkedAt: '2026-08-07T00:00:00.000Z',
      },
    });

    expect(presentation).toEqual(expect.objectContaining({
      qualityDisabled: false,
      qualityOptions: QUALITY_OPTIONS,
    }));
  });
});
