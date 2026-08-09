export interface RoomSummary {
  roomId: string;
  anchorName: string;
}

export type AddRoomResult =
  | { added: true; room: RoomSummary }
  | { added: false; reason: 'duplicate' | 'limit' };

export class RoomRegistry {
  static readonly MAX_ROOMS = 9;
  private readonly rooms: RoomSummary[] = [];

  add(room: RoomSummary): AddRoomResult {
    if (this.rooms.some((existing) => existing.roomId === room.roomId)) {
      return { added: false, reason: 'duplicate' };
    }
    if (this.rooms.length >= RoomRegistry.MAX_ROOMS) {
      return { added: false, reason: 'limit' };
    }
    this.rooms.push(room);
    return { added: true, room };
  }

  list(): RoomSummary[] {
    return [...this.rooms];
  }

  remove(roomId: string): boolean {
    const index = this.rooms.findIndex((room) => room.roomId === roomId);
    if (index === -1) return false;
    this.rooms.splice(index, 1);
    return true;
  }
}
