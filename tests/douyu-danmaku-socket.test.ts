import { Buffer } from 'node:buffer';
import { describe, expect, it } from 'vitest';
import { rawDataToArrayBuffer } from '../src/infrastructure/douyu-danmaku/socket';

describe('ws transport normalization', () => {
  it('copies Buffer and fragmented Buffer data into exact ArrayBuffers', () => {
    expect([...new Uint8Array(rawDataToArrayBuffer(Buffer.from([1, 2, 3])))]).toEqual([
      1,
      2,
      3,
    ]);
    expect([
      ...new Uint8Array(
        rawDataToArrayBuffer([Buffer.from([4, 5]), Buffer.from([6])]),
      ),
    ]).toEqual([4, 5, 6]);
  });
});
