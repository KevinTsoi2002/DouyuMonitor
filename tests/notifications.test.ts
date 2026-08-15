import { describe, expect, it, vi } from 'vitest';
import { createNotificationDispatcher } from '../src/renderer/notifications/notification-context';
import type { NotificationEvent } from '../src/renderer/notifications/notification-policy';

function event(type: NotificationEvent['type']): NotificationEvent {
  return {
    type,
    roomId: '101',
    anchorName: '星河',
    title: type === 'playback-failed' ? '星河 播放异常' : '星河 已开播',
    body: '房间 101 状态变化',
    dedupeKey: `101:${type}:test`,
    createdAt: 1,
  };
}

describe('notification dispatcher', () => {
  it('sends supported system notifications without a fallback toast', async () => {
    const showSystemNotification = vi.fn(async () => ({ ok: true as const, data: undefined }));
    const pushToast = vi.fn();
    const dispatcher = createNotificationDispatcher({
      enabled: true,
      supported: true,
      appApi: { showSystemNotification },
      pushToast,
    });

    await dispatcher.dispatch(event('playback-failed'));

    expect(showSystemNotification).toHaveBeenCalledWith({ title: '星河 播放异常', body: '房间 101 状态变化' });
    expect(pushToast).not.toHaveBeenCalled();
  });

  it('uses an error or success toast when system notifications are disabled', async () => {
    const showSystemNotification = vi.fn(async () => ({ ok: true as const, data: undefined }));
    const pushToast = vi.fn();
    const dispatcher = createNotificationDispatcher({
      enabled: false,
      supported: true,
      appApi: { showSystemNotification },
      pushToast,
    });

    await dispatcher.dispatch(event('playback-failed'));
    await dispatcher.dispatch({ ...event('playback-recovered'), title: '星河 播放已恢复' });
    await dispatcher.dispatch({ ...event('room-online'), title: '星河 正在直播' });

    expect(showSystemNotification).not.toHaveBeenCalled();
    expect(pushToast).toHaveBeenNthCalledWith(1, expect.objectContaining({ level: 'error' }));
    expect(pushToast).toHaveBeenNthCalledWith(2, expect.objectContaining({ level: 'success' }));
    expect(pushToast).toHaveBeenCalledTimes(2);
  });

  it('falls back for playback events when support is unavailable or IPC fails', async () => {
    const pushToast = vi.fn();
    const dispatcher = createNotificationDispatcher({
      enabled: true,
      supported: false,
      appApi: { showSystemNotification: vi.fn() },
      pushToast,
    });
    await dispatcher.dispatch(event('playback-recovered'));
    await dispatcher.dispatch(event('room-offline'));
    expect(pushToast).toHaveBeenCalledTimes(1);

    const failedShow = vi.fn(async () => ({
      ok: false as const,
      error: { code: 'NOTIFICATION_FAILED' as const, message: 'failed', retryable: true },
    }));
    const failedPushToast = vi.fn();
    const failedDispatcher = createNotificationDispatcher({
      enabled: true,
      supported: true,
      appApi: { showSystemNotification: failedShow },
      pushToast: failedPushToast,
    });
    await failedDispatcher.dispatch(event('playback-failed'));
    expect(failedPushToast).toHaveBeenCalledWith(expect.objectContaining({ level: 'error' }));
  });
});
