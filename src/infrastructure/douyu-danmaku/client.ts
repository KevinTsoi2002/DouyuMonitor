import type { DanmakuErrorCode, DanmakuStatus } from '../../shared/danmaku-contract';
import { DouyuFrameDecoder, encodeDouyuFrame } from './protocol';
import { parseStt, serializeStt } from './stt';
import {
  createWsDanmakuSocket,
  type DanmakuSocket,
  type DanmakuSocketFactory,
} from './socket';

export interface RawChatMessage {
  type: 'chatmsg';
  rid: string;
  cid?: string;
  nn?: string;
  txt?: string;
}

export type DouyuClientEvent =
  | { type: 'chat'; message: RawChatMessage }
  | { type: 'status'; status: DanmakuStatus };

export interface DouyuDanmakuClient {
  start(): void;
  stop(): void;
}

interface ClientDependencies {
  socketFactory: DanmakuSocketFactory;
  random: () => number;
  setTimeout: typeof globalThis.setTimeout;
  clearTimeout: typeof globalThis.clearTimeout;
  setInterval: typeof globalThis.setInterval;
  clearInterval: typeof globalThis.clearInterval;
}

const ENDPOINTS = [8501, 8502, 8503, 8504, 8505, 8506].map(
  (port) => `wss://danmuproxy.douyu.com:${port}/`,
);
const RETRY_DELAYS = [1_000, 2_000, 4_000, 8_000, 15_000, 15_000];
const HEARTBEAT_MS = 45_000;
const HANDSHAKE_MS = 10_000;
const STABLE_MS = 60_000;
const AUTH_EVIDENCE = /auth|login|token|sign|\u8ba4\u8bc1|\u767b\u5f55|\u7b7e\u540d/i;

function withJitter(base: number, random: () => number): number {
  return Math.round(base * (0.8 + random() * 0.4));
}

export function createDouyuDanmakuClient(
  roomId: string,
  emit: (event: DouyuClientEvent) => void,
  overrides: Partial<ClientDependencies> = {},
): DouyuDanmakuClient {
  const dependencies: ClientDependencies = {
    socketFactory: createWsDanmakuSocket,
    random: Math.random,
    setTimeout: globalThis.setTimeout,
    clearTimeout: globalThis.clearTimeout,
    setInterval: globalThis.setInterval,
    clearInterval: globalThis.clearInterval,
    ...overrides,
  };

  let socket: DanmakuSocket | undefined;
  let socketOpened = false;
  let reconnectTimer: ReturnType<typeof globalThis.setTimeout> | undefined;
  let heartbeatTimer: ReturnType<typeof globalThis.setInterval> | undefined;
  let handshakeTimer: ReturnType<typeof globalThis.setTimeout> | undefined;
  let stableTimer: ReturnType<typeof globalThis.setTimeout> | undefined;
  let generation = 0;
  let failureCount = 0;
  let initialEndpoint = 0;
  let currentEndpoint = 0;
  let manuallyStopped = true;
  const deniedEndpoints = new Set<number>();

  const sendStatus = (status: DanmakuStatus) => {
    emit({ type: 'status', status });
  };

  const clearConnectionTimers = () => {
    if (heartbeatTimer !== undefined) dependencies.clearInterval(heartbeatTimer);
    if (handshakeTimer !== undefined) dependencies.clearTimeout(handshakeTimer);
    if (stableTimer !== undefined) dependencies.clearTimeout(stableTimer);
    heartbeatTimer = undefined;
    handshakeTimer = undefined;
    stableTimer = undefined;
  };

  const releaseSocket = () => {
    const current = socket;
    socket = undefined;
    socketOpened = false;
    if (!current) return;
    current.dispose();
    current.close();
  };

  const isCurrent = (connectionGeneration: number) =>
    !manuallyStopped && connectionGeneration === generation;

  const blockForAuthentication = (connectionGeneration: number, attempt: number) => {
    if (!isCurrent(connectionGeneration)) return;
    clearConnectionTimers();
    releaseSocket();
    generation += 1;
    failureCount = Math.max(failureCount, attempt);
    sendStatus({
      roomId,
      state: 'platform-blocked',
      attempt,
      errorCode: 'AUTH_REQUIRED',
    });
  };

  const connectNext = () => {
    if (manuallyStopped || socket) return;

    const connectionGeneration = generation + 1;
    generation = connectionGeneration;
    currentEndpoint = (initialEndpoint + failureCount) % ENDPOINTS.length;
    const decoder = new DouyuFrameDecoder();
    let connectedEmitted = false;

    const markStable = () => {
      failureCount = 0;
      deniedEndpoints.clear();
    };

    const markConnected = () => {
      if (handshakeTimer !== undefined) {
        dependencies.clearTimeout(handshakeTimer);
        handshakeTimer = undefined;
      }
      if (connectedEmitted) return;
      connectedEmitted = true;
      sendStatus({ roomId, state: 'connected' });
    };

    const failCurrent = (errorCode: DanmakuErrorCode) => {
      if (!isCurrent(connectionGeneration)) return;
      clearConnectionTimers();
      releaseSocket();
      generation += 1;
      failureCount += 1;

      if (failureCount >= RETRY_DELAYS.length) {
        sendStatus({
          roomId,
          state: 'failed',
          attempt: failureCount,
          errorCode: 'RETRY_EXHAUSTED',
        });
        return;
      }

      sendStatus({
        roomId,
        state: 'reconnecting',
        attempt: failureCount,
        errorCode,
      });
      const retryGeneration = generation;
      const delay = withJitter(RETRY_DELAYS[failureCount - 1], dependencies.random);
      reconnectTimer = dependencies.setTimeout(() => {
        if (manuallyStopped || retryGeneration !== generation) return;
        reconnectTimer = undefined;
        connectNext();
      }, delay);
    };

    const send = (value: Record<string, string | number>) => {
      if (!isCurrent(connectionGeneration) || !socket) return;
      socket.send(encodeDouyuFrame(serializeStt(value)));
    };

    try {
      socket = dependencies.socketFactory(ENDPOINTS[currentEndpoint], {
        open() {
          if (!isCurrent(connectionGeneration)) return;
          socketOpened = true;
          try {
            send({ type: 'loginreq', roomid: roomId });
            send({ type: 'joingroup', rid: roomId, gid: -9999 });
          } catch {
            failCurrent('NETWORK_UNAVAILABLE');
            return;
          }

          heartbeatTimer = dependencies.setInterval(() => {
            if (!isCurrent(connectionGeneration)) return;
            try {
              send({ type: 'mrkl' });
            } catch {
              failCurrent('NETWORK_UNAVAILABLE');
            }
          }, HEARTBEAT_MS);
          handshakeTimer = dependencies.setTimeout(
            () => failCurrent('HANDSHAKE_TIMEOUT'),
            HANDSHAKE_MS,
          );
          stableTimer = dependencies.setTimeout(() => {
            stableTimer = undefined;
            if (isCurrent(connectionGeneration)) markStable();
          }, STABLE_MS);
        },
        message(data) {
          if (!isCurrent(connectionGeneration)) return;
          try {
            for (const raw of decoder.push(data)) {
              const message = parseStt(raw);
              const authText = ['msg', 'message', 'reason', 'error']
                .map((key) => message[key] ?? '')
                .join(' ');
              if (
                (message.type === 'error' || message.type === 'loginres') &&
                AUTH_EVIDENCE.test(authText)
              ) {
                blockForAuthentication(connectionGeneration, failureCount + 1);
                return;
              }

              const isSameRoomChat =
                message.type === 'chatmsg' && message.rid === roomId;
              if (
                message.type === 'loginres' ||
                message.type === 'setmsggroup' ||
                isSameRoomChat
              ) {
                markConnected();
              }
              if (!isSameRoomChat) continue;

              markStable();
              const chat: RawChatMessage = {
                type: 'chatmsg',
                rid: message.rid,
              };
              if (message.cid !== undefined) chat.cid = message.cid;
              if (message.nn !== undefined) chat.nn = message.nn;
              if (message.txt !== undefined) chat.txt = message.txt;
              emit({ type: 'chat', message: chat });
            }
          } catch {
            failCurrent('PROTOCOL_CHANGED');
          }
        },
        error() {
          failCurrent('NETWORK_UNAVAILABLE');
        },
        close(code, reason) {
          if (code === 1008 && AUTH_EVIDENCE.test(reason)) {
            blockForAuthentication(connectionGeneration, failureCount + 1);
            return;
          }
          failCurrent('NETWORK_UNAVAILABLE');
        },
        unexpectedResponse(statusCode) {
          if (!isCurrent(connectionGeneration)) return;
          if (statusCode === 401 || statusCode === 403) {
            deniedEndpoints.add(currentEndpoint);
            if (deniedEndpoints.size === ENDPOINTS.length) {
              blockForAuthentication(connectionGeneration, ENDPOINTS.length);
              return;
            }
            failCurrent('AUTH_REQUIRED');
            return;
          }
          failCurrent('NETWORK_UNAVAILABLE');
        },
      });
    } catch {
      failCurrent('NETWORK_UNAVAILABLE');
    }
  };

  return {
    start() {
      if (socket || reconnectTimer !== undefined) return;
      manuallyStopped = false;
      failureCount = 0;
      deniedEndpoints.clear();
      initialEndpoint = Math.floor(dependencies.random() * ENDPOINTS.length);
      if (initialEndpoint < 0 || initialEndpoint >= ENDPOINTS.length) initialEndpoint = 0;
      sendStatus({ roomId, state: 'connecting', attempt: 0 });
      connectNext();
    },
    stop() {
      manuallyStopped = true;
      generation += 1;
      if (reconnectTimer !== undefined) dependencies.clearTimeout(reconnectTimer);
      reconnectTimer = undefined;
      clearConnectionTimers();

      const current = socket;
      const wasOpen = socketOpened;
      socket = undefined;
      socketOpened = false;
      if (current) {
        if (wasOpen) {
          try {
            current.send(encodeDouyuFrame(serializeStt({ type: 'logout' })));
          } catch {
            // The session is already stopping; transport errors are intentionally ignored.
          }
        }
        current.dispose();
        current.close();
      }
      sendStatus({ roomId, state: 'idle' });
    },
  };
}
