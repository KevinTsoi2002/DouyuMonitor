const HEADER_BYTES = 12;
const LENGTH_AFTER_FIRST_FIELD = 8;
const CLIENT_PROTOCOL = 689;
const SERVER_PROTOCOL = 690;
const MAX_FRAME_BYTES = 1024 * 1024;

function concat(left: Uint8Array, right: Uint8Array): Uint8Array {
  const result = new Uint8Array(left.byteLength + right.byteLength);
  result.set(left, 0);
  result.set(right, left.byteLength);
  return result;
}

export function encodeDouyuFrame(text: string, protocol = CLIENT_PROTOCOL): Uint8Array {
  const payload = new TextEncoder().encode(text);
  const bodyLength = LENGTH_AFTER_FIRST_FIELD + payload.byteLength + 1;
  const frame = new Uint8Array(bodyLength + 4);
  const view = new DataView(frame.buffer);
  view.setUint32(0, bodyLength, true);
  view.setUint32(4, bodyLength, true);
  view.setUint16(8, protocol, true);
  frame.set(payload, HEADER_BYTES);
  frame[frame.byteLength - 1] = 0;
  return frame;
}

export class DouyuFrameDecoder {
  private pending: Uint8Array = new Uint8Array(0);
  private readonly decoder = new TextDecoder('utf-8', { fatal: true });

  push(chunk: ArrayBuffer | Uint8Array): string[] {
    const incoming = chunk instanceof Uint8Array ? chunk : new Uint8Array(chunk);
    this.pending = concat(this.pending, incoming);
    const messages: string[] = [];

    while (this.pending.byteLength >= 4) {
      const view = new DataView(
        this.pending.buffer,
        this.pending.byteOffset,
        this.pending.byteLength,
      );
      const bodyLength = view.getUint32(0, true);
      const totalLength = bodyLength + 4;
      if (bodyLength < LENGTH_AFTER_FIRST_FIELD + 1) throw new Error('Invalid frame length');
      if (totalLength > MAX_FRAME_BYTES) throw new Error('Frame size exceeds limit');
      if (this.pending.byteLength < totalLength) break;
      if (view.getUint32(4, true) !== bodyLength) throw new Error('Repeated length mismatch');
      if (view.getUint16(8, true) !== SERVER_PROTOCOL) throw new Error('Unexpected protocol type');
      if (this.pending[totalLength - 1] !== 0) throw new Error('Missing frame terminator');

      messages.push(this.decoder.decode(this.pending.slice(HEADER_BYTES, totalLength - 1)));
      this.pending = this.pending.slice(totalLength);
    }

    return messages;
  }
}
