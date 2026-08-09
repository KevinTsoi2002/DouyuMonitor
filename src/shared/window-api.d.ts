import type { AppApi } from '../preload/bridge';

declare global {
  interface Window {
    appApi?: AppApi;
  }
}

export {};
