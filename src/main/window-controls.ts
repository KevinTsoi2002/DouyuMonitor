import { IPC_CHANNELS } from '../shared/ipc-contract';
import type { DanmakuEventTarget } from './danmaku-session-manager';
import type { IpcMainLike } from './ipc-handlers';

export interface WindowControlTarget {
  minimize(): void;
  maximize(): void;
  unmaximize(): void;
  close(): void;
  isMaximized(): boolean;
}

export interface MaximizeNotificationTarget {
  isMaximized(): boolean;
  on(event: 'maximize' | 'unmaximize', listener: () => void): unknown;
  webContents: {
    send(channel: string, payload: unknown): void;
  };
}

export function registerWindowControlHandlers(
  ipcMain: IpcMainLike,
  resolveWindow: (sender: DanmakuEventTarget) => WindowControlTarget | undefined,
): void {
  ipcMain.handle(IPC_CHANNELS.windowMinimize, async (event) => {
    resolveWindow(event.sender)?.minimize();
  });
  ipcMain.handle(IPC_CHANNELS.windowToggleMaximize, async (event) => {
    const window = resolveWindow(event.sender);
    if (!window) return;
    if (window.isMaximized()) window.unmaximize();
    else window.maximize();
  });
  ipcMain.handle(IPC_CHANNELS.windowClose, async (event) => {
    resolveWindow(event.sender)?.close();
  });
}

export function wireMaximizedNotifications(window: MaximizeNotificationTarget): void {
  const notify = () => {
    window.webContents.send(
      IPC_CHANNELS.windowMaximizedChanged,
      window.isMaximized(),
    );
  };
  window.on('maximize', notify);
  window.on('unmaximize', notify);
}
