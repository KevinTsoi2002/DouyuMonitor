import { describe, expect, it, vi } from 'vitest';
import { IPC_CHANNELS } from '../src/shared/ipc-contract';
import {
  registerWindowControlHandlers,
  wireMaximizedNotifications,
  type WindowControlTarget,
} from '../src/main/window-controls';
import type { IpcMainLike } from '../src/main/ipc-handlers';

function createFakeIpcMain() {
  const handlers = new Map<string, (event: { sender: { id: number } }) => Promise<unknown>>();
  const ipcMain: IpcMainLike = {
    handle(channel, listener) {
      handlers.set(channel, listener as (event: { sender: { id: number } }) => Promise<unknown>);
    },
  };
  return { ipcMain, handlers };
}

function createTarget(maximized = false): WindowControlTarget {
  return {
    minimize: vi.fn(),
    maximize: vi.fn(),
    unmaximize: vi.fn(),
    close: vi.fn(),
    isMaximized: vi.fn(() => maximized),
  };
}

describe('window controls', () => {
  it('operates only on the window resolved from the IPC sender', async () => {
    const target = createTarget();
    const { ipcMain, handlers } = createFakeIpcMain();
    registerWindowControlHandlers(ipcMain, (sender) => sender.id === 7 ? target : undefined);

    await handlers.get(IPC_CHANNELS.windowMinimize)?.({ sender: { id: 7 } });
    await handlers.get(IPC_CHANNELS.windowClose)?.({ sender: { id: 8 } });

    expect(target.minimize).toHaveBeenCalledOnce();
    expect(target.close).not.toHaveBeenCalled();
  });

  it('toggles maximize and restore on the sender window', async () => {
    const normal = createTarget(false);
    const maximized = createTarget(true);
    const { ipcMain, handlers } = createFakeIpcMain();
    registerWindowControlHandlers(ipcMain, (sender) => sender.id === 1 ? normal : maximized);

    await handlers.get(IPC_CHANNELS.windowToggleMaximize)?.({ sender: { id: 1 } });
    await handlers.get(IPC_CHANNELS.windowToggleMaximize)?.({ sender: { id: 2 } });

    expect(normal.maximize).toHaveBeenCalledOnce();
    expect(maximized.unmaximize).toHaveBeenCalledOnce();
  });

  it('notifies the renderer when maximize state changes', () => {
    const listeners = new Map<string, () => void>();
    let maximized = false;
    const send = vi.fn();
    wireMaximizedNotifications({
      isMaximized: () => maximized,
      on: (event, listener) => { listeners.set(event, listener); },
      webContents: { send },
    });

    maximized = true;
    listeners.get('maximize')?.();
    maximized = false;
    listeners.get('unmaximize')?.();

    expect(send).toHaveBeenNthCalledWith(1, IPC_CHANNELS.windowMaximizedChanged, true);
    expect(send).toHaveBeenNthCalledWith(2, IPC_CHANNELS.windowMaximizedChanged, false);
  });
});
