import { readFileSync } from 'node:fs';
import { describe, expect, it } from 'vitest';
import {
  createStreamgetBridge,
  findStreamgetPython,
  parseStreamgetResponse,
  resolveStreamgetLaunch,
} from '../src/main/streamget-bridge';

describe('streamget bridge', () => {
  it.each([
    ['http', 'live.douyucdn.cn'],
    ['https', 'live.douyucdn.cn'],
    ['http', 'live.douyucdn2.cn'],
    ['https', 'live.douyucdn2.cn'],
    ['http', 'live.edgesrv.com'],
    ['https', 'live.edgesrv.com'],
  ])('accepts %s FLV URLs from %s', (protocol, hostname) => {
    const flvUrl = `${protocol}://${hostname}/live/63136.flv`;

    expect(parseStreamgetResponse('63136', JSON.stringify({
      roomId: '63136',
      isLive: true,
      flvUrl,
    }), 'auto')).toEqual({
      roomId: '63136',
      isLive: true,
      flvUrl,
      resolvedQuality: 'auto',
      source: 'app-fallback',
    });
  });

  it('accepts a live FLV response from an allowed Douyu host', async () => {
    const bridge = createStreamgetBridge({
      run: async () => JSON.stringify({
        roomId: '63136',
        isLive: true,
        resolvedQuality: '720p',
        source: 'web-h5',
        flvUrl: 'https://openflv-hw.douyucdn2.cn/live/63136_demo.flv?wsAuth=redacted',
      }),
    });

    await expect(bridge.resolve('63136', '720p')).resolves.toEqual({
      roomId: '63136',
      isLive: true,
      resolvedQuality: '720p',
      source: 'web-h5',
      flvUrl: 'https://openflv-hw.douyucdn2.cn/live/63136_demo.flv?wsAuth=redacted',
    });
  });

  it('rejects stream URLs outside the Douyu CDN allowlist', async () => {
    const bridge = createStreamgetBridge({
      run: async () => JSON.stringify({
      roomId: '63136',
      isLive: true,
      resolvedQuality: '720p',
      source: 'web-h5',
      flvUrl: 'https://example.invalid/live/63136.flv',
      }),
    });

    await expect(bridge.resolve('63136')).rejects.toMatchObject({
      code: 'UNSAFE_STREAM_URL',
    });
  });

  it.each([
    'https://live.douyucdn.cn.attacker.invalid/live/63136.flv',
    'https://notdouyucdn.cn/live/63136.flv',
  ])('rejects a host that spoofs an allowed suffix', (flvUrl) => {
    expect(() => parseStreamgetResponse('63136', JSON.stringify({
      roomId: '63136',
      isLive: true,
      resolvedQuality: '720p',
      source: 'web-h5',
      flvUrl,
    }), '720p')).toThrowError(expect.objectContaining({ code: 'UNSAFE_STREAM_URL' }));
  });

  it('rejects a non-HTTP stream URL', () => {
    expect(() => parseStreamgetResponse('63136', JSON.stringify({
      roomId: '63136',
      isLive: true,
      resolvedQuality: '720p',
      source: 'web-h5',
      flvUrl: 'ftp://live.douyucdn.cn/live/63136.flv',
    }), '720p')).toThrowError(expect.objectContaining({ code: 'UNSAFE_STREAM_URL' }));
  });

  it('preserves an offline response without requiring a URL', () => {
    expect(parseStreamgetResponse('63136', JSON.stringify({
      roomId: '63136',
      isLive: false,
    }))).toEqual({ roomId: '63136', isLive: false });
  });

  it('rejects malformed sidecar output', () => {
    expect(() => parseStreamgetResponse('63136', '{"roomId":"63136"}'))
      .toThrowError(expect.objectContaining({ code: 'INVALID_RESPONSE' }));
  });

  it('prefers a project-local Python environment', () => {
    const command = findStreamgetPython({
      cwd: 'C:\\DouyuMonitor',
      platform: 'win32',
      exists: (candidate) => candidate.endsWith('\\.venv\\Scripts\\python.exe'),
    });

    expect(command).toBe('C:\\DouyuMonitor\\.venv\\Scripts\\python.exe');
  });

  it('launches the Python script from the project during development', () => {
    expect(resolveStreamgetLaunch('63136', '720p', {
      cwd: 'C:\\DouyuMonitor',
      platform: 'win32',
      exists: (candidate) => candidate.endsWith('\\.venv\\Scripts\\python.exe'),
    })).toEqual({
      command: 'C:\\DouyuMonitor\\.venv\\Scripts\\python.exe',
      args: ['C:\\DouyuMonitor\\scripts\\streamget_bridge.py', '63136', '720p'],
      cwd: 'C:\\DouyuMonitor',
    });
  });

  it('launches only the bundled sidecar executable after packaging', () => {
    expect(resolveStreamgetLaunch('63136', 'original', {
      isPackaged: true,
      resourcesPath: 'C:\\Program Files\\DouyuMonitor\\resources',
      platform: 'win32',
    })).toEqual({
      command: 'C:\\Program Files\\DouyuMonitor\\resources\\streamget\\streamget_bridge.exe',
      args: ['63136', 'original'],
      cwd: 'C:\\Program Files\\DouyuMonitor\\resources\\streamget',
    });
  });

  it('ignores development command overrides after packaging', () => {
    expect(resolveStreamgetLaunch('63136', 'original', {
      command: 'python',
      isPackaged: true,
      resourcesPath: 'C:\\Program Files\\DouyuMonitor\\resources',
      platform: 'win32',
    })).toEqual({
      command: 'C:\\Program Files\\DouyuMonitor\\resources\\streamget\\streamget_bridge.exe',
      args: ['63136', 'original'],
      cwd: 'C:\\Program Files\\DouyuMonitor\\resources\\streamget',
    });
  });

  it('keeps the Python sidecar on the app stream-data path', () => {
    const source = readFileSync(
      new URL('../scripts/streamget_bridge.py', import.meta.url),
      'utf8',
    );

    expect(source).toContain('fetch_web_stream_data');
    expect(source).toContain('fetch_stream_url');
    expect(source).toContain('fetch_app_stream_data');
    expect(source).toContain('"720p": "HD"');
  });
});
