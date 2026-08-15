import { describe, expect, it } from 'vitest';
import {
  DEFAULT_DANMAKU_SETTINGS,
  durationToSliderValue,
  getDanmakuDensityProfile,
  parseDanmakuSettings,
  sliderValueToDuration,
} from '../src/renderer/danmaku/danmaku-settings';

describe('danmaku settings', () => {
  it('uses the global defaults when settings are absent', () => {
    expect(DEFAULT_DANMAKU_SETTINGS).toEqual({
      durationSeconds: 8,
      fontSize: 24,
      opacity: 0.9,
      region: 'full',
      density: 'normal',
      fontFamily: 'microsoft-yahei',
      rendering: 'native',
      governance: {
        enabled: true,
        keywordBlacklist: [],
        duplicateWindowSeconds: 3,
        peakProtectionEnabled: true,
      },
    });
    expect(parseDanmakuSettings(undefined)).toEqual(DEFAULT_DANMAKU_SETTINGS);
  });

  it('normalizes governance settings and clamps its numeric window', () => {
    expect(
      parseDanmakuSettings({
        governance: {
          enabled: 'yes',
          keywordBlacklist: [' 刷屏 ', '刷屏', '', '  '],
          duplicateWindowSeconds: 0,
          peakProtectionEnabled: false,
        },
      }).governance,
    ).toEqual({
      enabled: true,
      keywordBlacklist: ['刷屏'],
      duplicateWindowSeconds: 1,
      peakProtectionEnabled: false,
    });

    expect(
      parseDanmakuSettings({
        governance: {
          duplicateWindowSeconds: 99,
        },
      }).governance.duplicateWindowSeconds,
    ).toBe(10);
  });

  it('limits governance keywords to 40 characters and 50 entries', () => {
    const longKeyword = 'a'.repeat(45);
    const keywords = [
      longKeyword,
      ...Array.from({ length: 55 }, (_, index) => `关键词${index}`),
    ];

    const parsed = parseDanmakuSettings({
      governance: { keywordBlacklist: keywords },
    }).governance;

    expect(parsed.keywordBlacklist).toHaveLength(50);
    expect(parsed.keywordBlacklist[0]).toBe('a'.repeat(40));
    expect(
      parsed.keywordBlacklist.every(
        (keyword) => Array.from(keyword).length <= 40,
      ),
    ).toBe(true);
  });

  it('clamps each numeric setting independently', () => {
    expect(
      parseDanmakuSettings({
        durationSeconds: 2,
        fontSize: 50,
        opacity: 0.1,
      }),
    ).toMatchObject({
      durationSeconds: 4,
      fontSize: 36,
      opacity: 0.3,
    });

    expect(
      parseDanmakuSettings({
        durationSeconds: 20,
        fontSize: 10,
        opacity: 2,
      }),
    ).toMatchObject({
      durationSeconds: 15,
      fontSize: 14,
      opacity: 1,
    });
  });

  it('falls back invalid values without discarding other valid fields', () => {
    expect(
      parseDanmakuSettings({
        durationSeconds: Number.NaN,
        fontSize: 30,
        opacity: 0.65,
        region: 'middle',
        density: 'massive',
        fontFamily: 'simhei',
        rendering: 'advanced',
      }),
    ).toEqual({
      durationSeconds: 8,
      fontSize: 30,
      opacity: 0.65,
      region: 'full',
      density: 'massive',
      fontFamily: 'simhei',
      rendering: 'advanced',
      governance: {
        enabled: true,
        keywordBlacklist: [],
        duplicateWindowSeconds: 3,
        peakProtectionEnabled: true,
      },
    });

    expect(
      parseDanmakuSettings({
        region: 'top',
        density: 'unknown',
        fontFamily: 'serif',
        rendering: 'canvas',
      }),
    ).toMatchObject({
      region: 'top',
      density: 'normal',
      fontFamily: 'microsoft-yahei',
      rendering: 'native',
    });
  });

  it('maps the reversed duration slider in both directions and clamps inputs', () => {
    expect(sliderValueToDuration(0)).toBe(15);
    expect(sliderValueToDuration(50)).toBe(9.5);
    expect(sliderValueToDuration(100)).toBe(4);
    expect(sliderValueToDuration(-10)).toBe(15);
    expect(sliderValueToDuration(120)).toBe(4);

    expect(durationToSliderValue(15)).toBe(0);
    expect(durationToSliderValue(9.5)).toBe(50);
    expect(durationToSliderValue(4)).toBe(100);
    expect(durationToSliderValue(20)).toBe(0);
    expect(durationToSliderValue(2)).toBe(100);
  });

  it('returns the lane and interval profile for every density', () => {
    expect(getDanmakuDensityProfile('massive')).toEqual({
      laneRatio: 1,
      intervalMs: 80,
    });
    expect(getDanmakuDensityProfile('normal')).toEqual({
      laneRatio: 0.7,
      intervalMs: 180,
    });
    expect(getDanmakuDensityProfile('reduced')).toEqual({
      laneRatio: 0.4,
      intervalMs: 360,
    });
  });
});
