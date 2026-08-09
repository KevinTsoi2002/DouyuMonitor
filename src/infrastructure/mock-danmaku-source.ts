import type { DanmakuSource } from './renderer-danmaku-source';
import type { DanmakuEvent } from '../shared/danmaku-contract';
import { getDanmakuMessages } from '../renderer/data/mock-danmaku';

const MOCK_INTERVAL_MS = 1_250;

export function createMockDanmakuSource(): DanmakuSource {
  const listeners = new Set<(event: DanmakuEvent) => void>();
  const roomTimers = new Map<string, ReturnType<typeof globalThis.setInterval>>();
  const roomSequences = new Map<string, number>();

  const emit = (event: DanmakuEvent) => {
    for (const listener of [...listeners]) listener(event);
  };

  return {
    async start(roomId) {
      if (roomTimers.has(roomId)) return;
      roomSequences.set(roomId, 0);
      emit({
        type: 'status',
        status: { roomId, state: 'connected' },
      });

      const timer = globalThis.setInterval(() => {
        const sequence = (roomSequences.get(roomId) ?? 0) + 1;
        roomSequences.set(roomId, sequence);
        const messages = getDanmakuMessages(roomId);
        const text = messages[(sequence - 1) % messages.length];
        emit({
          type: 'messages',
          roomId,
          dropped: 0,
          messages: [
            {
              id: `mock-${roomId}-${sequence}`,
              roomId,
              nickname: '\u6f14\u793a\u7528\u6237',
              text,
              receivedAt: new Date().toISOString(),
            },
          ],
        });
      }, MOCK_INTERVAL_MS);
      roomTimers.set(roomId, timer);
    },
    async stop(roomId) {
      const timer = roomTimers.get(roomId);
      if (timer !== undefined) globalThis.clearInterval(timer);
      roomTimers.delete(roomId);
      roomSequences.delete(roomId);
    },
    subscribe(listener) {
      listeners.add(listener);
      return () => listeners.delete(listener);
    },
  };
}
