import { app, BrowserWindow, ipcMain, type WebContents } from 'electron';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { createDouyuDanmakuClient } from '../infrastructure/douyu-danmaku/client';
import { createDouyuHttpAdapter } from '../infrastructure/douyu-http-adapter';
import { createStreamgetDouyuAdapter } from '../infrastructure/streamget-douyu-adapter';
import {
  createDanmakuSessionManager,
  type DanmakuSessionManager,
} from './danmaku-session-manager';
import { registerIpcHandlers } from './ipc-handlers';
import { createBrowserWindowOptions, getRendererLoadTarget } from './main-config';
import { createStreamgetBridge } from './streamget-bridge';
import {
  registerWindowControlHandlers,
  wireMaximizedNotifications,
} from './window-controls';

const currentDir = dirname(fileURLToPath(import.meta.url));
const rendererIndexPath = join(currentDir, '../renderer/index.html');
const preloadPath = join(currentDir, '../preload/preload.cjs');

let mainWindow: BrowserWindow | null = null;

function createMainWindow(danmakuManager: DanmakuSessionManager): BrowserWindow {
  const window = new BrowserWindow(createBrowserWindowOptions(preloadPath));
  wireMaximizedNotifications(window);
  const ownerId = window.webContents.id;
  window.webContents.setWindowOpenHandler(() => ({ action: 'deny' }));

  const target = getRendererLoadTarget({
    devServerUrl: process.env.VITE_DEV_SERVER_URL,
    filePath: rendererIndexPath,
  });
  const load = target.kind === 'url' ? window.loadURL(target.value) : window.loadFile(target.value);
  void load.catch(() => {
    // Keep startup errors inside the window lifecycle; renderer diagnostics remain local.
  });

  window.on('closed', () => {
    danmakuManager.stopOwner(ownerId);
    if (mainWindow === window) mainWindow = null;
  });
  return window;
}

async function bootstrap(): Promise<void> {
  await app.whenReady();
  const danmakuManager = createDanmakuSessionManager((roomId, emit) =>
    createDouyuDanmakuClient(roomId, emit),
  );
  const streamgetAdapter = createStreamgetDouyuAdapter(
    createDouyuHttpAdapter(),
    createStreamgetBridge({
      isPackaged: app.isPackaged,
      resourcesPath: process.resourcesPath,
    }),
  );
  registerIpcHandlers(ipcMain, streamgetAdapter, danmakuManager);
  registerWindowControlHandlers(ipcMain, (sender) => (
    BrowserWindow.fromWebContents(sender as WebContents) ?? undefined
  ));
  mainWindow = createMainWindow(danmakuManager);

  app.on('before-quit', () => {
    danmakuManager.stopAll();
  });

  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      mainWindow = createMainWindow(danmakuManager);
    }
  });
}

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});

void bootstrap();
