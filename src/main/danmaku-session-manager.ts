import type {
  DouyuClientEvent,
  DouyuDanmakuClient,
} from '../infrastructure/douyu-danmaku/client';
import type {
  DanmakuEvent,
  DanmakuMessage,
  DanmakuStatus,
} from '../shared/danmaku-contract';
import { IPC_CHANNELS } from '../shared/ipc-contract';

export interface DanmakuEventTarget {
  id: number;
  isDestroyed(): boolean;
  send(channel: string, event: DanmakuEvent): void;
}

export type DanmakuClientFactory = (
  roomId: string,
  emit: (event: DouyuClientEvent) => void,
) => DouyuDanmakuClient;

export interface DanmakuSessionManager {
  start(owner: DanmakuEventTarget, roomId: string): 'started' | 'existing' | 'limit';
  stop(ownerId: number, roomId: string): void;
  stopOwner(ownerId: number): void;
  stopAll(): void;
}

interface Session {
  client: DouyuDanmakuClient;
  owners: Map<number, DanmakuEventTarget>;
  pending: DanmakuMessage[];
  seenIds: Set<string>;
  seenOrder: string[];
  fallbackSequence: number;
  dropped: number;
  lastStatus?: DanmakuStatus;
}

const MAX_ROOMS = 9;
const MAX_PENDING = 100;
const MAX_SEEN_IDS = 200;
const FLUSH_MS = 250;
const MAX_BATCH = 10;

function sanitizeText(value: string): string {
  return value
    .replace(/[\u0000-\u0008\u000b\u000c\u000e-\u001f\u007f]/g, '')
    .replace(/[\r\n]+/g, ' ')
    .trim();
}

function truncateUnicode(value: string, limit: number): string {
  return Array.from(value).slice(0, limit).join('');
}

export function normalizeChatMessage(
  roomId: string,
  raw: { cid?: string; nn?: string; txt?: string },
  fallbackId: string,
  now: Date,
): DanmakuMessage | null {
  const text = truncateUnicode(sanitizeText(raw.txt ?? ''), 200);
  if (!text) return null;

  const nickname = truncateUnicode(sanitizeText(raw.nn ?? ''), 40);
  const candidateId = sanitizeText(raw.cid ?? '') || fallbackId;
  const id = candidateId.slice(0, 200);
  if (!id) return null;

  return {
    id,
    roomId,
    nickname: nickname || '\u533f\u540d\u7528\u6237',
    text,
    receivedAt: now.toISOString(),
  };
}

export function createDanmakuSessionManager(
  clientFactory: DanmakuClientFactory,
  now: () => Date = () => new Date(),
): DanmakuSessionManager {
  const sessions = new Map<string, Session>();

  const stopSession = (roomId: string, session: Session) => {
    if (sessions.get(roomId) !== session) return;
    sessions.delete(roomId);
    session.client.stop();
  };

  const removeDestroyedOwners = (roomId: string, session: Session) => {
    for (const [ownerId, owner] of session.owners) {
      if (owner.isDestroyed()) session.owners.delete(ownerId);
    }
    if (session.owners.size === 0) stopSession(roomId, session);
  };

  const sendToOwners = (roomId: string, session: Session, event: DanmakuEvent) => {
    removeDestroyedOwners(roomId, session);
    if (!sessions.has(roomId)) return;

    for (const [ownerId, owner] of session.owners) {
      if (owner.isDestroyed()) {
        session.owners.delete(ownerId);
        continue;
      }
      try {
        owner.send(IPC_CHANNELS.danmakuEvent, event);
      } catch {
        session.owners.delete(ownerId);
      }
    }
    if (session.owners.size === 0) stopSession(roomId, session);
  };

  const handleEvent = (roomId: string, event: DouyuClientEvent) => {
    const session = sessions.get(roomId);
    if (!session) return;

    if (event.type === 'status') {
      if (event.status.roomId !== roomId) return;
      session.lastStatus = event.status;
      sendToOwners(roomId, session, { type: 'status', status: event.status });
      return;
    }

    if (event.message.rid !== roomId) return;
    session.fallbackSequence += 1;
    const message = normalizeChatMessage(
      roomId,
      event.message,
      `${roomId}:${session.fallbackSequence}`,
      now(),
    );
    if (!message || session.seenIds.has(message.id)) return;

    session.seenIds.add(message.id);
    session.seenOrder.push(message.id);
    if (session.seenOrder.length > MAX_SEEN_IDS) {
      const oldestId = session.seenOrder.shift();
      if (oldestId !== undefined) session.seenIds.delete(oldestId);
    }

    session.pending.push(message);
    if (session.pending.length > MAX_PENDING) {
      const overflow = session.pending.length - MAX_PENDING;
      session.pending.splice(0, overflow);
      session.dropped += overflow;
    }
  };

  const flushTimer = globalThis.setInterval(() => {
    for (const [roomId, session] of sessions) {
      removeDestroyedOwners(roomId, session);
      if (!sessions.has(roomId) || session.pending.length === 0) continue;

      const messages = session.pending.splice(0, MAX_BATCH);
      const dropped = session.dropped;
      session.dropped = 0;
      sendToOwners(roomId, session, {
        type: 'messages',
        roomId,
        messages,
        dropped,
      });
    }
  }, FLUSH_MS);

  return {
    start(owner, roomId) {
      const existing = sessions.get(roomId);
      if (existing) {
        if (!owner.isDestroyed()) existing.owners.set(owner.id, owner);
        if (existing.lastStatus?.state === 'failed') existing.client.start();
        return 'existing';
      }
      if (sessions.size >= MAX_ROOMS) return 'limit';

      const client = clientFactory(roomId, (event) => handleEvent(roomId, event));
      const session: Session = {
        client,
        owners: new Map(),
        pending: [],
        seenIds: new Set(),
        seenOrder: [],
        fallbackSequence: 0,
        dropped: 0,
      };
      if (!owner.isDestroyed()) session.owners.set(owner.id, owner);
      sessions.set(roomId, session);
      client.start();
      if (session.owners.size === 0) stopSession(roomId, session);
      return 'started';
    },
    stop(ownerId, roomId) {
      const session = sessions.get(roomId);
      if (!session) return;
      session.owners.delete(ownerId);
      if (session.owners.size === 0) stopSession(roomId, session);
    },
    stopOwner(ownerId) {
      for (const [roomId, session] of sessions) {
        session.owners.delete(ownerId);
        if (session.owners.size === 0) stopSession(roomId, session);
      }
    },
    stopAll() {
      globalThis.clearInterval(flushTimer);
      for (const [roomId, session] of sessions) stopSession(roomId, session);
    },
  };
}
