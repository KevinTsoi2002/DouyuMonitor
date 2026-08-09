import type { RoomCandidate, StreamQuality } from '../../domain/douyu-adapter';

export const MAX_HISTORY_ROOMS = 50;
export const MAX_GROUP_ROOMS = 9;
export const DEFAULT_ROOM_VOLUME = 0.5;

export interface LibraryRoom extends RoomCandidate {
  quality: StreamQuality;
  danmakuEnabled: boolean;
  volume: number;
}

export interface RoomHistoryEntry {
  roomId: string;
  addedAt: string;
}

export interface RoomGroup {
  id: string;
  name: string;
  roomIds: string[];
  createdAt: string;
}

export type RoomLibrary = Record<string, LibraryRoom>;

export function getRoomIdsForMode(
  mode: 'favorites' | 'history',
  favoriteRoomIds: string[],
  history: RoomHistoryEntry[],
): string[] {
  return mode === 'favorites' ? favoriteRoomIds : history.map((entry) => entry.roomId);
}

export function addHistoryEntry(
  history: RoomHistoryEntry[],
  roomId: string,
  addedAt: string,
): RoomHistoryEntry[] {
  return [
    { roomId, addedAt },
    ...history.filter((item) => item.roomId !== roomId),
  ].slice(0, MAX_HISTORY_ROOMS);
}

export function toggleRoomId(roomIds: string[], roomId: string): string[] {
  return roomIds.includes(roomId)
    ? roomIds.filter((id) => id !== roomId)
    : [roomId, ...roomIds];
}

export function addRoomIdToGroup(
  roomIds: string[],
  roomId: string,
): { result: 'added' | 'duplicate' | 'limit'; roomIds: string[] } {
  if (roomIds.includes(roomId)) return { result: 'duplicate', roomIds };
  if (roomIds.length >= MAX_GROUP_ROOMS) return { result: 'limit', roomIds };
  return { result: 'added', roomIds: [...roomIds, roomId] };
}
