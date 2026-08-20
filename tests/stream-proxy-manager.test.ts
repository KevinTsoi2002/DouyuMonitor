import { createServer, request as httpRequest, type Server } from 'node:http';
import { afterEach, describe, expect, it } from 'vitest';
import {
  createStreamProxyManager,
  type StreamProxyManager,
} from '../src/main/stream-proxy-manager';

let upstream: Server | undefined;
let manager: StreamProxyManager | undefined;

function listen(server: Server): Promise<number> {
  return new Promise((resolve, reject) => {
    server.once('error', reject);
    server.listen(0, '127.0.0.1', () => {
      server.off('error', reject);
      const address = server.address();
      if (!address || typeof address === 'string') return reject(new Error('NO_ADDRESS'));
      resolve(address.port);
    });
  });
}

function readResponse(url: string, headers?: Record<string, string>): Promise<{
  status: number;
  headers: Record<string, string | string[] | undefined>;
  body: string;
}> {
  return new Promise((resolve, reject) => {
    const request = httpRequest(url, { headers }, (response) => {
      const chunks: Buffer[] = [];
      response.on('data', (chunk: Buffer) => chunks.push(chunk));
      response.on('end', () => resolve({
        status: response.statusCode ?? 0,
        headers: response.headers,
        body: Buffer.concat(chunks).toString('utf8'),
      }));
    });
    request.on('error', reject);
    request.end();
  });
}

afterEach(async () => {
  await manager?.closeAll();
  await new Promise<void>((resolve) => upstream?.close(() => resolve()) ?? resolve());
  manager = undefined;
  upstream = undefined;
});

describe('StreamProxyManager', () => {
  it('gives each room a distinct loopback port and proxies FLV bytes', async () => {
    upstream = createServer((_request, response) => {
      response.writeHead(200, { 'content-type': 'video/x-flv' });
      response.end('FLV-DATA');
    });
    const upstreamPort = await listen(upstream);
    manager = createStreamProxyManager({
      validateUpstream: (url) => url.hostname === '127.0.0.1' && url.port === String(upstreamPort),
      createToken: () => 'fixed-test-token',
    });

    const firstUrl = await manager.register('1', `http://127.0.0.1:${upstreamPort}/one.flv`);
    const secondUrl = await manager.register('2', `http://127.0.0.1:${upstreamPort}/two.flv`);
    const first = await readResponse(firstUrl);
    const second = await readResponse(secondUrl);

    expect(new URL(firstUrl).hostname).toBe('127.0.0.1');
    expect(new URL(firstUrl).port).not.toBe(new URL(secondUrl).port);
    expect(new URL(firstUrl).pathname).toBe('/stream/fixed-test-token.flv');
    expect(first).toMatchObject({ status: 200, body: 'FLV-DATA' });
    expect(second).toMatchObject({ status: 200, body: 'FLV-DATA' });
    expect(first.headers['content-type']).toBe('video/x-flv');
    expect(first.headers['access-control-allow-origin']).toBe('*');
  });

  it('rejects wrong host and path without opening upstream', async () => {
    let upstreamRequests = 0;
    upstream = createServer((_request, response) => {
      upstreamRequests += 1;
      response.end('unexpected');
    });
    const upstreamPort = await listen(upstream);
    manager = createStreamProxyManager({
      validateUpstream: (url) => url.hostname === '127.0.0.1' && url.port === String(upstreamPort),
      createToken: () => 'fixed-test-token',
    });
    const proxyUrl = await manager.register('1', `http://127.0.0.1:${upstreamPort}/one.flv`);
    const parsed = new URL(proxyUrl);

    const wrongPath = await readResponse(`http://127.0.0.1:${parsed.port}/stream/wrong.flv`);
    const wrongHost = await readResponse(proxyUrl, { host: `localhost:${parsed.port}` });

    expect(wrongPath.status).toBe(404);
    expect(wrongHost.status).toBe(403);
    expect(upstreamRequests).toBe(0);
  });

  it('revalidates redirects and reuses a room endpoint when updated', async () => {
    upstream = createServer((request, response) => {
      if (request.url === '/redirect') {
        response.writeHead(302, { location: '/final.flv' });
        response.end();
        return;
      }
      response.end(request.url === '/updated.flv' ? 'UPDATED' : 'FINAL');
    });
    const upstreamPort = await listen(upstream);
    manager = createStreamProxyManager({
      validateUpstream: (url) => url.hostname === '127.0.0.1' && url.port === String(upstreamPort),
      createToken: () => 'fixed-test-token',
    });
    const initial = await manager.register('1', `http://127.0.0.1:${upstreamPort}/redirect`);
    const updated = await manager.register('1', `http://127.0.0.1:${upstreamPort}/updated.flv`);

    expect(updated).toBe(initial);
    await expect(readResponse(updated)).resolves.toMatchObject({ status: 200, body: 'UPDATED' });
  });

  it('releases one room endpoint and closes all endpoints idempotently', async () => {
    upstream = createServer((_request, response) => response.end('OK'));
    const upstreamPort = await listen(upstream);
    manager = createStreamProxyManager({
      validateUpstream: (url) => url.hostname === '127.0.0.1' && url.port === String(upstreamPort),
      createToken: () => 'fixed-test-token',
    });
    const firstUrl = await manager.register('1', `http://127.0.0.1:${upstreamPort}/one.flv`);
    const secondUrl = await manager.register('2', `http://127.0.0.1:${upstreamPort}/two.flv`);

    await manager.release('1');
    await expect(readResponse(firstUrl)).rejects.toBeDefined();
    await expect(readResponse(secondUrl)).resolves.toMatchObject({ status: 200, body: 'OK' });
    await manager.closeAll();
    await manager.closeAll();
  });
});
