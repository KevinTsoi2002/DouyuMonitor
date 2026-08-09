import type {
  DanmakuDensity,
  DanmakuRegion,
} from './danmaku-settings';
import { getDanmakuDensityProfile } from './danmaku-settings';

export interface DanmakuLane {
  index: number;
  top: number;
}

export interface ActiveDanmakuGeometry {
  laneIndex: number;
  width: number;
  containerWidth: number;
  launchedAt: number;
  durationMs: number;
}

export interface DanmakuLaunchGeometry {
  width: number;
  containerWidth: number;
  launchedAt: number;
  durationMs: number;
  fontSize: number;
}

export function calculateLanes(
  containerHeight: number,
  fontSize: number,
  region: DanmakuRegion,
  density: DanmakuDensity,
): DanmakuLane[] {
  if (containerHeight <= 0 || fontSize <= 0) return [];
  const regionHeight = region === 'full' ? containerHeight : containerHeight / 2;
  const regionTop = region === 'bottom' ? containerHeight / 2 : 0;
  const lineHeight = fontSize * 1.35;
  const physicalCount = Math.max(1, Math.floor(regionHeight / lineHeight));
  const { laneRatio } = getDanmakuDensityProfile(density);
  const laneCount = Math.min(
    physicalCount,
    Math.max(1, Math.ceil(physicalCount * laneRatio)),
  );

  return Array.from({ length: laneCount }, (_, index) => ({
    index,
    top: regionTop + index * lineHeight,
  }));
}

function canReuseLane(
  predecessor: ActiveDanmakuGeometry,
  candidate: DanmakuLaunchGeometry,
): boolean {
  if (
    predecessor.containerWidth !== candidate.containerWidth ||
    predecessor.durationMs <= 0 ||
    candidate.durationMs <= 0
  ) {
    return false;
  }

  const elapsed = candidate.launchedAt - predecessor.launchedAt;
  if (elapsed >= predecessor.durationMs) return true;
  if (elapsed < 0) return false;

  const predecessorDistance = predecessor.containerWidth + predecessor.width;
  const predecessorProgress = elapsed / predecessor.durationMs;
  const predecessorLeft = predecessor.containerWidth
    - predecessorDistance * predecessorProgress;
  const predecessorTail = predecessorLeft + predecessor.width;
  const entryGap = candidate.containerWidth - predecessorTail;
  const minimumGap = Math.max(12, candidate.fontSize * 0.5);
  if (entryGap < minimumGap) return false;

  const predecessorSpeed = predecessorDistance / predecessor.durationMs;
  const candidateSpeed = (candidate.containerWidth + candidate.width)
    / candidate.durationMs;
  if (candidateSpeed <= predecessorSpeed) return true;

  const catchUpMs = (entryGap - minimumGap) / (candidateSpeed - predecessorSpeed);
  const predecessorRemainingMs = predecessor.durationMs - elapsed;
  return catchUpMs >= predecessorRemainingMs;
}

export function selectLane(
  lanes: readonly DanmakuLane[],
  active: readonly ActiveDanmakuGeometry[],
  candidate: DanmakuLaunchGeometry,
): DanmakuLane | undefined {
  if (
    candidate.containerWidth <= 0 ||
    candidate.width <= 0 ||
    candidate.durationMs <= 0
  ) {
    return undefined;
  }

  for (const lane of lanes) {
    const predecessor = active
      .filter((item) => (
        item.laneIndex === lane.index &&
        candidate.launchedAt - item.launchedAt < item.durationMs
      ))
      .sort((left, right) => right.launchedAt - left.launchedAt)[0];
    if (!predecessor || canReuseLane(predecessor, candidate)) return lane;
  }
  return undefined;
}
