import { describe, expect, it } from 'vitest';
import {
  DEFAULT_ERROR_TOAST_DURATION_MS,
  DEFAULT_TOAST_DURATION_MS,
  toastReducer,
  type ToastItem,
} from '../src/renderer/notifications/toast-context';

function toast(id: string, level: ToastItem['level'] = 'info'): ToastItem {
  return {
    id,
    level,
    message: `消息 ${id}`,
    createdAt: Number(id.replace('toast-', '')) || 0,
  };
}

describe('toast state', () => {
  it('keeps the most recent three toasts in FIFO order', () => {
    const state = toastReducer([], { type: 'push', item: toast('toast-1') });
    const next = toastReducer(
      toastReducer(
        toastReducer(state, { type: 'push', item: toast('toast-2') }),
        { type: 'push', item: toast('toast-3') },
      ),
      { type: 'push', item: toast('toast-4') },
    );

    expect(next.map((item) => item.id)).toEqual(['toast-2', 'toast-3', 'toast-4']);
  });

  it('dismisses one toast without changing the rest', () => {
    const state = [toast('toast-1'), toast('toast-2'), toast('toast-3')];

    expect(toastReducer(state, { type: 'dismiss', id: 'toast-2' })).toEqual([
      state[0],
      state[2],
    ]);
    expect(toastReducer(state, { type: 'dismiss', id: 'missing' })).toBe(state);
  });

  it('uses longer default duration for errors', () => {
    expect(DEFAULT_TOAST_DURATION_MS).toBe(3500);
    expect(DEFAULT_ERROR_TOAST_DURATION_MS).toBe(6000);
  });
});
