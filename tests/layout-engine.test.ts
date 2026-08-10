import { describe, expect, it } from 'vitest';
import type { PrimaryRoomRatio } from '../src/domain/layout-engine';
import {
  calculateLayout,
  calculatePrimaryFocusLayout,
  DEFAULT_PRIMARY_ROOM_RATIO,
  getRecommendedLayoutId,
  PRIMARY_ROOM_RATIO_MAX,
  PRIMARY_ROOM_RATIO_MIN,
  resolveLayoutId,
} from '../src/domain/layout-engine';

describe('calculateLayout', () => {
  it('recommends stable layouts for every supported room count', () => {
    expect([0, 1, 2, 4, 5, 6, 7, 9].map(getRecommendedLayoutId)).toEqual([
      'single',
      'single',
      'grid-2x2',
      'grid-2x2',
      'grid-3x2',
      'grid-3x2',
      'grid-3x3',
      'grid-3x3',
    ]);
  });

  it('resolves auto layout before calculating slots', () => {
    const roomIds = ['a', 'b', 'c', 'd', 'e'];

    expect(resolveLayoutId('auto', roomIds.length)).toBe('grid-3x2');
    expect(calculateLayout(roomIds, 'auto')).toEqual(
      calculateLayout(roomIds, 'grid-3x2'),
    );
  });

  it('resolves over-capacity manual layouts to the adaptive grid they render', () => {
    expect(resolveLayoutId('single', 2)).toBe('grid-2x2');
    expect(resolveLayoutId('grid-2x2', 5)).toBe('grid-3x2');
    expect(resolveLayoutId('grid-3x2', 7)).toBe('grid-3x3');
    expect(resolveLayoutId('split-horizontal', 3)).toBe('grid-2x2');
    expect(resolveLayoutId('split-vertical', 3)).toBe('grid-2x2');
  });

  it('creates a single full-size slot for one room', () => {
    expect(calculateLayout(['room-a'], 'single')).toEqual([
      { roomId: 'room-a', row: 1, column: 1, rowSpan: 1, columnSpan: 1 },
    ]);
  });

  it('creates a 2x2 grid for four rooms', () => {
    expect(calculateLayout(['a', 'b', 'c', 'd'], 'grid-2x2')).toEqual([
      { roomId: 'a', row: 1, column: 1, rowSpan: 1, columnSpan: 1 },
      { roomId: 'b', row: 1, column: 2, rowSpan: 1, columnSpan: 1 },
      { roomId: 'c', row: 2, column: 1, rowSpan: 1, columnSpan: 1 },
      { roomId: 'd', row: 2, column: 2, rowSpan: 1, columnSpan: 1 },
    ]);
  });

  it('uses a primary slot when the primary-two layout has three rooms', () => {
    expect(calculateLayout(['a', 'b', 'c'], 'primary-two', 'b')).toEqual([
      { roomId: 'b', row: 1, column: 1, rowSpan: 2, columnSpan: 1 },
      { roomId: 'a', row: 1, column: 3, rowSpan: 1, columnSpan: 1 },
      { roomId: 'c', row: 2, column: 3, rowSpan: 1, columnSpan: 1 },
    ]);
  });

  it.each([
    [['a', 'b'], 'b'],
    [['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'], 'f'],
  ])('keeps primary-two slots compatible with the primary-focus engine', (roomIds, primaryRoomId) => {
    expect(calculateLayout(roomIds, 'primary-two', primaryRoomId)).toEqual(
      calculatePrimaryFocusLayout(
        roomIds,
        primaryRoomId,
        DEFAULT_PRIMARY_ROOM_RATIO,
        { width: 0, height: 0 },
      ).slots,
    );
  });

  it('falls back to an adaptive grid for a layout that cannot fit the room count', () => {
    expect(calculateLayout(['a', 'b', 'c', 'd', 'e'], 'grid-2x2')).toHaveLength(5);
  });
});

describe('calculatePrimaryFocusLayout', () => {
  it.each([
    [1, 1, 0],
    [2, 1, 1],
    [3, 1, 2],
    [4, 1, 3],
    [5, 2, 2],
    [6, 2, 3],
    [7, 2, 3],
    [8, 2, 4],
    [9, 2, 4],
  ])('maps %i rooms to the expected secondary grid', (roomCount, secondaryColumns, secondaryRows) => {
    const roomIds = Array.from({ length: roomCount }, (_, index) => `room-${index + 1}`);
    const plan = calculatePrimaryFocusLayout(roomIds, roomIds[0], 0.6, { width: 1440, height: 900 });

    expect(plan.secondaryColumns).toBe(secondaryColumns);
    expect(plan.secondaryRows).toBe(secondaryRows);
    expect(plan.slots).toHaveLength(roomCount);
    expect(new Set(plan.slots.map((slot) => slot.roomId))).toEqual(new Set(roomIds));
  });

  it('places the selected primary room beside row-major secondary rooms', () => {
    const plan = calculatePrimaryFocusLayout(['a', 'b', 'c', 'd'], 'c', 0.6, { width: 1440, height: 900 });

    expect(plan.orderedRoomIds).toEqual(['a', 'b', 'c', 'd']);
    expect(plan.slots).toEqual([
      { roomId: 'c', row: 1, column: 1, rowSpan: 3, columnSpan: 1 },
      { roomId: 'a', row: 1, column: 3, rowSpan: 1, columnSpan: 1 },
      { roomId: 'b', row: 2, column: 3, rowSpan: 1, columnSpan: 1 },
      { roomId: 'd', row: 3, column: 3, rowSpan: 1, columnSpan: 1 },
    ]);
  });

  it('fills a two-column secondary grid in row-major order', () => {
    const plan = calculatePrimaryFocusLayout(
      ['a', 'b', 'c', 'd', 'e', 'f'],
      'c',
      0.6,
      { width: 1440, height: 900 },
    );

    expect(plan.slots).toEqual([
      { roomId: 'c', row: 1, column: 1, rowSpan: 3, columnSpan: 1 },
      { roomId: 'a', row: 1, column: 3, rowSpan: 1, columnSpan: 1 },
      { roomId: 'b', row: 1, column: 4, rowSpan: 1, columnSpan: 1 },
      { roomId: 'd', row: 2, column: 3, rowSpan: 1, columnSpan: 1 },
      { roomId: 'e', row: 2, column: 4, rowSpan: 1, columnSpan: 1 },
      { roomId: 'f', row: 3, column: 3, rowSpan: 1, columnSpan: 1 },
    ]);
  });

  it('uses the only fitting snap ratio for nine rooms', () => {
    const plan = calculatePrimaryFocusLayout(
      Array.from({ length: 9 }, (_, index) => `room-${index + 1}`),
      'room-1',
      0.67,
      { width: 1180, height: 780 },
    );

    expect(plan.availableRatios).toEqual([0.5]);
    expect(plan.effectiveRatio).toBe(0.5);
  });

  it('falls back to the hard minimum when no horizontal snap ratio fits', () => {
    const plan = calculatePrimaryFocusLayout(
      Array.from({ length: 9 }, (_, index) => `room-${index + 1}`),
      'room-1',
      0.67,
      { width: 821, height: 780 },
    );

    expect(plan.orientation).toBe('horizontal');
    expect(plan.availableRatios).toEqual([]);
    expect(plan.effectiveRatio).toBe(0.42);
    expect(plan.effectiveRatio).toBe(PRIMARY_ROOM_RATIO_MIN);
  });

  it('calculates a vertical fallback when no snap ratio fits', () => {
    const plan = calculatePrimaryFocusLayout(
      ['a', 'b', 'c'],
      'a',
      0.67,
      { width: 820, height: 600 },
    );

    expect(plan.orientation).toBe('vertical');
    expect(plan.availableRatios).toEqual([]);
    expect(plan.effectiveRatio).toBeCloseTo(1 - (2 * 135 + 8 + 8 * 3) / 600);
  });

  it('exports the hard ratio bounds', () => {
    expect(PRIMARY_ROOM_RATIO_MIN).toBe(0.42);
    expect(PRIMARY_ROOM_RATIO_MAX).toBe(0.7);
  });

  it('clamps an out-of-contract runtime preference to the hard maximum', () => {
    const plan = calculatePrimaryFocusLayout(
      ['a', 'b'],
      'a',
      0.9 as PrimaryRoomRatio,
      { width: 10_000, height: 100 },
    );

    expect(plan.availableRatios).toEqual([]);
    expect(plan.effectiveRatio).toBe(PRIMARY_ROOM_RATIO_MAX);
  });

  it.each([
    [820, 'vertical'],
    [821, 'horizontal'],
  ] as const)('uses the expected orientation at %ipx', (width, orientation) => {
    expect(
      calculatePrimaryFocusLayout(['a', 'b'], 'a', 0.6, { width, height: 900 }).orientation,
    ).toBe(orientation);
  });

  it('stacks the primary room above secondary rooms in narrow workspaces', () => {
    const plan = calculatePrimaryFocusLayout(['a', 'b', 'c'], 'a', 0.6, { width: 390, height: 844 });

    expect(plan.orientation).toBe('vertical');
    expect(plan.slots).toEqual([
      { roomId: 'a', row: 1, column: 1, rowSpan: 1, columnSpan: 1 },
      { roomId: 'b', row: 3, column: 1, rowSpan: 1, columnSpan: 1 },
      { roomId: 'c', row: 4, column: 1, rowSpan: 1, columnSpan: 1 },
    ]);
  });

  it('gives a single room the full workspace', () => {
    const plan = calculatePrimaryFocusLayout(['a'], 'a', 0.6, { width: 1440, height: 900 });

    expect(plan.effectiveRatio).toBe(1);
    expect(plan.availableRatios).toEqual([]);
    expect(plan.slots).toEqual([
      { roomId: 'a', row: 1, column: 1, rowSpan: 1, columnSpan: 1 },
    ]);
  });
});
