import { describe, expect, it } from 'vitest';
import {
  calculateLayout,
  calculatePrimaryFocusLayout,
  getRecommendedLayoutId,
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
