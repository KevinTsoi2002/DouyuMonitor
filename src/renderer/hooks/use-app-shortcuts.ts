import { useEffect } from 'react';
import { createAppShortcutListener, type AppShortcutActions } from '../shortcuts';
import type { ToastInput } from '../notifications/toast-context';

export function useAppShortcuts(
  actions: AppShortcutActions,
  pushToast: (input: ToastInput) => unknown,
): void {
  useEffect(() => {
    if (typeof window === 'undefined') return undefined;
    const listener = createAppShortcutListener(actions, pushToast);
    window.addEventListener('keydown', listener);
    return () => window.removeEventListener('keydown', listener);
  }, [actions, pushToast]);
}
