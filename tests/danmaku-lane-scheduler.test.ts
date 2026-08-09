import { describe, expect, it } from 'vitest';
import {
  calculateLanes,
  selectLane,
  type ActiveDanmakuGeometry,
} from '../src/renderer/danmaku/danmaku-lane-scheduler';

describe('danmaku lane scheduler', () => {
  it('uses the approved density ratio for full-height tracks', () => {
    expect(calculateLanes(540, 24, 'full', 'massive')).toHaveLength(16);
    expect(calculateLanes(540, 24, 'full', 'normal')).toHaveLength(12);
    expect(calculateLanes(540, 24, 'full', 'reduced')).toHaveLength(7);
  });

  it('places top and bottom tracks in their selected half', () => {
    const top = calculateLanes(540, 24, 'top', 'massive');
    const bottom = calculateLanes(540, 24, 'bottom', 'massive');

    expect(top).toHaveLength(8);
    expect(top[0].top).toBe(0);
    expect(bottom).toHaveLength(8);
    expect(bottom[0].top).toBe(270);
    expect(bottom.at(-1)!.top).toBeLessThan(540);
  });

  it('returns no tracks while the container has no visible height', () => {
    expect(calculateLanes(0, 24, 'full', 'massive')).toEqual([]);
  });

  it('selects the first empty lane', () => {
    const lanes = calculateLanes(120, 24, 'full', 'massive');
    expect(selectLane(lanes, [], {
      width: 120,
      containerWidth: 800,
      launchedAt: 1000,
      durationMs: 8000,
      fontSize: 24,
    })).toEqual(lanes[0]);
  });

  it('rejects a lane before the predecessor tail clears the entry gap', () => {
    const active: ActiveDanmakuGeometry[] = [{
      laneIndex: 0,
      width: 200,
      containerWidth: 800,
      launchedAt: 0,
      durationMs: 8000,
    }];

    expect(selectLane([{ index: 0, top: 0 }], active, {
      width: 120,
      containerWidth: 800,
      launchedAt: 500,
      durationMs: 8000,
      fontSize: 24,
    })).toBeUndefined();
  });

  it('rejects a faster follower that would catch the predecessor', () => {
    const active: ActiveDanmakuGeometry[] = [{
      laneIndex: 0,
      width: 420,
      containerWidth: 800,
      launchedAt: 0,
      durationMs: 15000,
    }];

    expect(selectLane([{ index: 0, top: 0 }], active, {
      width: 80,
      containerWidth: 800,
      launchedAt: 7000,
      durationMs: 4000,
      fontSize: 24,
    })).toBeUndefined();
  });

  it('reuses a lane after a non-catching follower has enough entry space', () => {
    const active: ActiveDanmakuGeometry[] = [{
      laneIndex: 0,
      width: 120,
      containerWidth: 800,
      launchedAt: 0,
      durationMs: 4000,
    }];
    const lane = { index: 0, top: 0 };

    expect(selectLane([lane], active, {
      width: 160,
      containerWidth: 800,
      launchedAt: 2000,
      durationMs: 8000,
      fontSize: 24,
    })).toEqual(lane);
  });
});
