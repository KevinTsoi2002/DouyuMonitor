import { contextBridge, ipcRenderer } from 'electron';
import { createAppApi } from './bridge';

contextBridge.exposeInMainWorld('appApi', createAppApi({
  invoke: (channel, payload) => ipcRenderer.invoke(channel, payload),
  on: (channel, listener) => ipcRenderer.on(channel, listener),
  removeListener: (channel, listener) => ipcRenderer.removeListener(channel, listener),
}));
