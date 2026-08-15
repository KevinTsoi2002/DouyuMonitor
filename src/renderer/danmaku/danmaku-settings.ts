export type DanmakuRegion = 'full' | 'top' | 'bottom';
export type DanmakuDensity = 'massive' | 'normal' | 'reduced';
export type DanmakuFontFamily = 'simhei' | 'microsoft-yahei';
export type DanmakuRendering = 'native' | 'advanced';

export interface DanmakuGovernanceSettings {
  enabled: boolean;
  keywordBlacklist: string[];
  duplicateWindowSeconds: number;
  peakProtectionEnabled: boolean;
}

export type DanmakuGovernanceOverride = Partial<DanmakuGovernanceSettings>;

export interface DanmakuSettings {
  durationSeconds: number;
  fontSize: number;
  opacity: number;
  region: DanmakuRegion;
  density: DanmakuDensity;
  fontFamily: DanmakuFontFamily;
  rendering: DanmakuRendering;
  governance: DanmakuGovernanceSettings;
}

export interface DanmakuDensityProfile {
  laneRatio: number;
  intervalMs: number;
}

export const DEFAULT_DANMAKU_GOVERNANCE: Readonly<DanmakuGovernanceSettings> = {
  enabled: true,
  keywordBlacklist: [],
  duplicateWindowSeconds: 3,
  peakProtectionEnabled: true,
};

export const DEFAULT_DANMAKU_SETTINGS: Readonly<DanmakuSettings> = {
  durationSeconds: 8,
  fontSize: 24,
  opacity: 0.9,
  region: 'full',
  density: 'normal',
  fontFamily: 'microsoft-yahei',
  rendering: 'native',
  governance: DEFAULT_DANMAKU_GOVERNANCE,
};

const REGIONS: readonly DanmakuRegion[] = ['full', 'top', 'bottom'];
const DENSITIES: readonly DanmakuDensity[] = ['massive', 'normal', 'reduced'];
const FONT_FAMILIES: readonly DanmakuFontFamily[] = [
  'simhei',
  'microsoft-yahei',
];
const RENDERING_MODES: readonly DanmakuRendering[] = ['native', 'advanced'];
const MAX_GOVERNANCE_KEYWORDS = 50;
const MAX_GOVERNANCE_KEYWORD_LENGTH = 40;

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

function parseBoolean(value: unknown, fallback: boolean): boolean {
  return typeof value === 'boolean' ? value : fallback;
}

function parseGovernanceKeywords(value: unknown): string[] {
  if (!Array.isArray(value)) return [];

  const keywords: string[] = [];
  const seen = new Set<string>();

  for (const item of value) {
    if (typeof item !== 'string') continue;

    const keyword = Array.from(
      item.trim().toLocaleLowerCase('zh-CN'),
    )
      .slice(0, MAX_GOVERNANCE_KEYWORD_LENGTH)
      .join('');
    if (!keyword || seen.has(keyword)) continue;

    seen.add(keyword);
    keywords.push(keyword);
    if (keywords.length >= MAX_GOVERNANCE_KEYWORDS) break;
  }

  return keywords;
}

export function parseDanmakuGovernanceSettings(
  value: unknown,
): DanmakuGovernanceSettings {
  const governance =
    typeof value === 'object' && value !== null
      ? (value as Record<string, unknown>)
      : {};

  return {
    enabled: parseBoolean(
      governance.enabled,
      DEFAULT_DANMAKU_GOVERNANCE.enabled,
    ),
    keywordBlacklist: parseGovernanceKeywords(governance.keywordBlacklist),
    duplicateWindowSeconds: parseNumber(
      governance.duplicateWindowSeconds,
      1,
      10,
      DEFAULT_DANMAKU_GOVERNANCE.duplicateWindowSeconds,
    ),
    peakProtectionEnabled: parseBoolean(
      governance.peakProtectionEnabled,
      DEFAULT_DANMAKU_GOVERNANCE.peakProtectionEnabled,
    ),
  };
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
    governance: parseDanmakuGovernanceSettings(settings.governance),
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
