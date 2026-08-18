import { describe, expect, it } from 'vitest';
import { getDisplayedRoomQuality } from '../src/renderer/components/RoomTile';

describe('room tile effective quality', () => {
  it('shows adaptive 720p without changing the stored user selection', () => {
    expect(getDisplayedRoomQuality('720p', 'original')).toBe('high');
  });

  it('falls back to the stored selection when no adaptive override exists', () => {
    expect(getDisplayedRoomQuality(undefined, 'super')).toBe('super');
  });
});
