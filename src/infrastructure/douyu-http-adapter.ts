import {
  DouyuAdapterError,
  type DouyuAdapter,
  type ObservedStreamQuality,
  type RoomCandidate,
} from '../domain/douyu-adapter';

const ROOM_API_BASE_URL = 'https://open.douyucdn.cn/api/RoomApi/room/';
const SEARCH_API_URL = 'https://www.douyu.com/japi/search/api/searchShow';
const BETARD_API_BASE_URL = 'https://www.douyu.com/betard/';

export type DouyuFetch = (url: string, init?: RequestInit) => Promise<Response>;

export interface DouyuHttpAdapterOptions {
  fetch?: DouyuFetch;
  timeoutMs?: number;
  now?: () => Date;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null;
}

function scalarString(value: unknown): string | undefined {
  if (typeof value === 'string' && value.length > 0) return value;
  if (typeof value === 'number' && Number.isFinite(value)) return String(value);
  return undefined;
}

function safeHttpUrl(value: unknown): string | undefined {
  if (typeof value !== 'string' || value.length === 0) return undefined;

  try {
    const url = new URL(value);
    return url.protocol === 'http:' || url.protocol === 'https:'
      ? url.toString()
      : undefined;
  } catch {
    return undefined;
  }
}

function formatViewerLabel(value: unknown): string {
  const viewers = Number(value);
  if (!Number.isFinite(viewers) || viewers <= 0) return '0';
  if (viewers >= 10_000) {
    const count = (viewers / 10_000).toFixed(1).replace(/\.0$/, '');
    return `${count} 万`;
  }
  return new Intl.NumberFormat('zh-CN').format(Math.round(viewers));
}

function observedQualities(value: unknown): ObservedStreamQuality[] {
  if (!Array.isArray(value)) return [];

  const qualities = new Map<number, ObservedStreamQuality>();
  for (const item of value) {
    if (!isRecord(item) || typeof item.name !== 'string' || typeof item.type !== 'number') {
      continue;
    }
    const label = item.name.trim();
    if (!label || !Number.isInteger(item.type) || qualities.has(item.type)) continue;

    qualities.set(item.type, {
      id: `douyu-${item.type}`,
      label,
      providerType: item.type,
    });
  }
  return [...qualities.values()];
}

async function requestJson(
  url: string,
  fetchImpl: DouyuFetch,
  timeoutMs: number,
): Promise<unknown> {
  let response: Response;
  try {
    response = await fetchImpl(url, {
      headers: { Accept: 'application/json' },
      signal: AbortSignal.timeout(timeoutMs),
    });
  } catch {
    throw new DouyuAdapterError('NETWORK_UNAVAILABLE');
  }

  if (!response.ok) throw new DouyuAdapterError('NETWORK_UNAVAILABLE');

  try {
    return await response.json();
  } catch {
    throw new DouyuAdapterError('PROTOCOL_CHANGED');
  }
}

async function fetchRoom(
  roomId: string,
  fetchImpl: DouyuFetch,
  timeoutMs: number,
): Promise<RoomCandidate> {
  const payload = await requestJson(
    `${ROOM_API_BASE_URL}${encodeURIComponent(roomId)}`,
    fetchImpl,
    timeoutMs,
  );
  if (!isRecord(payload) || typeof payload.error !== 'number') {
    throw new DouyuAdapterError('PROTOCOL_CHANGED');
  }
  if (payload.error !== 0) throw new DouyuAdapterError('ROOM_NOT_FOUND');
  if (!isRecord(payload.data)) throw new DouyuAdapterError('PROTOCOL_CHANGED');

  const mappedRoomId = scalarString(payload.data.room_id);
  const title = scalarString(payload.data.room_name);
  const anchorName = scalarString(payload.data.owner_name);
  const avatarUrl = safeHttpUrl(payload.data.avatar);
  if (!mappedRoomId || !title || !anchorName) {
    throw new DouyuAdapterError('PROTOCOL_CHANGED');
  }

  return {
    roomId: mappedRoomId,
    anchorName,
    ...(avatarUrl ? { avatarUrl } : {}),
    title,
    category: scalarString(payload.data.cate_name) ?? '未分类',
    online: scalarString(payload.data.room_status) === '1',
    viewerLabel: formatViewerLabel(payload.data.online),
  };
}

function mapSearchCandidate(value: unknown): RoomCandidate | undefined {
  if (!isRecord(value)) return undefined;

  const roomId = scalarString(value.rid);
  const anchorName = scalarString(value.nickName);
  const title = scalarString(value.roomName);
  const avatarUrl = safeHttpUrl(value.avatar);
  if (!roomId || !anchorName || !title) return undefined;

  return {
    roomId,
    anchorName,
    ...(avatarUrl ? { avatarUrl } : {}),
    title,
    category: scalarString(value.cateName) ?? '未分类',
    online: scalarString(value.isLive) === '1',
    viewerLabel: formatViewerLabel(value.hot),
  };
}

async function searchAnchors(
  query: string,
  fetchImpl: DouyuFetch,
  timeoutMs: number,
): Promise<RoomCandidate[]> {
  const url = new URL(SEARCH_API_URL);
  url.searchParams.set('kw', query);
  url.searchParams.set('page', '1');
  url.searchParams.set('pageSize', '20');

  const payload = await requestJson(url.toString(), fetchImpl, timeoutMs);
  if (!isRecord(payload) || payload.error !== 0 || !isRecord(payload.data)) {
    throw new DouyuAdapterError('PROTOCOL_CHANGED');
  }
  if (!Array.isArray(payload.data.relateShow)) {
    throw new DouyuAdapterError('PROTOCOL_CHANGED');
  }

  const candidates = new Map<string, RoomCandidate>();
  for (const item of payload.data.relateShow) {
    const candidate = mapSearchCandidate(item);
    if (candidate && !candidates.has(candidate.roomId)) {
      candidates.set(candidate.roomId, candidate);
    }
  }
  return [...candidates.values()];
}

export function createDouyuHttpAdapter(
  options: DouyuHttpAdapterOptions = {},
): DouyuAdapter {
  const fetchImpl = options.fetch ?? globalThis.fetch;
  const timeoutMs = options.timeoutMs ?? 10_000;
  const now = options.now ?? (() => new Date());

  return {
    async search(input) {
      if (input.type === 'room-id') {
        return [await fetchRoom(input.value, fetchImpl, timeoutMs)];
      }
      return searchAnchors(input.value, fetchImpl, timeoutMs);
    },

    async getStreamAvailability(roomId) {
      const payload = await requestJson(
        `${BETARD_API_BASE_URL}${encodeURIComponent(roomId)}`,
        fetchImpl,
        timeoutMs,
      );
      if (!isRecord(payload) || !isRecord(payload.room)) {
        throw new DouyuAdapterError('PROTOCOL_CHANGED');
      }

      const mappedRoomId = scalarString(payload.room.room_id);
      const showStatus = scalarString(payload.room.show_status);
      if (!mappedRoomId || !showStatus || !Array.isArray(payload.room.multirates)) {
        throw new DouyuAdapterError('PROTOCOL_CHANGED');
      }

      const qualities = observedQualities(payload.room.multirates);
      return {
        kind: 'blocked',
        roomId: mappedRoomId,
        reason: showStatus !== '1'
          ? 'ROOM_OFFLINE'
          : qualities.length > 0
            ? 'SIGNATURE_REQUIRED'
            : 'NO_PUBLIC_SOURCE',
        observedQualities: qualities,
        checkedAt: now().toISOString(),
      };
    },
  };
}
