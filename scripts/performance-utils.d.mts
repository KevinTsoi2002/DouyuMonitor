export interface PerformanceMetricSample {
  cpuPercent: number;
  workingSetBytes: number;
  privateBytes: number;
}

export interface PerformanceMetricSummary {
  sampleCount: number;
  averageCpuPercent: number;
  peakCpuPercent: number;
  averageWorkingSetBytes: number;
  peakWorkingSetBytes: number;
  peakPrivateBytes: number;
}

export interface PerformanceProfileLayout {
  id: 'single' | 'grid-2x2' | 'grid-3x2' | 'grid-3x3';
  shortLabel: '单' | '2×2' | '3×2' | '3×3';
}

export const SCREENSHOT_PRIVACY_STYLE: string;

export function parseRoomIds(value: string | undefined): string[];
export function parseProfileCounts(value: string | undefined): number[];
export function parseSampleDurationMs(value: string | undefined): number;
export function profileLayoutForRoomCount(roomCount: number): PerformanceProfileLayout;
export function summarizeMetricSamples(
  samples: PerformanceMetricSample[],
): PerformanceMetricSummary;
