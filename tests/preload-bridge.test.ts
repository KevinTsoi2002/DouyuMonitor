import { describe, expect, it } from 'vitest';
import { createAppApi, type IpcRendererLike } from '../src/preload/bridge';
import { IPC_CHANNELS, ok } from '../src/shared/ipc-contract';

describe('createAppApi', () => {
  it('invokes the approved search channel with a typed payload', async () => {
    const calls: Array<{ channel: string; payload: unknown }> = [];
    const ipcRenderer: IpcRendererLike = {
      invoke: async (channel, payload) => {
        calls.push({ channel, payload });
        return ok([{ roomId: '63136' }]);
      },
      on: () => {},
      removeListener: () => {},
    };
    const api = createAppApi(ipcRenderer);

    await expect(api.searchRooms('63136')).resolves.toEqual(ok([{ roomId: '63136' }]));
    expect(calls).toEqual([{ channel: IPC_CHANNELS.searchRooms, payload: { input: '63136' } }]);
  });

  it('does not expose a generic invoke method', () => {
    const api = createAppApi({
      invoke: async () => ok({ status: 'ok' }),
      on: () => {},
      removeListener: () => {},
    });

    expect('invoke' in api).toBe(false);
    expect(Object.keys(api)).toEqual([
      'searchRooms',
      'getStreamAvailability',
      'releaseStreamProxy',
      'startDanmaku',
      'stopDanmaku',
      'onDanmakuEvent',
      'minimizeWindow',
      'toggleMaximizeWindow',
      'closeWindow',
      'onMaximizedChanged',
      'getSystemNotificationSupport',
      'showSystemNotification',
      'ping',
    ]);
  });

  it('invokes only the approved system notification channels', async () => {
    const calls: Array<{ channel: string; payload: unknown }> = [];
    const api = createAppApi({
      invoke: async (channel, payload) => {
        calls.push({ channel, payload });
        return ok(channel === IPC_CHANNELS.getSystemNotificationSupport
          ? { supported: true }
          : undefined);
      },
      on: () => {},
      removeListener: () => {},
    });

    await expect(api.getSystemNotificationSupport()).resolves.toEqual(ok({ supported: true }));
    await expect(api.showSystemNotification({ title: '斗鱼监控', body: '星河已开播' }))
      .resolves.toEqual(ok(undefined));
    expect(calls).toEqual([
      { channel: IPC_CHANNELS.getSystemNotificationSupport, payload: undefined },
      {
        channel: IPC_CHANNELS.showSystemNotification,
        payload: { title: '斗鱼监控', body: '星河已开播' },
      },
    ]);
  });

  it('invokes only the approved stream availability channel', async () => {
    const calls: Array<{ channel: string; payload: unknown }> = [];
    const ipcRenderer: IpcRendererLike = {
      invoke: async (channel, payload) => {
        calls.push({ channel, payload });
        return ok({ kind: 'blocked', roomId: '63136' });
      },
      on: () => {},
      removeListener: () => {},
    };
    const api = createAppApi(ipcRenderer);

    await api.getStreamAvailability('63136', '720p');
    await api.releaseStreamProxy('63136');

    expect(calls).toContainEqual({
      channel: IPC_CHANNELS.getStreamAvailability,
      payload: { roomId: '63136', quality: '720p' },
    });
    expect(calls).toContainEqual({
      channel: IPC_CHANNELS.releaseStreamProxy,
      payload: { roomId: '63136' },
    });
  });

  it('invokes only the approved danmaku lifecycle channels', async () => {
    const calls: Array<{ channel: string; payload: unknown }> = [];
    const api = createAppApi({
      invoke: async (channel, payload) => {
        calls.push({ channel, payload });
        return ok(undefined);
      },
      on: () => {},
      removeListener: () => {},
    });

    await api.startDanmaku('63136');
    await api.stopDanmaku('63136');

    expect(calls).toEqual([
      { channel: IPC_CHANNELS.startDanmaku, payload: { roomId: '63136' } },
      { channel: IPC_CHANNELS.stopDanmaku, payload: { roomId: '63136' } },
    ]);
  });

  it('subscribes only to normalized danmaku events and returns an exact unsubscriber', () => {
    const listeners = new Map<string, (event: unknown, payload: unknown) => void>();
    const removed: string[] = [];
    const api = createAppApi({
      invoke: async () => ok(undefined),
      on(channel, listener) {
        listeners.set(channel, listener);
      },
      removeListener(channel, listener) {
        if (listeners.get(channel) === listener) listeners.delete(channel);
        removed.push(channel);
      },
    });
    const received: unknown[] = [];
    const unsubscribe = api.onDanmakuEvent((event) => received.push(event));

    listeners.get(IPC_CHANNELS.danmakuEvent)?.({}, {
      type: 'status',
      status: { roomId: '63136', state: 'connected' },
    });
    listeners.get(IPC_CHANNELS.danmakuEvent)?.({}, {
      type: 'messages',
      roomId: 'abc',
    });
    unsubscribe();

    expect(received).toHaveLength(1);
    expect(removed).toEqual([IPC_CHANNELS.danmakuEvent]);
  });

  it('invokes only fixed window command channels', async () => {
    const calls: Array<{ channel: string; payload: unknown }> = [];
    const api = createAppApi({
      invoke: async (channel, payload) => {
        calls.push({ channel, payload });
        return undefined;
      },
      on: () => {},
      removeListener: () => {},
    });

    await api.minimizeWindow();
    await api.toggleMaximizeWindow();
    await api.closeWindow();

    expect(calls).toEqual([
      { channel: IPC_CHANNELS.windowMinimize, payload: undefined },
      { channel: IPC_CHANNELS.windowToggleMaximize, payload: undefined },
      { channel: IPC_CHANNELS.windowClose, payload: undefined },
    ]);
  });

  it('accepts only boolean maximize notifications', () => {
    const listeners = new Map<string, (event: unknown, payload: unknown) => void>();
    const removed: string[] = [];
    const api = createAppApi({
      invoke: async () => undefined,
      on: (channel, listener) => { listeners.set(channel, listener); },
      removeListener: (channel, listener) => {
        if (listeners.get(channel) === listener) listeners.delete(channel);
        removed.push(channel);
      },
    });
    const received: boolean[] = [];
    const unsubscribe = api.onMaximizedChanged((maximized) => received.push(maximized));

    listeners.get(IPC_CHANNELS.windowMaximizedChanged)?.({}, true);
    listeners.get(IPC_CHANNELS.windowMaximizedChanged)?.({}, 'true');
    unsubscribe();

    expect(received).toEqual([true]);
    expect(removed).toEqual([IPC_CHANNELS.windowMaximizedChanged]);
  });
});
