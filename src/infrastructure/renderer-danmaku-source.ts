import type { AppApi } from '../preload/bridge';
import type { DanmakuEvent } from '../shared/danmaku-contract';

export interface DanmakuSource {
  start(roomId: string): Promise<void>;
  stop(roomId: string): Promise<void>;
  subscribe(listener: (event: DanmakuEvent) => void): () => void;
}

type DanmakuAppApi = Pick<
  AppApi,
  'startDanmaku' | 'stopDanmaku' | 'onDanmakuEvent'
>;

export function createRendererDanmakuSource(api: DanmakuAppApi): DanmakuSource {
  return {
    async start(roomId) {
      const result = await api.startDanmaku(roomId);
      if (!result.ok) throw new Error(result.error.message);
    },
    async stop(roomId) {
      const result = await api.stopDanmaku(roomId);
      if (!result.ok) throw new Error(result.error.message);
    },
    subscribe(listener) {
      return api.onDanmakuEvent(listener);
    },
  };
}
