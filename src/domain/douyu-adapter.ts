import type { RoomInput } from './input-resolver';

export type StreamQuality = 'auto' | 'original' | 'super' | 'high' | 'standard';
export type StreamRequestQuality = StreamQuality | '720p';

export const STREAM_REQUEST_QUALITIES = [
  'auto',
  'original',
  'super',
  'high',
  'standard',
  '720p',
] as const satisfies readonly StreamRequestQuality[];

export type RoomStatus = 'playing' | 'offline' | 'reconnecting' | 'error';

export interface RoomCandidate {
  roomId: string;
  anchorName: string;
  avatarUrl?: string;
  title: string;
  category: string;
  online: boolean;
  viewerLabel: string;
}

export interface ObservedStreamQuality {
  id: string;
  label: string;
  providerType: number;
}

export interface StreamVariant {
  id: string;
  label: string;
  quality: StreamQuality;
  playbackUrl: string;
  container: 'hls' | 'flv';
}

export type StreamBlockReason =
  | 'ROOM_OFFLINE'
  | 'NO_PUBLIC_SOURCE'
  | 'SIGNATURE_REQUIRED';

export type StreamAvailability =
  | {
      kind: 'available';
      roomId: string;
      variants: StreamVariant[];
      checkedAt: string;
    }
  | {
      kind: 'blocked';
      roomId: string;
      reason: StreamBlockReason;
      observedQualities: ObservedStreamQuality[];
      checkedAt: string;
    };

export type DouyuAdapterErrorCode =
  | 'ROOM_NOT_FOUND'
  | 'NETWORK_UNAVAILABLE'
  | 'PROTOCOL_CHANGED'
  | 'STREAMGET_UNAVAILABLE'
  | 'LOCAL_STREAM_PROXY_FAILED';

export class DouyuAdapterError extends Error {
  constructor(
    public readonly code: DouyuAdapterErrorCode,
    message: string = code,
  ) {
    super(message);
    this.name = 'DouyuAdapterError';
  }
}

export interface DouyuAdapter {
  search(input: RoomInput): Promise<RoomCandidate[]>;
  getStreamAvailability(
    roomId: string,
    quality?: StreamRequestQuality,
  ): Promise<StreamAvailability>;
  releaseStream?(roomId: string): Promise<void>;
}
