import type {
  GetStreamAvailabilityResult,
  IpcResult,
  SearchRoomsResult,
  SystemNotificationSupportResult,
} from '../shared/ipc-contract';
import { isDanmakuEvent, type DanmakuEvent } from '../shared/danmaku-contract';
import { IPC_CHANNELS } from '../shared/ipc-contract';

export interface IpcRendererLike {
  invoke(channel: string, payload?: unknown): Promise<unknown>;
  on(channel: string, listener: (event: unknown, payload: unknown) => void): void;
  removeListener(
    channel: string,
    listener: (event: unknown, payload: unknown) => void,
  ): void;
}

export interface AppApi {
  searchRooms(input: string): Promise<SearchRoomsResult>;
  getStreamAvailability(roomId: string): Promise<GetStreamAvailabilityResult>;
  startDanmaku(roomId: string): Promise<IpcResult<void>>;
  stopDanmaku(roomId: string): Promise<IpcResult<void>>;
  onDanmakuEvent(listener: (event: DanmakuEvent) => void): () => void;
  minimizeWindow(): Promise<void>;
  toggleMaximizeWindow(): Promise<void>;
  closeWindow(): Promise<void>;
  onMaximizedChanged(listener: (maximized: boolean) => void): () => void;
  getSystemNotificationSupport(): Promise<SystemNotificationSupportResult>;
  showSystemNotification(input: { title: string; body: string }): Promise<IpcResult<void>>;
  ping(): Promise<IpcResult<{ status: 'ok' }>>;
}

export function createAppApi(ipcRenderer: IpcRendererLike): AppApi {
  return {
    searchRooms(input) {
      return ipcRenderer.invoke(IPC_CHANNELS.searchRooms, { input }) as Promise<SearchRoomsResult>;
    },
    getStreamAvailability(roomId) {
      return ipcRenderer.invoke(
        IPC_CHANNELS.getStreamAvailability,
        { roomId },
      ) as Promise<GetStreamAvailabilityResult>;
    },
    startDanmaku(roomId) {
      return ipcRenderer.invoke(IPC_CHANNELS.startDanmaku, {
        roomId,
      }) as Promise<IpcResult<void>>;
    },
    stopDanmaku(roomId) {
      return ipcRenderer.invoke(IPC_CHANNELS.stopDanmaku, {
        roomId,
      }) as Promise<IpcResult<void>>;
    },
    onDanmakuEvent(listener) {
      const wrapper = (_event: unknown, payload: unknown) => {
        if (isDanmakuEvent(payload)) listener(payload);
      };
      ipcRenderer.on(IPC_CHANNELS.danmakuEvent, wrapper);
      return () => ipcRenderer.removeListener(IPC_CHANNELS.danmakuEvent, wrapper);
    },
    minimizeWindow() {
      return ipcRenderer.invoke(IPC_CHANNELS.windowMinimize) as Promise<void>;
    },
    toggleMaximizeWindow() {
      return ipcRenderer.invoke(IPC_CHANNELS.windowToggleMaximize) as Promise<void>;
    },
    closeWindow() {
      return ipcRenderer.invoke(IPC_CHANNELS.windowClose) as Promise<void>;
    },
    onMaximizedChanged(listener) {
      const wrapper = (_event: unknown, payload: unknown) => {
        if (typeof payload === 'boolean') listener(payload);
      };
      ipcRenderer.on(IPC_CHANNELS.windowMaximizedChanged, wrapper);
      return () => ipcRenderer.removeListener(IPC_CHANNELS.windowMaximizedChanged, wrapper);
    },
    getSystemNotificationSupport() {
      return ipcRenderer.invoke(
        IPC_CHANNELS.getSystemNotificationSupport,
      ) as Promise<SystemNotificationSupportResult>;
    },
    showSystemNotification(input) {
      return ipcRenderer.invoke(
        IPC_CHANNELS.showSystemNotification,
        input,
      ) as Promise<IpcResult<void>>;
    },
    ping() {
      return ipcRenderer.invoke(IPC_CHANNELS.ping) as Promise<IpcResult<{ status: 'ok' }>>;
    },
  };
}
