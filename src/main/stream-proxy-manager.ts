import { randomBytes } from 'node:crypto';
import { request as httpRequest, createServer, type ClientRequest, type IncomingMessage, type Server, type ServerResponse } from 'node:http';
import { request as httpsRequest } from 'node:https';
import { Agent as HttpAgent } from 'node:http';
import { Agent as HttpsAgent } from 'node:https';
import type { AddressInfo, Socket } from 'node:net';
import { isAllowedDouyuStreamUrl } from './douyu-stream-url';

export type StreamProxyErrorCode = 'LOCAL_STREAM_PROXY_FAILED' | 'UPSTREAM_FAILED';

export class StreamProxyError extends Error {
  constructor(public readonly code: StreamProxyErrorCode, message: string = code) {
    super(message);
    this.name = 'StreamProxyError';
  }
}

export interface StreamProxyManager {
  register(roomId: string, upstreamUrl: string): Promise<string>;
  release(roomId: string): Promise<void>;
  closeAll(): Promise<void>;
}

export interface StreamProxyManagerOptions {
  validateUpstream?: (url: URL) => boolean;
  createToken?: () => string;
  maxRedirects?: number;
}

interface ProxyEntry {
  roomId: string;
  server: Server;
  port: number;
  token: string;
  upstreamUrl: string;
  httpAgent: HttpAgent;
  httpsAgent: HttpsAgent;
  requests: Set<ClientRequest>;
  sockets: Set<Socket>;
}

function validateUpstreamUrl(value: string, validate: (url: URL) => boolean): URL {
  let url: URL;
  try {
    url = new URL(value);
  } catch {
    throw new StreamProxyError('UPSTREAM_FAILED');
  }
  if (!validate(url)) throw new StreamProxyError('UPSTREAM_FAILED');
  return url;
}

function writeError(response: ServerResponse, statusCode: number): void {
  if (response.headersSent) {
    response.destroy();
    return;
  }
  response.writeHead(statusCode, { 'content-type': 'text/plain; charset=utf-8' });
  response.end();
}

function listen(server: Server): Promise<number> {
  return new Promise((resolve, reject) => {
    const onError = (error: Error) => {
      server.off('listening', onListening);
      reject(new StreamProxyError('LOCAL_STREAM_PROXY_FAILED', error.message));
    };
    const onListening = () => {
      server.off('error', onError);
      const address = server.address();
      if (!address || typeof address === 'string') {
        reject(new StreamProxyError('LOCAL_STREAM_PROXY_FAILED'));
        return;
      }
      resolve((address as AddressInfo).port);
    };
    server.once('error', onError);
    server.once('listening', onListening);
    server.listen({ host: '127.0.0.1', port: 0 });
  });
}

function closeServer(server: Server): Promise<void> {
  return new Promise((resolve) => {
    if (!server.listening) {
      resolve();
      return;
    }
    server.close(() => resolve());
  });
}

export function createStreamProxyManager(
  options: StreamProxyManagerOptions = {},
): StreamProxyManager {
  const validate = options.validateUpstream ?? ((url: URL) => isAllowedDouyuStreamUrl(url.toString()));
  const createToken = options.createToken ?? (() => randomBytes(24).toString('hex'));
  const maxRedirects = options.maxRedirects ?? 3;
  const entries = new Map<string, ProxyEntry>();

  const proxyRequest = (
    entry: ProxyEntry,
    request: IncomingMessage,
    response: ServerResponse,
    redirectCount = 0,
  ): void => {
    let upstream: URL;
    try {
      upstream = validateUpstreamUrl(entry.upstreamUrl, validate);
    } catch {
      writeError(response, 502);
      return;
    }

    const requestFn = upstream.protocol === 'https:' ? httpsRequest : httpRequest;
    const agent = upstream.protocol === 'https:' ? entry.httpsAgent : entry.httpAgent;
    const clientRequest = requestFn(upstream, {
      agent,
      headers: {
        accept: '*/*',
        referer: 'https://www.douyu.com/',
        'user-agent': 'Mozilla/5.0 DouyuMonitor',
      },
    }, (upstreamResponse) => {
      const status = upstreamResponse.statusCode ?? 0;
      const location = upstreamResponse.headers.location;
      if (status >= 300 && status < 400 && location) {
        upstreamResponse.resume();
        if (redirectCount >= maxRedirects) {
          writeError(response, 502);
          return;
        }
        try {
          entry.upstreamUrl = validateUpstreamUrl(new URL(location, upstream).toString(), validate).toString();
        } catch {
          writeError(response, 502);
          return;
        }
        proxyRequest(entry, request, response, redirectCount + 1);
        return;
      }

      if (status < 200 || status >= 300) {
        upstreamResponse.resume();
        writeError(response, 502);
        return;
      }

      response.writeHead(200, {
        'content-type': 'video/x-flv',
        'cache-control': 'no-store',
        'access-control-allow-origin': '*',
        'x-content-type-options': 'nosniff',
      });
      upstreamResponse.on('data', (chunk: Buffer) => {
        if (!response.write(chunk)) {
          upstreamResponse.pause();
          response.once('drain', () => upstreamResponse.resume());
        }
      });
      upstreamResponse.on('end', () => response.end());
      upstreamResponse.on('error', () => writeError(response, 502));
    });
    entry.requests.add(clientRequest);
    clientRequest.once('close', () => entry.requests.delete(clientRequest));
    clientRequest.once('error', () => {
      entry.requests.delete(clientRequest);
      writeError(response, 502);
    });
    response.once('close', () => clientRequest.destroy());
    request.once('aborted', () => clientRequest.destroy());
    clientRequest.end();
  };

  const register = async (roomId: string, upstreamUrl: string): Promise<string> => {
    const existing = entries.get(roomId);
    if (existing) {
      validateUpstreamUrl(upstreamUrl, validate);
      existing.upstreamUrl = upstreamUrl;
      return `http://127.0.0.1:${existing.port}/stream/${existing.token}.flv`;
    }

    validateUpstreamUrl(upstreamUrl, validate);
    const entry = {
      roomId,
      server: undefined as unknown as Server,
      port: 0,
      token: createToken(),
      upstreamUrl,
      httpAgent: new HttpAgent({ keepAlive: true }),
      httpsAgent: new HttpsAgent({ keepAlive: true }),
      requests: new Set<ClientRequest>(),
      sockets: new Set<Socket>(),
    } satisfies ProxyEntry;
    entry.server = createServer((request, response) => {
      const expectedPath = `/stream/${entry.token}.flv`;
      const expectedHost = `127.0.0.1:${entry.port}`;
      if (request.method !== 'GET') {
        writeError(response, 405);
        return;
      }
      if (request.headers.host !== expectedHost) {
        writeError(response, 403);
        return;
      }
      if (request.url !== expectedPath) {
        writeError(response, 404);
        return;
      }
      proxyRequest(entry, request, response);
    });
    entry.server.on('connection', (socket) => {
      entry.sockets.add(socket);
      socket.once('close', () => entry.sockets.delete(socket));
    });

    try {
      entry.port = await listen(entry.server);
    } catch (error) {
      entry.httpAgent.destroy();
      entry.httpsAgent.destroy();
      throw error;
    }
    entries.set(roomId, entry);
    return `http://127.0.0.1:${entry.port}/stream/${entry.token}.flv`;
  };

  const release = async (roomId: string): Promise<void> => {
    const entry = entries.get(roomId);
    if (!entry) return;
    entries.delete(roomId);
    for (const request of entry.requests) request.destroy();
    for (const socket of entry.sockets) socket.destroy();
    entry.httpAgent.destroy();
    entry.httpsAgent.destroy();
    await closeServer(entry.server);
  };

  const closeAll = async (): Promise<void> => {
    await Promise.all([...entries.keys()].map((roomId) => release(roomId)));
  };

  return { register, release, closeAll };
}
