import { readFileSync } from 'node:fs';
import { describe, expect, it } from 'vitest';
import { createBrowserWindowOptions, getRendererLoadTarget } from '../src/main/main-config';

describe('Electron main configuration', () => {
  it('loads the dev renderer only when a URL is explicitly provided', () => {
    expect(getRendererLoadTarget({ devServerUrl: 'http://127.0.0.1:4173', filePath: 'dist/renderer/index.html' })).toEqual({
      kind: 'url',
      value: 'http://127.0.0.1:4173',
    });
    expect(getRendererLoadTarget({ filePath: 'dist/renderer/index.html' })).toEqual({
      kind: 'file',
      value: 'dist/renderer/index.html',
    });
  });

  it('returns a secure BrowserWindow option set', () => {
    const options = createBrowserWindowOptions('/tmp/preload.js');

    expect(options.webPreferences).toEqual(expect.objectContaining({
      preload: '/tmp/preload.js',
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
    }));
    expect(options.minWidth).toBeGreaterThanOrEqual(960);
    expect(options.minHeight).toBeGreaterThanOrEqual(620);
    expect(options.frame).toBe(false);
    expect(options.autoHideMenuBar).toBe(true);
  });

  it('wires the production Douyu adapter into the Electron entry', () => {
    const source = readFileSync(
      new URL('../src/main/main.ts', import.meta.url),
      'utf8',
    );

    expect(source).toContain('createDouyuHttpAdapter');
    expect(source).toContain('createDanmakuSessionManager');
    expect(source).toContain('createDouyuDanmakuClient');
    expect(source).toContain('createStreamgetDouyuAdapter');
    expect(source).toContain('createStreamgetBridge');
    expect(source).toContain('registerIpcHandlers(');
    expect(source).toContain('streamgetAdapter,');
    expect(source).toContain('danmakuManager,');
    expect(source).toContain('createSystemNotificationService()');
    expect(source).toContain('danmakuManager.stopOwner(ownerId)');
    expect(source).toContain('danmakuManager.stopAll()');
    expect(source).not.toContain('createMockDouyuAdapter');
  });
});
