export type RecommendedLayoutId = 'single' | 'grid-2x2' | 'grid-3x2' | 'grid-3x3';
export type LayoutId = 'auto' | RecommendedLayoutId | 'primary-two' | 'split-horizontal' | 'split-vertical' | string;

export interface LayoutSlot {
  roomId: string;
  row: number;
  column: number;
  rowSpan: number;
  columnSpan: number;
}

export type PrimaryRoomRatio = 0.5 | 0.6 | 0.67;
export type PrimaryLayoutOrientation = 'horizontal' | 'vertical';

export interface WorkspaceSize {
  width: number;
  height: number;
}

export interface PrimaryFocusLayoutPlan {
  kind: 'primary-focus';
  primaryRoomId: string;
  orderedRoomIds: string[];
  secondaryColumns: 1 | 2;
  secondaryRows: number;
  preferredRatio: PrimaryRoomRatio;
  effectiveRatio: number;
  availableRatios: PrimaryRoomRatio[];
  orientation: PrimaryLayoutOrientation;
  slots: LayoutSlot[];
}

export const PRIMARY_ROOM_RATIOS: readonly PrimaryRoomRatio[] = [0.5, 0.6, 0.67];
export const DEFAULT_PRIMARY_ROOM_RATIO: PrimaryRoomRatio = 0.6;
export const PRIMARY_ROOM_RATIO_MIN = 0.42;
export const PRIMARY_ROOM_RATIO_MAX = 0.7;

const GRID_GAP = 8;
const DIVIDER_SIZE = 8;
const MIN_SECONDARY_WIDTH = 240;
const MIN_SECONDARY_HEIGHT = 135;

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

function secondaryGrid(roomCount: number): { columns: 1 | 2; rows: number } {
  const secondaryCount = roomCount - 1;
  if (secondaryCount <= 0) return { columns: 1, rows: 0 };
  const columns: 1 | 2 = secondaryCount <= 3 ? 1 : 2;
  return { columns, rows: Math.ceil(secondaryCount / columns) };
}

function clampPrimaryRatio(ratio: number): number {
  return Math.min(PRIMARY_ROOM_RATIO_MAX, Math.max(PRIMARY_ROOM_RATIO_MIN, ratio));
}

function ratioFits(
  ratio: PrimaryRoomRatio,
  orientation: PrimaryLayoutOrientation,
  size: WorkspaceSize,
  secondaryColumns: number,
  secondaryRows: number,
): boolean {
  if (size.width <= 0 || size.height <= 0 || secondaryRows === 0) return true;

  if (orientation === 'horizontal') {
    const secondaryWidth = (size.width * (1 - ratio) - DIVIDER_SIZE - GRID_GAP * (secondaryColumns + 1)) / secondaryColumns;
    const secondaryHeight = (size.height - GRID_GAP * (secondaryRows - 1)) / secondaryRows;
    return secondaryWidth >= MIN_SECONDARY_WIDTH && secondaryHeight >= MIN_SECONDARY_HEIGHT;
  }

  const secondaryWidth = (size.width - GRID_GAP * (secondaryColumns - 1)) / secondaryColumns;
  const secondaryHeight = (size.height * (1 - ratio) - DIVIDER_SIZE - GRID_GAP * (secondaryRows + 1)) / secondaryRows;
  return secondaryWidth >= MIN_SECONDARY_WIDTH && secondaryHeight >= MIN_SECONDARY_HEIGHT;
}

function maximumRatioForTargets(
  orientation: PrimaryLayoutOrientation,
  size: WorkspaceSize,
  secondaryColumns: number,
  secondaryRows: number,
): number {
  if (orientation === 'horizontal') {
    return 1 - (secondaryColumns * MIN_SECONDARY_WIDTH + DIVIDER_SIZE + GRID_GAP * (secondaryColumns + 1)) / size.width;
  }

  return 1 - (secondaryRows * MIN_SECONDARY_HEIGHT + DIVIDER_SIZE + GRID_GAP * (secondaryRows + 1)) / size.height;
}

export function calculatePrimaryFocusLayout(
  roomIds: string[],
  requestedPrimaryRoomId: string | undefined,
  preferredRatio: PrimaryRoomRatio,
  size: WorkspaceSize,
): PrimaryFocusLayoutPlan {
  const primaryRoomId = requestedPrimaryRoomId && roomIds.includes(requestedPrimaryRoomId)
    ? requestedPrimaryRoomId
    : roomIds[0] ?? '';
  const { columns: secondaryColumns, rows: secondaryRows } = secondaryGrid(roomIds.length);
  const orientation: PrimaryLayoutOrientation = size.width > 0 && size.width <= 820 ? 'vertical' : 'horizontal';

  if (roomIds.length <= 1) {
    return {
      kind: 'primary-focus',
      primaryRoomId,
      orderedRoomIds: [...roomIds],
      secondaryColumns,
      secondaryRows,
      preferredRatio,
      effectiveRatio: 1,
      availableRatios: [],
      orientation,
      slots: roomIds.length === 0 ? [] : [{ roomId: primaryRoomId, row: 1, column: 1, rowSpan: 1, columnSpan: 1 }],
    };
  }

  const availableRatios = PRIMARY_ROOM_RATIOS.filter((ratio) => ratioFits(ratio, orientation, size, secondaryColumns, secondaryRows));
  const effectiveRatio = availableRatios.includes(preferredRatio)
    ? preferredRatio
    : availableRatios.length > 0
      ? availableRatios.reduce((closest, ratio) => Math.abs(ratio - preferredRatio) < Math.abs(closest - preferredRatio) ? ratio : closest)
      : clampPrimaryRatio(Math.min(preferredRatio, maximumRatioForTargets(orientation, size, secondaryColumns, secondaryRows)));
  const secondaryRoomIds = roomIds.filter((roomId) => roomId !== primaryRoomId);
  const slots = orientation === 'horizontal'
    ? [
        { roomId: primaryRoomId, row: 1, column: 1, rowSpan: Math.max(1, secondaryRows), columnSpan: 1 },
        ...secondaryRoomIds.map((roomId, index) => ({
          roomId,
          row: Math.floor(index / secondaryColumns) + 1,
          column: (index % secondaryColumns) + 3,
          rowSpan: 1,
          columnSpan: 1,
        })),
      ]
    : [
        { roomId: primaryRoomId, row: 1, column: 1, rowSpan: 1, columnSpan: secondaryColumns },
        ...secondaryRoomIds.map((roomId, index) => ({
          roomId,
          row: Math.floor(index / secondaryColumns) + 3,
          column: (index % secondaryColumns) + 1,
          rowSpan: 1,
          columnSpan: 1,
        })),
      ];

  return {
    kind: 'primary-focus',
    primaryRoomId,
    orderedRoomIds: [...roomIds],
    secondaryColumns,
    secondaryRows,
    preferredRatio,
    effectiveRatio,
    availableRatios,
    orientation,
    slots,
  };
}

function primaryTwoSlots(roomIds: string[], primaryRoomId?: string): LayoutSlot[] {
  return calculatePrimaryFocusLayout(
    roomIds,
    primaryRoomId,
    DEFAULT_PRIMARY_ROOM_RATIO,
    { width: 0, height: 0 },
  ).slots;
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
