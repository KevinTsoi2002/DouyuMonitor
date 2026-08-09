import { describe, expect, it } from 'vitest';
import { DouyuAdapterError } from '../src/domain/douyu-adapter';
import { toIpcError } from '../src/shared/ipc-contract';

describe('StreamGet IPC error', () => {
  it('maps dependency failures without exposing resolver output', () => {
    const result = toIpcError(new DouyuAdapterError(
      'STREAMGET_UNAVAILABLE',
      'flvUrl=https://example.invalid/?token=secret',
    ));

    expect(result).toEqual({
      code: 'STREAMGET_UNAVAILABLE',
      message: '\u65e0\u6cd5\u542f\u52a8 StreamGet\uff0c\u8bf7\u68c0\u67e5 Python \u73af\u5883',
      retryable: true,
    });
    expect(JSON.stringify(result)).not.toContain('secret');
  });
});
