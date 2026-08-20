import {
  DouyuAdapterError,
  type DouyuAdapter,
  type DouyuAdapterErrorCode,
  type StreamRequestQuality,
} from '../domain/douyu-adapter';
import type { AppApi } from '../preload/bridge';
import { createMockDouyuAdapter } from './mock-douyu-adapter';

function getAppApi(): AppApi | undefined {
  if (typeof window === 'undefined') return undefined;
  return window.appApi;
}

const ADAPTER_ERROR_CODES = new Set<DouyuAdapterErrorCode>([
  'ROOM_NOT_FOUND',
  'NETWORK_UNAVAILABLE',
  'PROTOCOL_CHANGED',
  'STREAMGET_UNAVAILABLE',
  'LOCAL_STREAM_PROXY_FAILED',
]);

function toAdapterError(error: { code: string; message: string }): Error {
  if (ADAPTER_ERROR_CODES.has(error.code as DouyuAdapterErrorCode)) {
    return new DouyuAdapterError(error.code as DouyuAdapterErrorCode, error.message);
  }
  return new Error(error.message);
}

export function createRendererDouyuAdapter(): DouyuAdapter {
  const appApi = getAppApi();
  if (!appApi) return createMockDouyuAdapter();

  return {
    async search(input) {
      const result = await appApi.searchRooms(input.value);
      if (!result.ok) throw new Error(result.error.message);
      return result.data;
    },

    async getStreamAvailability(roomId, quality: StreamRequestQuality = 'auto') {
      const result = await appApi.getStreamAvailability(roomId, quality);
      if (!result.ok) throw toAdapterError(result.error);
      return result.data;
    },

    async releaseStream(roomId) {
      const result = await appApi.releaseStreamProxy(roomId);
      if (!result.ok) throw toAdapterError(result.error);
    },
  };
}
