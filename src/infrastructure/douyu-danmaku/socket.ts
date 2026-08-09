import { Buffer } from 'node:buffer';
import type { ClientRequest, IncomingMessage } from 'node:http';
import WebSocket, { type RawData } from 'ws';

export interface DanmakuSocketHandlers {
  open(): void;
  message(data: ArrayBuffer): void;
  error(error: unknown): void;
  close(code: number, reason: string): void;
  unexpectedResponse(statusCode: number): void;
}

export interface DanmakuSocket {
  send(data: Uint8Array): void;
  close(): void;
  dispose(): void;
}

export type DanmakuSocketFactory = (
  url: string,
  handlers: DanmakuSocketHandlers,
) => DanmakuSocket;

export function rawDataToArrayBuffer(data: RawData): ArrayBuffer {
  const buffer = Array.isArray(data)
    ? Buffer.concat(data)
    : data instanceof ArrayBuffer
      ? new Uint8Array(data)
      : data;
  return buffer.buffer.slice(
    buffer.byteOffset,
    buffer.byteOffset + buffer.byteLength,
  ) as ArrayBuffer;
}

export const createWsDanmakuSocket: DanmakuSocketFactory = (url, handlers) => {
  const socket = new WebSocket(url);
  socket.binaryType = 'arraybuffer';

  const onOpen = () => handlers.open();
  const onMessage = (data: RawData) => handlers.message(rawDataToArrayBuffer(data));
  const onError = (error: Error) => handlers.error(error);
  const onClose = (code: number, reason: Buffer) =>
    handlers.close(code, reason.toString('utf8'));
  const onUnexpectedResponse = (_request: ClientRequest, response: IncomingMessage) => {
    handlers.unexpectedResponse(response.statusCode ?? 0);
  };

  socket.on('open', onOpen);
  socket.on('message', onMessage);
  socket.on('error', onError);
  socket.on('close', onClose);
  socket.on('unexpected-response', onUnexpectedResponse);

  return {
    send(data) {
      if (socket.readyState !== WebSocket.OPEN) throw new Error('Socket is not open');
      socket.send(data);
    },
    close() {
      if (
        socket.readyState === WebSocket.OPEN ||
        socket.readyState === WebSocket.CONNECTING
      ) {
        socket.close();
      }
    },
    dispose() {
      socket.off('open', onOpen);
      socket.off('message', onMessage);
      socket.off('error', onError);
      socket.off('close', onClose);
      socket.off('unexpected-response', onUnexpectedResponse);
    },
  };
};
