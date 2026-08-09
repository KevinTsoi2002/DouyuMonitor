export type DanmakuRegion = 'full' | 'top' | 'bottom';
export type DanmakuDensity = 'massive' | 'normal' | 'reduced';
export type DanmakuFontFamily = 'simhei' | 'microsoft-yahei';
export type DanmakuRendering = 'native' | 'advanced';

export interface DanmakuSettings {
  durationSeconds: number;
  fontSize: number;
  opacity: number;
  region: DanmakuRegion;
  density: DanmakuDensity;
  fontFamily: DanmakuFontFamily;
  rendering: DanmakuRendering;
}

export interface DanmakuDensityProfile {
  laneRatio: number;
  intervalMs: number;
}

export const DEFAULT_DANMAKU_SETTINGS: Readonly<DanmakuSettings> = {
  durationSeconds: 8,
  fontSize: 24,
  opacity: 0.9,
  region: 'full',
  density: 'normal',
  fontFamily: 'microsoft-yahei',
  rendering: 'native',
};

const REGIONS: readonly DanmakuRegion[] = ['full', 'top', 'bottom'];
const DENSITIES: readonly DanmakuDensity[] = ['massive', 'normal', 'reduced'];
const FONT_FAMILIES: readonly DanmakuFontFamily[] = [
  'simhei',
  'microsoft-yahei',
];
const RENDERING_MODES: readonly DanmakuRendering[] = ['native', 'advanced'];

const DENSITY_PROFILES: Record<DanmakuDensity, DanmakuDensityProfile> = {
  massive: { laneRatio: 1, intervalMs: 80 },
  normal: { laneRatio: 0.7, intervalMs: 180 },
  reduced: { laneRatio: 0.4, intervalMs: 360 },
};

function clamp(value: number, minimum: number, maximum: number): number {
  return Math.min(maximum, Math.max(minimum, value));
}

function parseNumber(
  value: unknown,
  minimum: number,
  maximum: number,
  fallback: number,
): number {
  return typeof value === 'number' && Number.isFinite(value)
    ? clamp(value, minimum, maximum)
    : fallback;
}

function parseEnum<T extends string>(
  value: unknown,
  allowedValues: readonly T[],
  fallback: T,
): T {
  return typeof value === 'string' && allowedValues.some((item) => item === value)
    ? (value as T)
    : fallback;
}

export function parseDanmakuSettings(value: unknown): DanmakuSettings {
  const settings =
    typeof value === 'object' && value !== null
      ? (value as Record<string, unknown>)
      : {};

  return {
    durationSeconds: parseNumber(
      settings.durationSeconds,
      4,
      15,
      DEFAULT_DANMAKU_SETTINGS.durationSeconds,
    ),
    fontSize: parseNumber(
      settings.fontSize,
      14,
      36,
      DEFAULT_DANMAKU_SETTINGS.fontSize,
    ),
    opacity: parseNumber(
      settings.opacity,
      0.3,
      1,
      DEFAULT_DANMAKU_SETTINGS.opacity,
    ),
    region: parseEnum(
      settings.region,
      REGIONS,
      DEFAULT_DANMAKU_SETTINGS.region,
    ),
    density: parseEnum(
      settings.density,
      DENSITIES,
      DEFAULT_DANMAKU_SETTINGS.density,
    ),
    fontFamily: parseEnum(
      settings.fontFamily,
      FONT_FAMILIES,
      DEFAULT_DANMAKU_SETTINGS.fontFamily,
    ),
    rendering: parseEnum(
      settings.rendering,
      RENDERING_MODES,
      DEFAULT_DANMAKU_SETTINGS.rendering,
    ),
  };
}

export function sliderValueToDuration(sliderValue: number): number {
  return 15 - (clamp(sliderValue, 0, 100) / 100) * 11;
}

export function durationToSliderValue(durationSeconds: number): number {
  return ((15 - clamp(durationSeconds, 4, 15)) / 11) * 100;
}

export function getDanmakuDensityProfile(
  density: DanmakuDensity,
): DanmakuDensityProfile {
  return DENSITY_PROFILES[density];
}
