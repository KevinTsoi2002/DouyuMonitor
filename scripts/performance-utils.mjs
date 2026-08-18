const ROOM_ID_ERROR = 'Room IDs must contain digits only';
const PROFILE_ERROR = 'Profiles must be integers from 1 to 9';
const DEFAULT_SAMPLE_DURATION_MS = 15_000;
const MIN_SAMPLE_DURATION_MS = 2_000;
const MAX_SAMPLE_DURATION_MS = 86_400_000;

export const SCREENSHOT_PRIVACY_STYLE = `
  .danmaku-overlay {
    visibility: hidden !important;
  }
`;

export function parseRoomIds(value) {
  if (typeof value !== 'string' || value.trim() === '') {
    throw new Error('At least one room ID is required');
  }

  const roomIds = value.split(',').map((roomId) => roomId.trim());
  if (roomIds.some((roomId) => !/^\d{1,20}$/.test(roomId))) {
    throw new Error(ROOM_ID_ERROR);
  }

  return [...new Set(roomIds)];
}

export function parseProfileCounts(value) {
  const profileValue = value === undefined ? '1,4,6,9' : value;
  if (typeof profileValue !== 'string' || profileValue.trim() === '') {
    throw new Error(PROFILE_ERROR);
  }

  const profiles = profileValue.split(',').map((profile) => profile.trim());
  if (profiles.some((profile) => !/^[1-9]$/.test(profile))) {
    throw new Error(PROFILE_ERROR);
  }

  return [...new Set(profiles.map(Number))];
}

export function parseSampleDurationMs(value) {
  if (value === undefined) return DEFAULT_SAMPLE_DURATION_MS;
  if (!/^\d+$/.test(value.trim())) throw new Error('INVALID_SAMPLE_DURATION');

  const duration = Number(value);
  if (
    !Number.isSafeInteger(duration)
    || duration < MIN_SAMPLE_DURATION_MS
    || duration > MAX_SAMPLE_DURATION_MS
  ) {
    throw new Error('INVALID_SAMPLE_DURATION');
  }
  return duration;
}

export function profileLayoutForRoomCount(roomCount) {
  if (roomCount <= 1) return { id: 'single', shortLabel: '单' };
  if (roomCount <= 4) return { id: 'grid-2x2', shortLabel: '2×2' };
  if (roomCount <= 6) return { id: 'grid-3x2', shortLabel: '3×2' };
  return { id: 'grid-3x3', shortLabel: '3×3' };
}

export function roomQualityPolicySatisfied(rooms) {
  if (!Array.isArray(rooms) || rooms.length <= 4) return true;
  return rooms.slice(1).every((room) => (
    Number.isFinite(room.videoWidth)
    && Number.isFinite(room.videoHeight)
    && room.videoWidth <= 1280
    && room.videoHeight <= 720
  ));
}

export function summarizeMetricSamples(samples) {
  if (!Array.isArray(samples) || samples.length === 0) {
    throw new Error('Metric samples are required');
  }

  const roundAverage = (total) => Math.round((total / samples.length) * 100) / 100;
  const cpuValues = samples.map((sample) => sample.cpuPercent);
  const workingSetValues = samples.map((sample) => sample.workingSetBytes);
  const privateByteValues = samples.map((sample) => sample.privateBytes);

  return {
    sampleCount: samples.length,
    averageCpuPercent: roundAverage(cpuValues.reduce((total, value) => total + value, 0)),
    peakCpuPercent: Math.max(...cpuValues),
    averageWorkingSetBytes: roundAverage(
      workingSetValues.reduce((total, value) => total + value, 0),
    ),
    peakWorkingSetBytes: Math.max(...workingSetValues),
    peakPrivateBytes: Math.max(...privateByteValues),
  };
}
