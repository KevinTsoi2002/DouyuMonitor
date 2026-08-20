import { readFileSync } from 'node:fs';
import { describe, expect, it } from 'vitest';

const source = readFileSync('scripts/performance-baseline.mjs', 'utf8');

describe('performance baseline script', () => {
  it('records sustained playback and decoded video metrics for every room', () => {
    for (const field of [
      'initialCurrentTime',
      'finalCurrentTime',
      'initialDecodedFrames',
      'finalDecodedFrames',
      'videoWidth',
      'videoHeight',
      'continuedPlayback',
    ]) {
      expect(source).toContain(field);
    }
    expect(source).toContain('getVideoPlaybackQuality');
    expect(source).toContain('webkitDecodedFrameCount');
    expect(source).toContain('profile.rooms.every((room) => room.continuedPlayback)');
  });
});
