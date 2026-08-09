export type RoomInput =
  | { type: 'room-id'; value: string }
  | { type: 'anchor-name'; value: string };

export function resolveRoomInput(input: string): RoomInput {
  const value = input.trim();
  if (!value) {
    throw new Error('请输入直播间号或主播名字');
  }

  if (/^\d+$/.test(value)) {
    return { type: 'room-id', value };
  }

  try {
    const url = new URL(value);
    const host = url.hostname.toLowerCase();
    const firstPathSegment = url.pathname.split('/').filter(Boolean)[0];
    if (host === 'douyu.com' || host.endsWith('.douyu.com')) {
      if (firstPathSegment && /^\d+$/.test(firstPathSegment)) {
        return { type: 'room-id', value: firstPathSegment };
      }
    }
  } catch {
    // Non-URL text is a valid anchor search.
  }

  return { type: 'anchor-name', value };
}
