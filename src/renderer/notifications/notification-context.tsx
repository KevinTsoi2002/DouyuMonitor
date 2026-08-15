import { createContext, useCallback, useContext, useEffect, useMemo, useRef, useState, type PropsWithChildren } from 'react';
import type { AppApi } from '../../preload/bridge';
import type { IpcResult } from '../../shared/ipc-contract';
import { getRoomMonitoringView } from '../monitoring-status';
import {
  DEFAULT_APP_PREFERENCES,
  loadAppPreferences,
  saveAppPreferences,
  type AppPreferences,
} from './app-preferences';
import {
  createNotificationPolicy,
  type NotificationEvent,
  type RoomNotificationSnapshot,
} from './notification-policy';
import { useToast, type ToastInput } from './toast-context';
import { useWorkspace } from '../store/workspace-context';
import type { WorkspaceStorage } from '../store/workspace-persistence';

type NotificationBridge = Pick<AppApi, 'getSystemNotificationSupport' | 'showSystemNotification'>;

interface NotificationDispatcherOptions {
  enabled: boolean;
  supported: boolean;
  appApi?: Partial<NotificationBridge>;
  pushToast(input: ToastInput): unknown;
}

function isPlaybackEvent(type: NotificationEvent['type']): boolean {
  return type === 'playback-failed' || type === 'playback-recovered';
}

function fallbackToast(event: NotificationEvent, pushToast: NotificationDispatcherOptions['pushToast']) {
  if (!isPlaybackEvent(event.type)) return;
  pushToast({
    level: event.type === 'playback-failed' ? 'error' : 'success',
    message: `${event.title}：${event.body}`,
  });
}

export function createNotificationDispatcher(options: NotificationDispatcherOptions) {
  return {
    async dispatch(event: NotificationEvent): Promise<void> {
      if (!options.enabled || !options.supported || !options.appApi?.showSystemNotification) {
        fallbackToast(event, options.pushToast);
        return;
      }

      let result: IpcResult<void>;
      try {
        result = await options.appApi.showSystemNotification({
          title: event.title,
          body: event.body,
        });
      } catch {
        fallbackToast(event, options.pushToast);
        return;
      }
      if (!result.ok) fallbackToast(event, options.pushToast);
    },
  };
}

interface NotificationContextValue {
  systemNotificationsEnabled: boolean;
  systemNotificationsSupported: boolean;
  systemNotificationSupportChecked: boolean;
  setSystemNotificationsEnabled(enabled: boolean): boolean;
}

const NotificationContext = createContext<NotificationContextValue | null>(null);

function getDefaultNotificationBridge(): NotificationBridge | undefined {
  if (typeof window === 'undefined' || !window.appApi) return undefined;
  return window.appApi;
}

function toRoomNotificationSnapshot(room: Parameters<typeof getRoomMonitoringView>[0]): RoomNotificationSnapshot {
  const view = getRoomMonitoringView(room);
  return {
    roomId: room.roomId,
    anchorName: room.anchorName,
    online: view.online,
    playbackState: view.playbackState,
    playbackErrorCode: view.lastErrorType,
  };
}

export interface NotificationProviderProps extends PropsWithChildren {
  storage?: WorkspaceStorage;
  appApi?: NotificationBridge;
}

export function NotificationProvider({ storage, appApi = getDefaultNotificationBridge(), children }: NotificationProviderProps) {
  const { pushToast } = useToast();
  const rooms = useWorkspace((state) => state.rooms);
  const [preferences, setPreferences] = useState<AppPreferences>(() => loadAppPreferences(storage));
  const [systemNotificationsSupported, setSystemNotificationsSupported] = useState(false);
  const [systemNotificationSupportChecked, setSystemNotificationSupportChecked] = useState(false);
  const policyRef = useRef(createNotificationPolicy());

  useEffect(() => {
    let cancelled = false;
    setSystemNotificationSupportChecked(false);
    if (!appApi?.getSystemNotificationSupport) {
      setSystemNotificationsSupported(false);
      setSystemNotificationSupportChecked(true);
      return () => { cancelled = true; };
    }
    void appApi.getSystemNotificationSupport().then((result) => {
      if (cancelled) return;
      setSystemNotificationsSupported(result.ok && result.data.supported);
      setSystemNotificationSupportChecked(true);
    }).catch(() => {
      if (cancelled) return;
      setSystemNotificationsSupported(false);
      setSystemNotificationSupportChecked(true);
    });
    return () => { cancelled = true; };
  }, [appApi]);

  useEffect(() => {
    const snapshots = rooms.map(toRoomNotificationSnapshot);
    const dispatcher = createNotificationDispatcher({
      enabled: preferences.systemNotificationsEnabled,
      supported: systemNotificationsSupported,
      appApi,
      pushToast,
    });
    for (const event of policyRef.current.update(snapshots)) void dispatcher.dispatch(event);
  }, [appApi, preferences.systemNotificationsEnabled, pushToast, rooms, systemNotificationsSupported]);

  const setSystemNotificationsEnabled = useCallback((enabled: boolean) => {
    if (enabled && (!systemNotificationSupportChecked || !systemNotificationsSupported)) {
      pushToast({ level: 'warning', message: '当前运行环境不支持系统通知' });
      return false;
    }
    const next: AppPreferences = {
      ...DEFAULT_APP_PREFERENCES,
      ...preferences,
      systemNotificationsEnabled: enabled,
    };
    const saved = saveAppPreferences(storage, next);
    setPreferences(next);
    if (!saved) {
      pushToast({ level: 'warning', message: '通知设置无法保存，本次运行仍会生效' });
    }
    return saved;
  }, [preferences, pushToast, storage, systemNotificationSupportChecked, systemNotificationsSupported]);

  const value = useMemo(() => ({
    systemNotificationsEnabled: preferences.systemNotificationsEnabled,
    systemNotificationsSupported,
    systemNotificationSupportChecked,
    setSystemNotificationsEnabled,
  }), [preferences.systemNotificationsEnabled, setSystemNotificationsEnabled, systemNotificationSupportChecked, systemNotificationsSupported]);

  return <NotificationContext.Provider value={value}>{children}</NotificationContext.Provider>;
}

export function useNotifications(): NotificationContextValue {
  const context = useContext(NotificationContext);
  if (!context) throw new Error('NotificationProvider is required');
  return context;
}
