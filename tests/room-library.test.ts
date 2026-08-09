import { describe, expect, it } from 'vitest';
import {
  addHistoryEntry,
  addRoomIdToGroup,
  getRoomIdsForMode,
  toggleRoomId,
  type RoomHistoryEntry,
} from '../src/renderer/store/room-library';

describe('room library helpers', () => {
  it('deduplicates history and keeps the newest fifty rooms', () => {
    let history: RoomHistoryEntry[] = [];
    for (let index = 0; index < 51; index += 1) {
      history = addHistoryEntry(
        history,
        String(index),
        `2026-08-10T00:00:${String(index).padStart(2, '0')}Z`,
      );
    }

    history = addHistoryEntry(history, '10', '2026-08-10T01:00:00Z');

    expect(history).toHaveLength(50);
    expect(history[0]).toEqual({ roomId: '10', addedAt: '2026-08-10T01:00:00Z' });
    expect(new Set(history.map((item) => item.roomId)).size).toBe(50);
  });

  it('toggles room ids without duplicates', () => {
    expect(toggleRoomId(['1'], '1')).toEqual([]);
    expect(toggleRoomId([], '1')).toEqual(['1']);
  });

  it('keeps group order and enforces nine rooms', () => {
    let roomIds: string[] = [];
    for (let index = 1; index <= 9; index += 1) {
      const addition = addRoomIdToGroup(roomIds, String(index));
      expect(addition.result).toBe('added');
      roomIds = addition.roomIds;
    }

    expect(addRoomIdToGroup(roomIds, '10')).toEqual({ result: 'limit', roomIds });
    expect(addRoomIdToGroup(roomIds, '9')).toEqual({ result: 'duplicate', roomIds });
  });

  it('derives library ids without creating a new favorites reference', () => {
    const favoriteRoomIds = ['1', '2'];
    const history = [{ roomId: '3', addedAt: '2026-08-10T00:00:00Z' }];

    expect(getRoomIdsForMode('favorites', favoriteRoomIds, history)).toBe(favoriteRoomIds);
    expect(getRoomIdsForMode('history', favoriteRoomIds, history)).toEqual(['3']);
  });
});
