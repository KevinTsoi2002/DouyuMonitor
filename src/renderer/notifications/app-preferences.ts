import type { WorkspaceStorage } from '../store/workspace-persistence';

export const APP_PREFERENCES_STORAGE_KEY = 'douyu-monitor.preferences.v1';
export const APP_PREFERENCES_SCHEMA_VERSION = 1 as const;

export interface AppPreferences {
  schemaVersion: typeof APP_PREFERENCES_SCHEMA_VERSION;
  systemNotificationsEnabled: boolean;
}

export const DEFAULT_APP_PREFERENCES: AppPreferences = {
  schemaVersion: APP_PREFERENCES_SCHEMA_VERSION,
  systemNotificationsEnabled: false,
};

function getDefaultStorage(): WorkspaceStorage | undefined {
  if (typeof globalThis.localStorage === 'undefined') return undefined;
  return globalThis.localStorage;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return Boolean(value) && typeof value === 'object' && !Array.isArray(value);
}

function parseAppPreferences(value: unknown): AppPreferences | undefined {
  if (!isRecord(value)) return undefined;
  if (value.schemaVersion !== APP_PREFERENCES_SCHEMA_VERSION) return undefined;
  if (typeof value.systemNotificationsEnabled !== 'boolean') return undefined;
  return {
    schemaVersion: APP_PREFERENCES_SCHEMA_VERSION,
    systemNotificationsEnabled: value.systemNotificationsEnabled,
  };
}

export function loadAppPreferences(
  storage: WorkspaceStorage | undefined = getDefaultStorage(),
): AppPreferences {
  if (!storage) return DEFAULT_APP_PREFERENCES;
  try {
    const raw = storage.getItem(APP_PREFERENCES_STORAGE_KEY);
    if (!raw) return DEFAULT_APP_PREFERENCES;
    return parseAppPreferences(JSON.parse(raw)) ?? DEFAULT_APP_PREFERENCES;
  } catch {
    return DEFAULT_APP_PREFERENCES;
  }
}

export function saveAppPreferences(
  storage: WorkspaceStorage | undefined,
  preferences: AppPreferences,
): boolean {
  if (!storage) return false;
  const normalized = parseAppPreferences(preferences);
  if (!normalized) return false;
  try {
    storage.setItem(APP_PREFERENCES_STORAGE_KEY, JSON.stringify(normalized));
    return true;
  } catch {
    return false;
  }
}
