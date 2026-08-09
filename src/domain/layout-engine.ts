export type RecommendedLayoutId = 'single' | 'grid-2x2' | 'grid-3x2' | 'grid-3x3';
export type LayoutId = 'auto' | RecommendedLayoutId | 'primary-two' | 'split-horizontal' | 'split-vertical' | string;

export interface LayoutSlot {
  roomId: string;
  row: number;
  column: number;
  rowSpan: number;
  columnSpan: number;
}

function gridSlots(roomIds: string[], columns: number): LayoutSlot[] {
  return roomIds.map((roomId, index) => ({
    roomId,
    row: Math.floor(index / columns) + 1,
    column: (index % columns) + 1,
    rowSpan: 1,
    columnSpan: 1,
  }));
}

function adaptiveSlots(roomIds: string[]): LayoutSlot[] {
  if (roomIds.length === 0) return [];
  const columns = Math.ceil(Math.sqrt(roomIds.length));
  return gridSlots(roomIds, columns);
}

function primaryTwoSlots(roomIds: string[], primaryRoomId?: string): LayoutSlot[] {
  if (roomIds.length < 3) return gridSlots(roomIds, 2);

  const primary = primaryRoomId && roomIds.includes(primaryRoomId) ? primaryRoomId : roomIds[0];
  const remaining = roomIds.filter((roomId) => roomId !== primary).slice(0, 2);
  return [
    { roomId: primary, row: 1, column: 1, rowSpan: 2, columnSpan: 2 },
    ...remaining.map((roomId, index) => ({
      roomId,
      row: index + 1,
      column: 3,
      rowSpan: 1,
      columnSpan: 1,
    })),
    ...roomIds.filter((roomId) => roomId !== primary && !remaining.includes(roomId)).map((roomId, index) => ({
      roomId,
      row: Math.floor(index / 2) + 3,
      column: (index % 2) + 1,
      rowSpan: 1,
      columnSpan: 1,
    })),
  ];
}

export function getRecommendedLayoutId(roomCount: number): RecommendedLayoutId {
  if (roomCount <= 1) return 'single';
  if (roomCount <= 4) return 'grid-2x2';
  if (roomCount <= 6) return 'grid-3x2';
  return 'grid-3x3';
}

export function resolveLayoutId(layoutId: LayoutId, roomCount: number): LayoutId {
  const resolvedLayoutId = layoutId === 'auto' ? getRecommendedLayoutId(roomCount) : layoutId;

  switch (resolvedLayoutId) {
    case 'single':
      return roomCount <= 1 ? resolvedLayoutId : getRecommendedLayoutId(roomCount);
    case 'grid-2x2':
      return roomCount <= 4 ? resolvedLayoutId : getRecommendedLayoutId(roomCount);
    case 'grid-3x2':
      return roomCount <= 6 ? resolvedLayoutId : getRecommendedLayoutId(roomCount);
    case 'split-horizontal':
    case 'split-vertical':
      return roomCount <= 2 ? resolvedLayoutId : getRecommendedLayoutId(roomCount);
    default:
      return resolvedLayoutId;
  }
}

export function calculateLayout(
  roomIds: string[],
  layoutId: LayoutId,
  primaryRoomId?: string,
): LayoutSlot[] {
  if (roomIds.length === 0) return [];

  switch (resolveLayoutId(layoutId, roomIds.length)) {
    case 'single':
      return roomIds.length === 1 ? gridSlots(roomIds, 1) : adaptiveSlots(roomIds);
    case 'grid-2x2':
      return roomIds.length <= 4 ? gridSlots(roomIds, 2) : adaptiveSlots(roomIds);
    case 'grid-3x2':
      return roomIds.length <= 6 ? gridSlots(roomIds, 3) : adaptiveSlots(roomIds);
    case 'grid-3x3':
      return roomIds.length <= 9 ? gridSlots(roomIds, 3) : adaptiveSlots(roomIds);
    case 'primary-two':
      return primaryTwoSlots(roomIds, primaryRoomId);
    case 'split-horizontal':
      return roomIds.length <= 2 ? gridSlots(roomIds, 2) : adaptiveSlots(roomIds);
    case 'split-vertical':
      return roomIds.length <= 2
        ? roomIds.map((roomId, index) => ({
            roomId,
            row: index + 1,
            column: 1,
            rowSpan: 1,
            columnSpan: 1,
          }))
        : adaptiveSlots(roomIds);
    default:
      return adaptiveSlots(roomIds);
  }
}
