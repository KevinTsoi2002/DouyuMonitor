import { describe, expect, it } from 'vitest';
import {
  calculateLayout,
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
      { roomId: 'b', row: 1, column: 1, rowSpan: 2, columnSpan: 2 },
      { roomId: 'a', row: 1, column: 3, rowSpan: 1, columnSpan: 1 },
      { roomId: 'c', row: 2, column: 3, rowSpan: 1, columnSpan: 1 },
    ]);
  });

  it('falls back to an adaptive grid for a layout that cannot fit the room count', () => {
    expect(calculateLayout(['a', 'b', 'c', 'd', 'e'], 'grid-2x2')).toHaveLength(5);
  });
});
