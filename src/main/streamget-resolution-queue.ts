import type { StreamRequestQuality } from '../domain/douyu-adapter';
import type { StreamgetRawResult } from './streamget-bridge';

export type StreamgetResolver = (
  roomId: string,
  quality: StreamRequestQuality,
) => Promise<StreamgetRawResult>;

export interface StreamgetResolutionQueue {
  resolve(roomId: string, quality: StreamRequestQuality): Promise<StreamgetRawResult>;
  cancel(roomId: string): void;
  cancelAll(): void;
}

export interface StreamgetResolutionQueueOptions {
  concurrency?: number;
}

export class StreamgetResolutionCancelledError extends Error {
  readonly code = 'RESOLUTION_CANCELLED';

  constructor() {
    super('StreamGet resolution cancelled');
    this.name = 'StreamgetResolutionCancelledError';
  }
}

interface QueueJob {
  roomId: string;
  key: string;
  run: () => Promise<StreamgetRawResult>;
  resolve: (value: StreamgetRawResult) => void;
  reject: (error: unknown) => void;
  cancelled: boolean;
}

export function createStreamgetResolutionQueue(
  resolver: StreamgetResolver,
  options: StreamgetResolutionQueueOptions = {},
): StreamgetResolutionQueue {
  const concurrency = options.concurrency ?? 2;
  if (!Number.isInteger(concurrency) || concurrency < 1) {
    throw new RangeError('StreamGet resolution concurrency must be a positive integer');
  }

  const pending: QueueJob[] = [];
  const jobsByKey = new Map<string, Promise<StreamgetRawResult>>();
  let active = 0;

  const pump = (): void => {
    while (active < concurrency && pending.length > 0) {
      const job = pending.shift()!;
      if (job.cancelled) continue;
      active += 1;
      void Promise.resolve()
        .then(job.run)
        .then(job.resolve, job.reject)
        .finally(() => {
          active -= 1;
          jobsByKey.delete(job.key);
          pump();
        });
    }
  };

  const resolve = (roomId: string, quality: StreamRequestQuality): Promise<StreamgetRawResult> => {
    const key = `${roomId}:${quality}`;
    const existing = jobsByKey.get(key);
    if (existing) return existing;

    let resolveJob!: (value: StreamgetRawResult) => void;
    let rejectJob!: (error: unknown) => void;
    const promise = new Promise<StreamgetRawResult>((resolvePromise, rejectPromise) => {
      resolveJob = resolvePromise;
      rejectJob = rejectPromise;
    });
    jobsByKey.set(key, promise);
    pending.push({
      roomId,
      key,
      run: () => resolver(roomId, quality),
      resolve: resolveJob,
      reject: rejectJob,
      cancelled: false,
    });
    pump();
    return promise;
  };

  const cancel = (roomId: string): void => {
    for (const job of pending) {
      if (job.roomId !== roomId || job.cancelled) continue;
      job.cancelled = true;
      jobsByKey.delete(job.key);
      job.reject(new StreamgetResolutionCancelledError());
    }
  };

  const cancelAll = (): void => {
    for (const job of pending) {
      if (job.cancelled) continue;
      job.cancelled = true;
      jobsByKey.delete(job.key);
      job.reject(new StreamgetResolutionCancelledError());
    }
  };

  return { resolve, cancel, cancelAll };
}
