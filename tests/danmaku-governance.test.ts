import { describe, expect, it } from 'vitest';
import type { DanmakuMessage } from '../src/shared/danmaku-contract';
import {
  applyDanmakuGovernance,
  createDanmakuGovernanceRuntime,
  getDanmakuPeakLevel,
} from '../src/renderer/danmaku/danmaku-governance';
import type { DanmakuGovernanceSettings } from '../src/renderer/danmaku/danmaku-settings';

function message(id: string, text: string): DanmakuMessage {
  return {
    id,
    roomId: '63136',
    nickname: `User ${id}`,
    text,
    receivedAt: new Date(10_000).toISOString(),
  };
}

function uniqueMessages(count: number): DanmakuMessage[] {
  return Array.from({ length: count }, (_, index) => message(String(index), `消息 ${index}`));
}

const baseSettings: DanmakuGovernanceSettings = {
  enabled: true,
  keywordBlacklist: ['广告'],
  duplicateWindowSeconds: 3,
  peakProtectionEnabled: false,
};

describe('danmaku governance', () => {
  it('filters matching keywords before the message enters the accepted list', () => {
    const result = applyDanmakuGovernance(
      [message('1', '请看广告'), message('2', '正常消息')],
      baseSettings,
      createDanmakuGovernanceRuntime(),
      10_000,
    );

    expect(result.accepted.map((item) => item.id)).toEqual(['2']);
    expect(result.stats.filtered).toBe(1);
    expect(result.stats.duplicates).toBe(0);
  });

  it('suppresses adjacent duplicate text inside the configured window', () => {
    const first = applyDanmakuGovernance(
      [message('1', '正常消息')],
      baseSettings,
      createDanmakuGovernanceRuntime(),
      10_000,
    );
    const duplicate = applyDanmakuGovernance(
      [message('2', ' 正常消息 ')],
      baseSettings,
      first.runtime,
      12_999,
    );
    const afterWindow = applyDanmakuGovernance(
      [message('3', '正常消息')],
      baseSettings,
      duplicate.runtime,
      13_000,
    );

    expect(duplicate.accepted).toEqual([]);
    expect(duplicate.stats.duplicates).toBe(1);
    expect(afterWindow.accepted.map((item) => item.id)).toEqual(['3']);
  });

  it('classifies the three peak levels at their documented boundaries', () => {
    expect(getDanmakuPeakLevel(10)).toBe('normal');
    expect(getDanmakuPeakLevel(10.01)).toBe('crowded');
    expect(getDanmakuPeakLevel(30)).toBe('crowded');
    expect(getDanmakuPeakLevel(30.01)).toBe('burst');
  });

  it('limits crowded input to twenty accepted messages per second', () => {
    const result = applyDanmakuGovernance(
      uniqueMessages(60),
      { ...baseSettings, keywordBlacklist: [], peakProtectionEnabled: true },
      createDanmakuGovernanceRuntime(),
      10_000,
    );

    expect(result.stats.level).toBe('crowded');
    expect(result.accepted).toHaveLength(20);
    expect(result.stats.rateLimited).toBe(40);
  });

  it('limits burst input to ten accepted messages per second', () => {
    const result = applyDanmakuGovernance(
      uniqueMessages(100),
      { ...baseSettings, keywordBlacklist: [], peakProtectionEnabled: true },
      createDanmakuGovernanceRuntime(),
      10_000,
    );

    expect(result.stats.level).toBe('burst');
    expect(result.accepted).toHaveLength(10);
    expect(result.stats.rateLimited).toBe(90);
  });

  it('keeps cumulative counters while reporting the current rolling rate', () => {
    const crowded: DanmakuGovernanceSettings = {
      ...baseSettings,
      keywordBlacklist: [],
      peakProtectionEnabled: true,
    };
    const first = applyDanmakuGovernance(
      uniqueMessages(60),
      crowded,
      createDanmakuGovernanceRuntime(),
      10_000,
    );
    const second = applyDanmakuGovernance(
      [message('next', '新消息')],
      crowded,
      first.runtime,
      13_001,
    );

    expect(second.stats.rateLimited).toBe(40);
    expect(second.stats.recentRate).toBe(0.33);
    expect(second.stats.peakRate).toBe(20);
    expect(second.stats.queueOverflow).toBe(0);
    expect(second.stats.upstreamDropped).toBe(0);
  });
});
