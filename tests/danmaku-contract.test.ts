import { describe, expect, it } from 'vitest';
import {
  isDanmakuEvent,
  isValidDanmakuRoomRequest,
} from '../src/shared/danmaku-contract';

const validMessage = {
  id: 'cid-1',
  roomId: '63136',
  nickname: '测试用户',
  text: '你好，斗鱼！',
  receivedAt: '2026-08-07T00:00:00.000Z',
};

describe('danmaku contract', () => {
  it('accepts a normalized messages event', () => {
    expect(
      isDanmakuEvent({
        type: 'messages',
        roomId: '63136',
        messages: [validMessage],
        dropped: 0,
      }),
    ).toBe(true);
  });

  it('accepts a reconnecting status event', () => {
    expect(
      isDanmakuEvent({
        type: 'status',
        status: { roomId: '63136', state: 'reconnecting', attempt: 2 },
      }),
    ).toBe(true);
  });

  it('rejects null', () => {
    expect(isDanmakuEvent(null)).toBe(false);
  });

  it('rejects a messages event with a non-numeric room ID', () => {
    expect(
      isDanmakuEvent({
        type: 'messages',
        roomId: 'abc',
        messages: [validMessage],
        dropped: 0,
      }),
    ).toBe(false);
  });

  it('rejects a message whose room ID differs from the batch room ID', () => {
    expect(
      isDanmakuEvent({
        type: 'messages',
        roomId: '63136',
        messages: [{ ...validMessage, roomId: '12345' }],
        dropped: 0,
      }),
    ).toBe(false);
  });

  it('rejects a negative status attempt', () => {
    expect(
      isDanmakuEvent({
        type: 'status',
        status: { roomId: '63136', state: 'reconnecting', attempt: -1 },
      }),
    ).toBe(false);
  });

  it('validates one-to-twenty digit room requests', () => {
    expect(isValidDanmakuRoomRequest({ roomId: '63136' })).toBe(true);
    expect(isValidDanmakuRoomRequest({ roomId: '' })).toBe(false);
    expect(isValidDanmakuRoomRequest({ roomId: 'abc' })).toBe(false);
    expect(isValidDanmakuRoomRequest({ roomId: '1'.repeat(21) })).toBe(false);
    expect(isValidDanmakuRoomRequest(null)).toBe(false);
  });
});
