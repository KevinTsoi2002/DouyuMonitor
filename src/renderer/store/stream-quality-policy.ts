import type {
  RoomStatus,
  StreamQuality,
  StreamRequestQuality,
} from '../../domain/douyu-adapter';

export interface QualityPolicyRoom {
  roomId: string;
  online: boolean;
  status: RoomStatus;
  quality: StreamQuality;
}

export function resolveEffectiveQualities(
  rooms: readonly QualityPolicyRoom[],
  primaryRoomId?: string,
): ReadonlyMap<string, StreamRequestQuality> {
  const onlineRooms = rooms.filter((room) => room.online && room.status !== 'offline');
  const adaptive = onlineRooms.length > 4;
  const primary = primaryRoomId && onlineRooms.some((room) => room.roomId === primaryRoomId)
    ? primaryRoomId
    : onlineRooms[0]?.roomId;

  return new Map(rooms.map((room) => [
    room.roomId,
    adaptive && room.online && room.status !== 'offline'
      ? room.roomId === primary ? 'original' : '720p'
      : room.quality,
  ]));
}

export function changedEffectiveQualityRoomIds(
  before: ReadonlyMap<string, StreamRequestQuality>,
  after: ReadonlyMap<string, StreamRequestQuality>,
): string[] {
  const ids = new Set([...before.keys(), ...after.keys()]);
  return [...ids].filter((roomId) => before.get(roomId) !== after.get(roomId));
}
