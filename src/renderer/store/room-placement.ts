export function normalizeRoomPlacementOrder(activeRoomIds: string[], requestedOrder: unknown): string[] {
  const activeIds = new Set(activeRoomIds);
  const normalized: string[] = [];
  const requestedIds = Array.isArray(requestedOrder) ? requestedOrder : [];

  for (const roomId of requestedIds) {
    if (typeof roomId === 'string' && activeIds.has(roomId) && !normalized.includes(roomId)) {
      normalized.push(roomId);
    }
  }

  for (const roomId of activeRoomIds) {
    if (!normalized.includes(roomId)) normalized.push(roomId);
  }

  return normalized;
}

export function moveRoomPlacement(roomIds: string[], sourceRoomId: string, targetRoomId: string): string[] {
  const sourceIndex = roomIds.indexOf(sourceRoomId);
  const targetIndex = roomIds.indexOf(targetRoomId);
  if (sourceIndex < 0 || targetIndex < 0 || sourceIndex === targetIndex) return [...roomIds];

  const movedRoomIds = [...roomIds];
  const [sourceRoom] = movedRoomIds.splice(sourceIndex, 1);
  movedRoomIds.splice(targetIndex, 0, sourceRoom);
  return movedRoomIds;
}

export function swapPrimaryRoomPlacement(
  roomIds: string[],
  currentPrimaryRoomId: string | undefined,
  targetPrimaryRoomId: string,
): string[] {
  const currentPrimaryIndex = currentPrimaryRoomId === undefined
    ? -1
    : roomIds.indexOf(currentPrimaryRoomId);
  const targetPrimaryIndex = roomIds.indexOf(targetPrimaryRoomId);
  if (currentPrimaryIndex < 0 || targetPrimaryIndex < 0 || currentPrimaryIndex === targetPrimaryIndex) {
    return [...roomIds];
  }

  const swappedRoomIds = [...roomIds];
  [swappedRoomIds[currentPrimaryIndex], swappedRoomIds[targetPrimaryIndex]] = [
    swappedRoomIds[targetPrimaryIndex],
    swappedRoomIds[currentPrimaryIndex],
  ];
  return swappedRoomIds;
}

export function nextPrimaryAfterRemoval(roomIds: string[], removedRoomId: string): string | undefined {
  const removedIndex = roomIds.indexOf(removedRoomId);
  if (removedIndex < 0) return undefined;
  return roomIds[removedIndex + 1] ?? roomIds[removedIndex - 1];
}
