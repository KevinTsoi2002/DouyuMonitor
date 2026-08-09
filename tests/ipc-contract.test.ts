import { describe, expect, it } from 'vitest';
import { DouyuAdapterError } from '../src/domain/douyu-adapter';
import {
  IPC_CHANNELS,
  isValidGetStreamAvailabilityRequest,
  isValidSearchRoomsRequest,
  ok,
  toIpcError,
  type SearchRoomsRequest,
} from '../src/shared/ipc-contract';

describe('IPC contract', () => {
  it('keeps channel names stable and namespaced', () => {
    expect(IPC_CHANNELS.searchRooms).toBe('rooms.search');
    expect(IPC_CHANNELS.getStreamAvailability).toBe('playback.getAvailability');
    expect(IPC_CHANNELS.ping).toBe('app.ping');
    expect(IPC_CHANNELS.startDanmaku).toBe('danmaku.start');
    expect(IPC_CHANNELS.stopDanmaku).toBe('danmaku.stop');
    expect(IPC_CHANNELS.danmakuEvent).toBe('danmaku.event');
    expect(IPC_CHANNELS.windowMinimize).toBe('window.minimize');
    expect(IPC_CHANNELS.windowToggleMaximize).toBe('window.toggleMaximize');
    expect(IPC_CHANNELS.windowClose).toBe('window.close');
    expect(IPC_CHANNELS.windowMaximizedChanged).toBe('window.maximizedChanged');
    expect(Object.values(IPC_CHANNELS).every((channel) => channel.includes('.'))).toBe(true);
  });

  it('accepts only one-to-twenty digit availability requests', () => {
    expect(isValidGetStreamAvailabilityRequest({ roomId: '63136' })).toBe(true);
    expect(isValidGetStreamAvailabilityRequest({ roomId: '' })).toBe(false);
    expect(isValidGetStreamAvailabilityRequest({ roomId: 'abc' })).toBe(false);
    expect(isValidGetStreamAvailabilityRequest({ roomId: '1'.repeat(21) })).toBe(false);
    expect(isValidGetStreamAvailabilityRequest(null)).toBe(false);
  });

  it('accepts only bounded non-empty search requests', () => {
    const request: SearchRoomsRequest = { input: ' 63136 ' };

    expect(isValidSearchRoomsRequest(request)).toBe(true);
    expect(isValidSearchRoomsRequest({ input: '   ' })).toBe(false);
    expect(isValidSearchRoomsRequest({ input: 'a'.repeat(201) })).toBe(false);
    expect(isValidSearchRoomsRequest({ input: 63136 })).toBe(false);
    expect(isValidSearchRoomsRequest(null)).toBe(false);
  });

  it('creates a typed success envelope', () => {
    expect(ok({ roomId: '63136' })).toEqual({ ok: true, data: { roomId: '63136' } });
  });

  it('maps unknown errors to safe user-facing data', () => {
    const result = toIpcError(new Error('Authorization cookie=secret-token'));

    expect(result).toEqual({
      code: 'UNKNOWN',
      message: '操作失败，请稍后重试',
      retryable: true,
    });
    expect(JSON.stringify(result)).not.toContain('secret-token');
  });

  it.each([
    ['ROOM_NOT_FOUND', '未找到对应直播间', false],
    ['NETWORK_UNAVAILABLE', '无法连接斗鱼，请检查网络后重试', true],
    ['PROTOCOL_CHANGED', '斗鱼接口响应异常，请稍后重试', true],
  ] as const)('maps %s without exposing details', (code, message, retryable) => {
    const result = toIpcError(
      new DouyuAdapterError(code, 'response contained token=secret-token'),
    );

    expect(result).toEqual({ code, message, retryable });
    expect(JSON.stringify(result)).not.toContain('secret-token');
  });
});
