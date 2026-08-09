import { execFile } from 'node:child_process';
import { existsSync } from 'node:fs';
import { promisify } from 'node:util';
import { join, posix, win32 } from 'node:path';

const execFileAsync = promisify(execFile);
const ALLOWED_HOST_SUFFIXES = ['.douyucdn.cn', '.douyucdn2.cn', '.edgesrv.com'];

export type StreamgetRawResult = {
  roomId: string;
  isLive: boolean;
  flvUrl?: string;
};

export type StreamgetBridgeErrorCode =
  | 'INVALID_RESPONSE'
  | 'UNSAFE_STREAM_URL'
  | 'SIDECAR_FAILED';

export class StreamgetBridgeError extends Error {
  constructor(
    public readonly code: StreamgetBridgeErrorCode,
    message: string = code,
  ) {
    super(message);
    this.name = 'StreamgetBridgeError';
  }
}

export interface StreamgetBridge {
  resolve(roomId: string): Promise<StreamgetRawResult>;
}

export interface StreamgetBridgeOptions {
  run?: (roomId: string) => Promise<string>;
  command?: string;
  scriptPath?: string;
  cwd?: string;
  timeoutMs?: number;
  isPackaged?: boolean;
  resourcesPath?: string;
  platform?: NodeJS.Platform;
  exists?: (path: string) => boolean;
}

export interface StreamgetLaunch {
  command: string;
  args: string[];
  cwd: string;
}

export interface FindStreamgetPythonOptions {
  cwd?: string;
  platform?: NodeJS.Platform;
  explicit?: string;
  exists?: (path: string) => boolean;
}

export function findStreamgetPython(options: FindStreamgetPythonOptions = {}): string {
  if (options.explicit) return options.explicit;
  const platform = options.platform ?? process.platform;
  const cwd = options.cwd ?? process.cwd();
  const pathApi = platform === 'win32' ? win32 : posix;
  const localPython = platform === 'win32'
    ? pathApi.join(cwd, '.venv', 'Scripts', 'python.exe')
    : pathApi.join(cwd, '.venv', 'bin', 'python');
  if ((options.exists ?? existsSync)(localPython)) return localPython;
  return platform === 'win32' ? 'python' : 'python3';
}

export function resolveStreamgetLaunch(
  roomId: string,
  options: StreamgetBridgeOptions = {},
): StreamgetLaunch {
  const platform = options.platform ?? process.platform;
  const pathApi = platform === 'win32' ? win32 : posix;
  const cwd = options.cwd ?? process.cwd();

  if (options.isPackaged) {
    if (!options.resourcesPath) {
      throw new StreamgetBridgeError('SIDECAR_FAILED', 'Missing packaged resources path');
    }
    const sidecarDir = pathApi.join(options.resourcesPath, 'streamget');
    const executableName = platform === 'win32' ? 'streamget_bridge.exe' : 'streamget_bridge';
    return {
      command: pathApi.join(sidecarDir, executableName),
      args: [roomId],
      cwd: sidecarDir,
    };
  }

  return {
    command: options.command ?? findStreamgetPython({
      cwd,
      platform,
      explicit: process.env.STREAMGET_PYTHON,
      exists: options.exists,
    }),
    args: [options.scriptPath ?? pathApi.join(cwd, 'scripts', 'streamget_bridge.py'), roomId],
    cwd,
  };
}

function isAllowedHost(hostname: string): boolean {
  const normalized = hostname.toLowerCase();
  return ALLOWED_HOST_SUFFIXES.some((suffix) => normalized.endsWith(suffix));
}

function validateFlvUrl(value: unknown): string {
  if (typeof value !== 'string' || !value) {
    throw new StreamgetBridgeError('INVALID_RESPONSE');
  }

  let url: URL;
  try {
    url = new URL(value);
  } catch {
    throw new StreamgetBridgeError('UNSAFE_STREAM_URL');
  }

  if (!['http:', 'https:'].includes(url.protocol) || !isAllowedHost(url.hostname)) {
    throw new StreamgetBridgeError('UNSAFE_STREAM_URL');
  }
  return url.toString();
}

export function parseStreamgetResponse(roomId: string, output: string): StreamgetRawResult {
  let parsed: unknown;
  try {
    parsed = JSON.parse(output);
  } catch {
    throw new StreamgetBridgeError('INVALID_RESPONSE');
  }

  if (!parsed || typeof parsed !== 'object') {
    throw new StreamgetBridgeError('INVALID_RESPONSE');
  }
  const value = parsed as { roomId?: unknown; isLive?: unknown; flvUrl?: unknown };
  if (value.roomId !== roomId || typeof value.isLive !== 'boolean') {
    throw new StreamgetBridgeError('INVALID_RESPONSE');
  }
  if (!value.isLive) return { roomId, isLive: false };
  return { roomId, isLive: true, flvUrl: validateFlvUrl(value.flvUrl) };
}

function createDefaultRunner(options: StreamgetBridgeOptions): (roomId: string) => Promise<string> {
  const timeoutMs = options.timeoutMs ?? 20_000;

  return async (roomId) => {
    try {
      const launch = resolveStreamgetLaunch(roomId, options);
      const result = await execFileAsync(launch.command, launch.args, {
        cwd: launch.cwd,
        timeout: timeoutMs,
        windowsHide: true,
        maxBuffer: 128 * 1024,
      });
      return result.stdout.trim();
    } catch (error) {
      const message = error instanceof Error ? error.message : 'sidecar failed';
      throw new StreamgetBridgeError('SIDECAR_FAILED', message);
    }
  };
}

export function createStreamgetBridge(options: StreamgetBridgeOptions = {}): StreamgetBridge {
  const run = options.run ?? createDefaultRunner(options);
  return {
    async resolve(roomId) {
      const output = await run(roomId);
      return parseStreamgetResponse(roomId, output);
    },
  };
}
