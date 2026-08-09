import type { RoomCandidate } from '../domain/douyu-adapter';
import { MOCK_ROOM_CANDIDATES } from '../infrastructure/mock-douyu-adapter';

export function getInitialRoomsForRuntime(electronMode: boolean): RoomCandidate[] {
  return electronMode ? [] : MOCK_ROOM_CANDIDATES.slice(0, 3);
}
