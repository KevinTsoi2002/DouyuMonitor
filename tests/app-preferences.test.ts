import { describe, expect, it } from 'vitest';
import {
  APP_PREFERENCES_SCHEMA_VERSION,
  APP_PREFERENCES_STORAGE_KEY,
  DEFAULT_APP_PREFERENCES,
  loadAppPreferences,
  saveAppPreferences,
  type AppPreferences,
} from '../src/renderer/notifications/app-preferences';

function createMemoryStorage(initial?: Record<string, string>) {
  const values = new Map(Object.entries(initial ?? {}));
  return {
    getItem(key: string) { return values.get(key) ?? null; },
    setItem(key: string, value: string) { values.set(key, value); },
    removeItem(key: string) { values.delete(key); },
  };
}

describe('app preferences', () => {
  it('defaults system notifications to disabled when no preference exists', () => {
    expect(loadAppPreferences(createMemoryStorage())).toEqual(DEFAULT_APP_PREFERENCES);
  });

  it('round-trips the versioned system notification preference', () => {
    const storage = createMemoryStorage();
    const preferences: AppPreferences = {
      schemaVersion: APP_PREFERENCES_SCHEMA_VERSION,
      systemNotificationsEnabled: true,
    };

    expect(saveAppPreferences(storage, preferences)).toBe(true);
    expect(storage.getItem(APP_PREFERENCES_STORAGE_KEY)).toContain('systemNotificationsEnabled');
    expect(loadAppPreferences(storage)).toEqual(preferences);
  });

  it('falls back to defaults for invalid JSON, versions, or boolean fields', () => {
    const storage = createMemoryStorage();

    storage.setItem(APP_PREFERENCES_STORAGE_KEY, 'not-json');
    expect(loadAppPreferences(storage)).toEqual(DEFAULT_APP_PREFERENCES);

    storage.setItem(APP_PREFERENCES_STORAGE_KEY, JSON.stringify({
      schemaVersion: APP_PREFERENCES_SCHEMA_VERSION + 1,
      systemNotificationsEnabled: true,
    }));
    expect(loadAppPreferences(storage)).toEqual(DEFAULT_APP_PREFERENCES);

    storage.setItem(APP_PREFERENCES_STORAGE_KEY, JSON.stringify({
      schemaVersion: APP_PREFERENCES_SCHEMA_VERSION,
      systemNotificationsEnabled: 'yes',
    }));
    expect(loadAppPreferences(storage)).toEqual(DEFAULT_APP_PREFERENCES);
  });

  it('keeps the in-memory app usable when storage throws', () => {
    const storage = {
      getItem() { throw new Error('read failed'); },
      setItem() { throw new Error('write failed'); },
      removeItem() {},
    };

    expect(loadAppPreferences(storage)).toEqual(DEFAULT_APP_PREFERENCES);
    expect(saveAppPreferences(storage, {
      schemaVersion: APP_PREFERENCES_SCHEMA_VERSION,
      systemNotificationsEnabled: true,
    })).toBe(false);
  });
});
