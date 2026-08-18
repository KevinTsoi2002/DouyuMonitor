import { execFile } from 'node:child_process';
import { existsSync } from 'node:fs';
import { promisify } from 'node:util';
import { join, posix, win32 } from 'node:path';
import type { StreamRequestQuality } from '../domain/douyu-adapter';
import {
  DouyuStreamUrlError,
  parseAllowedDouyuFlvUrl,
} from './douyu-stream-url';

const execFileAsync = promisify(execFile);
export type StreamgetRawResult = {
  roomId: string;
  isLive: boolean;
  flvUrl?: string;
  resolvedQuality?: StreamRequestQuality;
  source?: 'web-h5' | 'app-fallback';
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
  resolve(roomId: string, quality?: StreamRequestQuality): Promise<StreamgetRawResult>;
}

export interface StreamgetBridgeOptions {
  run?: (roomId: string, quality: StreamRequestQuality) => Promise<string>;
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
  qualityOrOptions: StreamRequestQuality | StreamgetBridgeOptions = 'auto',
  maybeOptions: StreamgetBridgeOptions = {},
): StreamgetLaunch {
  const quality = typeof qualityOrOptions === 'string' ? qualityOrOptions : 'auto';
  const options = typeof qualityOrOptions === 'string' ? maybeOptions : qualityOrOptions;
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
      args: [roomId, quality],
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
    args: [options.scriptPath ?? pathApi.join(cwd, 'scripts', 'streamget_bridge.py'), roomId, quality],
    cwd,
  };
}

function validateFlvUrl(value: unknown): string {
  try {
    return parseAllowedDouyuFlvUrl(value).toString();
  } catch (error) {
    if (error instanceof DouyuStreamUrlError && error.code === 'INVALID_RESPONSE') {
      throw new StreamgetBridgeError('INVALID_RESPONSE');
    }
    throw new StreamgetBridgeError('UNSAFE_STREAM_URL');
  }
}

export function parseStreamgetResponse(
  roomId: string,
  output: string,
  requestedQuality: StreamRequestQuality = 'auto',
): StreamgetRawResult {
  let parsed: unknown;
  try {
    parsed = JSON.parse(output);
  } catch {
    throw new StreamgetBridgeError('INVALID_RESPONSE');
  }

  if (!parsed || typeof parsed !== 'object') {
    throw new StreamgetBridgeError('INVALID_RESPONSE');
  }
  const value = parsed as {
    roomId?: unknown;
    isLive?: unknown;
    flvUrl?: unknown;
    resolvedQuality?: unknown;
    source?: unknown;
  };
  if (value.roomId !== roomId || typeof value.isLive !== 'boolean') {
    throw new StreamgetBridgeError('INVALID_RESPONSE');
  }
  if (!value.isLive) return { roomId, isLive: false };
  const resolvedQuality = value.resolvedQuality;
  const source = value.source;
  if (
    resolvedQuality !== undefined
    && !['auto', 'original', 'super', 'high', 'standard', '720p'].includes(resolvedQuality as string)
  ) {
    throw new StreamgetBridgeError('INVALID_RESPONSE');
  }
  if (source !== undefined && source !== 'web-h5' && source !== 'app-fallback') {
    throw new StreamgetBridgeError('INVALID_RESPONSE');
  }
  return {
    roomId,
    isLive: true,
    flvUrl: validateFlvUrl(value.flvUrl),
    resolvedQuality: (resolvedQuality as StreamRequestQuality | undefined) ?? requestedQuality,
    source: source ?? 'app-fallback',
  };
}

function createDefaultRunner(
  options: StreamgetBridgeOptions,
): (roomId: string, quality: StreamRequestQuality) => Promise<string> {
  const timeoutMs = options.timeoutMs ?? 30_000;

  return async (roomId, quality) => {
    try {
      const launch = resolveStreamgetLaunch(roomId, quality, options);
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
    async resolve(roomId, quality = 'auto') {
      const output = await run(roomId, quality);
      return parseStreamgetResponse(roomId, output, quality);
    },
  };
}
