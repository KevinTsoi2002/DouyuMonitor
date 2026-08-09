import { describe, expect, it } from 'vitest';
import { RoomRegistry } from '../src/domain/room-registry';

describe('RoomRegistry', () => {
  it('does not add a duplicate room', () => {
    const registry = new RoomRegistry();
    registry.add({ roomId: '123', anchorName: '小明' });

    expect(registry.add({ roomId: '123', anchorName: '小明' })).toEqual({
      added: false,
      reason: 'duplicate',
    });
    expect(registry.list()).toHaveLength(1);
  });

  it('rejects the tenth room', () => {
    const registry = new RoomRegistry();
    for (let index = 1; index <= 9; index += 1) {
      registry.add({ roomId: String(index), anchorName: `主播${index}` });
    }

    expect(registry.add({ roomId: '10', anchorName: '主播10' })).toEqual({
      added: false,
      reason: 'limit',
    });
  });

  it('allows a removed room to be added again', () => {
    const registry = new RoomRegistry();
    registry.add({ roomId: '123', anchorName: '小明' });

    expect(registry.remove('123')).toBe(true);
    expect(registry.list()).toEqual([]);
    expect(registry.add({ roomId: '123', anchorName: '小明' }).added).toBe(true);
  });
});
