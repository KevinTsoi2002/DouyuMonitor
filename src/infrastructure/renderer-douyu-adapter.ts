import type { DouyuAdapter } from '../domain/douyu-adapter';
import type { AppApi } from '../preload/bridge';
import { createMockDouyuAdapter } from './mock-douyu-adapter';

function getAppApi(): AppApi | undefined {
  if (typeof window === 'undefined') return undefined;
  return window.appApi;
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

    async getStreamAvailability(roomId) {
      const result = await appApi.getStreamAvailability(roomId);
      if (!result.ok) throw new Error(result.error.message);
      return result.data;
    },
  };
}
