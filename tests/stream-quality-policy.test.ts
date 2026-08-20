import { describe, expect, it } from 'vitest';
import type { RoomStatus, StreamQuality } from '../src/domain/douyu-adapter';
import {
  changedEffectiveQualityRoomIds,
  resolveEffectiveQualities,
  type QualityPolicyRoom,
} from '../src/renderer/store/stream-quality-policy';

function room(
  roomId: string,
  quality: StreamQuality,
  overrides: Partial<Pick<QualityPolicyRoom, 'online' | 'status'>> = {},
): QualityPolicyRoom {
  return {
    roomId,
    online: true,
    status: 'playing' satisfies RoomStatus,
    quality,
    ...overrides,
  };
}

const fourOnlineRooms: readonly QualityPolicyRoom[] = [
  room('1', 'original'),
  room('2', 'super'),
  room('3', 'high'),
  room('4', 'standard'),
];

const fiveOnlineRooms: readonly QualityPolicyRoom[] = [
  ...fourOnlineRooms,
  room('5', 'auto'),
];

describe('stream quality policy', () => {
  it('keeps stored qualities for four or fewer online rooms', () => {
    expect(resolveEffectiveQualities(fourOnlineRooms, '1')).toEqual(new Map([
      ['1', 'original'],
      ['2', 'super'],
      ['3', 'high'],
      ['4', 'standard'],
    ]));
  });

  it('uses original for the primary and 720p for other rooms above four', () => {
    expect(resolveEffectiveQualities(fiveOnlineRooms, '1')).toEqual(new Map([
      ['1', 'original'],
      ['2', '720p'],
      ['3', '720p'],
      ['4', '720p'],
      ['5', '720p'],
    ]));

    expect(resolveEffectiveQualities(fiveOnlineRooms, '3').get('3')).toBe('original');
    expect(resolveEffectiveQualities(fiveOnlineRooms, '3').get('1')).toBe('720p');
  });

  it('excludes offline rooms from the threshold while retaining their stored quality', () => {
    const listedRooms = [
      ...fourOnlineRooms,
      room('5', 'high', { online: false, status: 'offline' }),
    ];

    expect(resolveEffectiveQualities(listedRooms, '1')).toEqual(new Map([
      ['1', 'original'],
      ['2', 'super'],
      ['3', 'high'],
      ['4', 'standard'],
      ['5', 'high'],
    ]));
  });

  it('returns room IDs whose effective quality changed in after-map order', () => {
    expect(changedEffectiveQualityRoomIds(
      resolveEffectiveQualities(fiveOnlineRooms, '1'),
      resolveEffectiveQualities(fiveOnlineRooms, '3'),
    )).toEqual(['1', '3']);
  });
});
