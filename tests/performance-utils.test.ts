import { describe, expect, it } from 'vitest';

import {
  SCREENSHOT_PRIVACY_STYLE,
  parseProfileCounts,
  parseRoomIds,
  parseSampleDurationMs,
  profileLayoutForRoomCount,
  roomQualityPolicySatisfied,
  summarizeMetricSamples,
} from '../scripts/performance-utils.mjs';

describe('performance baseline utilities', () => {
  it('hides danmaku content in retained screenshots', () => {
    expect(SCREENSHOT_PRIVACY_STYLE).toContain('.danmaku-overlay');
    expect(SCREENSHOT_PRIVACY_STYLE).toContain('visibility: hidden');
  });

  describe('parseRoomIds', () => {
    it('trims and de-duplicates numeric room IDs while preserving order', () => {
      expect(parseRoomIds(' 63136, 123456,63136 ')).toEqual(['63136', '123456']);
    });

    it.each(['https://www.douyu.com/63136', 'room 63136'])(
      'rejects non-numeric room input %s without echoing it',
      (value) => {
        expect(() => parseRoomIds(value)).toThrow('Room IDs must contain digits only');
      },
    );
  });

  describe('parseProfileCounts', () => {
    it('uses the default profile set', () => {
      expect(parseProfileCounts(undefined)).toEqual([1, 4, 6, 9]);
    });

    it('de-duplicates profiles while preserving order', () => {
      expect(parseProfileCounts(' 1,4,1,9 ')).toEqual([1, 4, 9]);
    });

    it.each(['0', '10'])(
      'rejects an out-of-range profile %s',
      (value) => {
        expect(() => parseProfileCounts(value)).toThrow(
          'Profiles must be integers from 1 to 9',
        );
      },
    );
  });

  describe('parseSampleDurationMs', () => {
    it('accepts the two-hour stability duration', () => {
      expect(parseSampleDurationMs('7200000')).toBe(7_200_000);
    });

    it.each(['1999', '86400001', 'not-a-number'])(
      'rejects an unsafe sample duration %s',
      (value) => {
        expect(() => parseSampleDurationMs(value)).toThrow('INVALID_SAMPLE_DURATION');
      },
    );
  });

  it.each([
    [1, { id: 'single', shortLabel: '单' }],
    [4, { id: 'grid-2x2', shortLabel: '2×2' }],
    [6, { id: 'grid-3x2', shortLabel: '3×2' }],
    [9, { id: 'grid-3x3', shortLabel: '3×3' }],
  ])('selects the release layout for a %i-room profile', (roomCount, expected) => {
    expect(profileLayoutForRoomCount(roomCount)).toEqual(expected);
  });

  describe('summarizeMetricSamples', () => {
    it('aggregates two samples and rounds averages to two decimals', () => {
      expect(
        summarizeMetricSamples([
          { cpuPercent: 12.345, workingSetBytes: 100, privateBytes: 120 },
          { cpuPercent: 7.111, workingSetBytes: 300, privateBytes: 500 },
        ]),
      ).toEqual({
        sampleCount: 2,
        averageCpuPercent: 9.73,
        peakCpuPercent: 12.345,
        averageWorkingSetBytes: 200,
        peakWorkingSetBytes: 300,
        peakPrivateBytes: 500,
      });
    });

    it('requires at least one sample', () => {
      expect(() => summarizeMetricSamples([])).toThrow('Metric samples are required');
    });
  });

  describe('roomQualityPolicySatisfied', () => {
    it('allows stored quality for four or fewer rooms', () => {
      expect(roomQualityPolicySatisfied([
        { videoWidth: 1920, videoHeight: 1080 },
        { videoWidth: 1920, videoHeight: 1080 },
      ])).toBe(true);
    });

    it('requires every non-primary room to be at most 1280 by 720 above four rooms', () => {
      const compliant = [
        { videoWidth: 1920, videoHeight: 1080 },
        ...Array.from({ length: 4 }, () => ({ videoWidth: 1280, videoHeight: 720 })),
      ];
      expect(roomQualityPolicySatisfied(compliant)).toBe(true);
      expect(roomQualityPolicySatisfied([
        compliant[0],
        { videoWidth: 1920, videoHeight: 1080 },
        ...compliant.slice(2),
      ])).toBe(false);
    });
  });
});
