import { describe, expect, it } from 'vitest';
import { resolveRoomInput } from '../src/domain/input-resolver';

describe('resolveRoomInput', () => {
  it('recognizes a numeric room id', () => {
    expect(resolveRoomInput(' 123456 ')).toEqual({ type: 'room-id', value: '123456' });
  });

  it('recognizes a Douyu room URL', () => {
    expect(resolveRoomInput('https://www.douyu.com/123456')).toEqual({
      type: 'room-id',
      value: '123456',
    });
  });

  it('treats non-numeric input as an anchor search', () => {
    expect(resolveRoomInput('主播小明')).toEqual({ type: 'anchor-name', value: '主播小明' });
  });

  it('rejects blank input', () => {
    expect(() => resolveRoomInput('  ')).toThrow('请输入直播间号或主播名字');
  });
});
