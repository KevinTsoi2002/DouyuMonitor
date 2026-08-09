import type {
  DouyuAdapter,
  RoomCandidate,
  StreamQuality,
} from '../domain/douyu-adapter';
import type { RoomInput } from '../domain/input-resolver';

export const MOCK_ROOM_CANDIDATES: RoomCandidate[] = [
  {
    roomId: '63136',
    anchorName: '星河',
    title: '星河的夜航电台 · 聊聊最近的比赛',
    category: '英雄联盟',
    online: true,
    viewerLabel: '18.6 万',
  },
  {
    roomId: '270888',
    anchorName: '林深',
    title: '林深的主舞台 · 新版本实战复盘',
    category: '王者荣耀',
    online: true,
    viewerLabel: '9.3 万',
  },
  {
    roomId: '385729',
    anchorName: '白昼',
    title: '白昼陪你看球 · 赛后慢聊',
    category: '体育',
    online: true,
    viewerLabel: '6.1 万',
  },
  {
    roomId: '89607',
    anchorName: '橘子汽水',
    title: '橘子汽水 · 今日歌单和现场点歌',
    category: '音乐',
    online: true,
    viewerLabel: '4.8 万',
  },
];

function roomForId(roomId: string): RoomCandidate {
  const existing = MOCK_ROOM_CANDIDATES.find((room) => room.roomId === roomId);
  if (existing) return { ...existing };

  return {
    roomId,
    anchorName: `主播 ${roomId}`,
    title: `房间 ${roomId} · 模拟直播信号`,
    category: '综合直播',
    online: true,
    viewerLabel: `${(Number(roomId.slice(-2)) % 80) + 10}.${Number(roomId.slice(-1))} 万`,
  };
}

export function createMockDouyuAdapter(): DouyuAdapter {
  return {
    async search(input: RoomInput) {
      if (input.type === 'room-id') return [roomForId(input.value)];

      const query = input.value.toLocaleLowerCase();
      return MOCK_ROOM_CANDIDATES.filter((room) =>
        room.anchorName.toLocaleLowerCase().includes(query),
      ).map((room) => ({ ...room }));
    },

    async getStreamAvailability(roomId: string) {
      const qualities: Array<{ quality: StreamQuality; label: string }> = [
        { quality: 'auto', label: '自动' },
        { quality: 'original', label: '原画' },
        { quality: 'super', label: '超清' },
        { quality: 'high', label: '高清' },
        { quality: 'standard', label: '标清' },
      ];

      return {
        kind: 'available',
        roomId,
        checkedAt: new Date().toISOString(),
        variants: qualities.map(({ quality, label }) => ({
          id: `mock-${quality}`,
          label,
          quality,
          playbackUrl: `mock://${roomId}/${quality}`,
          container: 'hls' as const,
        })),
      };
    },
  };
}
