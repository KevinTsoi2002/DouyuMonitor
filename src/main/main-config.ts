export interface RendererTargetInput {
  devServerUrl?: string;
  filePath: string;
}

export type RendererLoadTarget = { kind: 'url' | 'file'; value: string };

export interface SecureBrowserWindowOptions {
  width: number;
  height: number;
  minWidth: number;
  minHeight: number;
  backgroundColor: string;
  frame: false;
  autoHideMenuBar: true;
  webPreferences: {
    preload: string;
    contextIsolation: true;
    nodeIntegration: false;
    sandbox: true;
  };
}

export function getRendererLoadTarget(input: RendererTargetInput): RendererLoadTarget {
  const devServerUrl = input.devServerUrl?.trim();
  return devServerUrl ? { kind: 'url', value: devServerUrl } : { kind: 'file', value: input.filePath };
}

export function createBrowserWindowOptions(preloadPath: string): SecureBrowserWindowOptions {
  return {
    width: 1440,
    height: 900,
    minWidth: 960,
    minHeight: 620,
    backgroundColor: '#0b0e12',
    frame: false,
    autoHideMenuBar: true,
    webPreferences: {
      preload: preloadPath,
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
    },
  };
}
