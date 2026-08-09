import { describe, expect, it } from 'vitest';
import {
  DouyuFrameDecoder,
  encodeDouyuFrame,
} from '../src/infrastructure/douyu-danmaku/protocol';
import {
  parseStt,
  serializeStt,
} from '../src/infrastructure/douyu-danmaku/stt';

describe('Douyu STT', () => {
  it('round-trips escaped at-signs and slashes', () => {
    const encoded = serializeStt({ type: 'chatmsg', txt: 'A@B/C' });
    expect(encoded).toBe('type@=chatmsg/txt@=A@AB@SC/');
    expect(parseStt(encoded)).toEqual({ type: 'chatmsg', txt: 'A@B/C' });
  });

  it('splits each field at its first key separator', () => {
    expect(parseStt('type@=chatmsg/txt@=a@=b/')).toEqual({
      type: 'chatmsg',
      txt: 'a@=b',
    });
  });
});

describe('Douyu binary protocol', () => {
  it('decodes one server frame and validates its repeated length', () => {
    const frame = encodeDouyuFrame('type@=chatmsg/rid@=63136/', 690);
    const decoder = new DouyuFrameDecoder();
    expect(decoder.push(frame)).toEqual(['type@=chatmsg/rid@=63136/']);
  });

  it('preserves half a frame and emits coalesced frames in order', () => {
    const first = encodeDouyuFrame('type@=loginres/', 690);
    const second = encodeDouyuFrame('type@=chatmsg/txt@=hi/', 690);
    const joined = new Uint8Array(first.byteLength + second.byteLength);
    joined.set(first, 0);
    joined.set(second, first.byteLength);
    const decoder = new DouyuFrameDecoder();

    expect(decoder.push(joined.slice(0, 7))).toEqual([]);
    expect(decoder.push(joined.slice(7))).toEqual([
      'type@=loginres/',
      'type@=chatmsg/txt@=hi/',
    ]);
  });

  it('rejects mismatched lengths and oversized frames', () => {
    const mismatched = encodeDouyuFrame('type@=loginres/', 690);
    new DataView(mismatched.buffer).setUint32(4, 99, true);
    expect(() => new DouyuFrameDecoder().push(mismatched)).toThrow('length');

    const oversized = new Uint8Array(12);
    new DataView(oversized.buffer).setUint32(0, 1024 * 1024 + 1, true);
    expect(() => new DouyuFrameDecoder().push(oversized)).toThrow('size');
  });

  it('rejects an unexpected server protocol type', () => {
    const clientFrame = encodeDouyuFrame('type@=loginreq/', 689);
    expect(() => new DouyuFrameDecoder().push(clientFrame)).toThrow('protocol');
  });
});
