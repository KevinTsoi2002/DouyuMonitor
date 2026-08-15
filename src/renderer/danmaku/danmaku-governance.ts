import type { DanmakuMessage } from '../../shared/danmaku-contract';
import type { DanmakuGovernanceSettings } from './danmaku-settings';

export type DanmakuPeakLevel = 'normal' | 'crowded' | 'burst';

export interface DanmakuGovernanceStats {
  level: DanmakuPeakLevel;
  recentRate: number;
  peakRate: number;
  filtered: number;
  duplicates: number;
  rateLimited: number;
  queueOverflow: number;
  upstreamDropped: number;
}

export interface DanmakuGovernanceRuntime {
  inputTimestamps: number[];
  acceptedTimestamps: number[];
  lastComparableText?: string;
  lastComparableAt?: number;
  peakRate: number;
  stats: DanmakuGovernanceStats;
}

export interface DanmakuGovernanceResult {
  accepted: DanmakuMessage[];
  runtime: DanmakuGovernanceRuntime;
  stats: DanmakuGovernanceStats;
}

const INPUT_WINDOW_MS = 3_000;
const STATS_WINDOW_MS = 60_000;
const ACCEPTED_WINDOW_MS = 1_000;
const CROWDED_LIMIT = 20;
const BURST_LIMIT = 10;

function roundRate(rate: number): number {
  return Math.round(rate * 100) / 100;
}

function pruneTimestamps(timestamps: number[], now: number, windowMs: number): number[] {
  const cutoff = now - windowMs;
  return timestamps.filter((timestamp) => timestamp >= cutoff && timestamp <= now);
}

function normalizeText(text: string): string {
  return text.trim().toLocaleLowerCase('zh-CN');
}

function createStats(): DanmakuGovernanceStats {
  return {
    level: 'normal',
    recentRate: 0,
    peakRate: 0,
    filtered: 0,
    duplicates: 0,
    rateLimited: 0,
    queueOverflow: 0,
    upstreamDropped: 0,
  };
}

function getLimit(level: DanmakuPeakLevel): number {
  if (level === 'crowded') return CROWDED_LIMIT;
  if (level === 'burst') return BURST_LIMIT;
  return Number.POSITIVE_INFINITY;
}

export function createDanmakuGovernanceRuntime(): DanmakuGovernanceRuntime {
  return {
    inputTimestamps: [],
    acceptedTimestamps: [],
    peakRate: 0,
    stats: createStats(),
  };
}

export function getDanmakuPeakLevel(rate: number): DanmakuPeakLevel {
  if (!Number.isFinite(rate) || rate <= 10) return 'normal';
  if (rate <= 30) return 'crowded';
  return 'burst';
}

export function applyDanmakuGovernance(
  messages: DanmakuMessage[],
  settings: DanmakuGovernanceSettings,
  runtime: DanmakuGovernanceRuntime,
  now: number,
): DanmakuGovernanceResult {
  const inputTimestamps = pruneTimestamps(
    [...runtime.inputTimestamps, ...messages.map(() => now)],
    now,
    STATS_WINDOW_MS,
  );
  const recentInputCount = inputTimestamps.filter(
    (timestamp) => timestamp >= now - INPUT_WINDOW_MS,
  ).length;
  const recentRate = roundRate(recentInputCount / (INPUT_WINDOW_MS / 1_000));
  const level = getDanmakuPeakLevel(recentRate);
  const stats: DanmakuGovernanceStats = {
    ...runtime.stats,
    level,
    recentRate,
    peakRate: Math.max(runtime.stats.peakRate, recentRate),
  };
  const acceptedTimestamps = pruneTimestamps(
    runtime.acceptedTimestamps,
    now,
    ACCEPTED_WINDOW_MS,
  );
  const accepted: DanmakuMessage[] = [];
  let lastComparableText = runtime.lastComparableText;
  let lastComparableAt = runtime.lastComparableAt;
  const duplicateWindowMs = settings.duplicateWindowSeconds * 1_000;
  const normalizedKeywords = settings.keywordBlacklist.map(normalizeText).filter(Boolean);
  const limit = settings.peakProtectionEnabled ? getLimit(level) : Number.POSITIVE_INFINITY;

  for (const message of messages) {
    const comparableText = normalizeText(message.text);

    if (settings.enabled && normalizedKeywords.some((keyword) => comparableText.includes(keyword))) {
      stats.filtered += 1;
      continue;
    }

    if (
      settings.enabled &&
      comparableText === lastComparableText &&
      lastComparableAt !== undefined &&
      now - lastComparableAt < duplicateWindowMs
    ) {
      stats.duplicates += 1;
      continue;
    }

    if (settings.enabled) {
      lastComparableText = comparableText;
      lastComparableAt = now;
    }

    if (settings.enabled && acceptedTimestamps.length >= limit) {
      stats.rateLimited += 1;
      continue;
    }

    accepted.push(message);
    acceptedTimestamps.push(now);
  }

  const nextRuntime: DanmakuGovernanceRuntime = {
    inputTimestamps,
    acceptedTimestamps,
    ...(lastComparableText === undefined ? {} : { lastComparableText }),
    ...(lastComparableAt === undefined ? {} : { lastComparableAt }),
    peakRate: stats.peakRate,
    stats,
  };
  return { accepted, runtime: nextRuntime, stats };
}
