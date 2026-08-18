import { describe, expect, it, vi } from 'vitest';
import type { StreamRequestQuality } from '../src/domain/douyu-adapter';
import type { StreamgetRawResult } from '../src/main/streamget-bridge';
import { createStreamgetResolutionQueue } from '../src/main/streamget-resolution-queue';

function liveResult(roomId: string): StreamgetRawResult {
  return {
    roomId,
    isLive: true,
    flvUrl: `https://openflv-hw.douyucdn2.cn/live/${roomId}.flv`,
  };
}

function deferred<T>() {
  let resolve!: (value: T) => void;
  let reject!: (error: unknown) => void;
  const promise = new Promise<T>((resolvePromise, rejectPromise) => {
    resolve = resolvePromise;
    reject = rejectPromise;
  });
  return { promise, resolve, reject };
}

describe('StreamGet resolution queue', () => {
  it('runs at most two sidecars and starts the next job after a rejection', async () => {
    const gates = new Map<string, ReturnType<typeof deferred<StreamgetRawResult>>>();
    let active = 0;
    let peak = 0;
    const queue = createStreamgetResolutionQueue(async (roomId, quality) => {
      active += 1;
      peak = Math.max(peak, active);
      const gate = deferred<StreamgetRawResult>();
      gates.set(`${roomId}:${quality}`, gate);
      try {
        return await gate.promise;
      } finally {
        active -= 1;
      }
    }, { concurrency: 2 });

    const first = queue.resolve('1', 'original');
    const second = queue.resolve('2', '720p');
    const third = queue.resolve('3', '720p');
    await Promise.resolve();
    expect(peak).toBe(2);
    expect(gates.has('3:720p')).toBe(false);

    gates.get('1:original')!.reject(new Error('failed'));
    await expect(first).rejects.toThrow('failed');
    await vi.waitFor(() => expect(gates.has('3:720p')).toBe(true));
    gates.get('2:720p')!.resolve(liveResult('2'));
    gates.get('3:720p')!.resolve(liveResult('3'));
    await expect(Promise.all([second, third])).resolves.toHaveLength(2);
  });

  it('coalesces matching room and quality requests but not different qualities', async () => {
    const resolver = vi.fn(async (roomId: string, _quality: StreamRequestQuality) => liveResult(roomId));
    const queue = createStreamgetResolutionQueue(resolver);

    await Promise.all([
      queue.resolve('1', '720p'),
      queue.resolve('1', '720p'),
      queue.resolve('1', 'original'),
    ]);

    expect(resolver).toHaveBeenCalledTimes(2);
  });

  it('rejects queued work for a removed room without cancelling active work', async () => {
    const gate = deferred<StreamgetRawResult>();
    const resolver = vi.fn(async (roomId: string) => {
      if (roomId === '1') return gate.promise;
      return liveResult(roomId);
    });
    const queue = createStreamgetResolutionQueue(resolver, { concurrency: 1 });
    const active = queue.resolve('1', 'original');
    const queued = queue.resolve('2', '720p');

    queue.cancel('2');
    await expect(queued).rejects.toMatchObject({ code: 'RESOLUTION_CANCELLED' });
    gate.resolve(liveResult('1'));
    await expect(active).resolves.toMatchObject({ roomId: '1' });
    expect(resolver).toHaveBeenCalledTimes(1);
  });
});
