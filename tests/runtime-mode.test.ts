import { describe, expect, it } from 'vitest';
import { MOCK_ROOM_CANDIDATES } from '../src/infrastructure/mock-douyu-adapter';
import { getInitialRoomsForRuntime } from '../src/renderer/runtime-mode';

describe('getInitialRoomsForRuntime', () => {
  it('starts Electron empty and keeps browser demo rooms', () => {
    expect(getInitialRoomsForRuntime(true)).toEqual([]);
    expect(getInitialRoomsForRuntime(false)).toEqual(MOCK_ROOM_CANDIDATES.slice(0, 3));
  });
});
