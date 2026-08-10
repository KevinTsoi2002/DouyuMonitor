import { describe, expect, it } from 'vitest';
import {
  moveRoomPlacement,
  nextPrimaryAfterRemoval,
  normalizeRoomPlacementOrder,
  swapPrimaryRoomPlacement,
} from '../src/renderer/store/room-placement';

describe('room placement', () => {
  it('normalizes a requested order by removing dangling and duplicate ids before appending active rooms', () => {
    expect(normalizeRoomPlacementOrder(
      ['63136', '270888', '999'],
      ['270888', 'missing', '270888'],
    )).toEqual(['270888', '63136', '999']);
  });

  it('moves a room into the target room original position', () => {
    expect(moveRoomPlacement(['a', 'b', 'c', 'd'], 'b', 'd')).toEqual(['a', 'c', 'd', 'b']);
    expect(moveRoomPlacement(['a', 'b'], 'missing', 'b')).toEqual(['a', 'b']);
  });

  it('swaps only the current primary and target positions across consecutive swaps', () => {
    const afterFirstSwap = swapPrimaryRoomPlacement(['a', 'b', 'c'], 'a', 'c');
    const afterSecondSwap = swapPrimaryRoomPlacement(afterFirstSwap, 'c', 'b');

    expect(afterFirstSwap).toEqual(['c', 'b', 'a']);
    expect(afterSecondSwap).toEqual(['b', 'c', 'a']);
  });

  it('leaves placement unchanged when no current primary room is selected', () => {
    const roomIds = ['a', 'b', 'c'];
    const result = swapPrimaryRoomPlacement(roomIds, undefined, 'c');

    expect(result).toEqual(roomIds);
    expect(result).not.toBe(roomIds);
  });

  it('selects the next visual room after removal, or the previous room at the end', () => {
    expect(nextPrimaryAfterRemoval(['a', 'b', 'c'], 'b')).toBe('c');
    expect(nextPrimaryAfterRemoval(['a', 'b', 'c'], 'c')).toBe('b');
    expect(nextPrimaryAfterRemoval(['a'], 'a')).toBeUndefined();
    expect(nextPrimaryAfterRemoval(['a', 'b'], 'missing')).toBe('a');
  });
});
