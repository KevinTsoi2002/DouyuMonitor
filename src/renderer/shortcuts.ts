import type { ToastInput } from './notifications/toast-context';

export interface ShortcutDefinition {
  key: string;
  label: string;
}

export const APP_SHORTCUTS = {
  addRoom: { key: 'a', label: 'Ctrl+Shift+A' },
  toggleWorkspace: { key: 'w', label: 'Ctrl+Shift+W' },
  toggleMonitoring: { key: 'm', label: 'Ctrl+Shift+M' },
  toggleDanmakuSettings: { key: 'd', label: 'Ctrl+Shift+D' },
  toggleSidebar: { key: 's', label: 'Ctrl+Shift+S' },
  refreshMainRoom: { key: 'r', label: 'Ctrl+Shift+R' },
} as const satisfies Record<string, ShortcutDefinition>;

export interface KeyboardShortcutEvent {
  key: string;
  ctrlKey: boolean;
  shiftKey: boolean;
  altKey: boolean;
  metaKey: boolean;
  repeat: boolean;
  defaultPrevented?: boolean;
  target: unknown;
  preventDefault(): void;
}

export interface AppShortcutActions {
  addRoom(): void | Promise<void>;
  toggleWorkspace(): void | Promise<void>;
  toggleMonitoring(): void | Promise<void>;
  toggleDanmakuSettings(): void | Promise<void>;
  toggleSidebar(): void | Promise<void>;
  hasPrimaryRoom: boolean;
  refreshMainRoom(): void | Promise<void>;
}

type ShortcutActionName = Exclude<keyof AppShortcutActions, 'hasPrimaryRoom'>;
type PushToast = (input: ToastInput) => unknown;

interface EditableTargetLike {
  tagName?: unknown;
  isContentEditable?: unknown;
  getAttribute?: (name: string) => string | null;
}

export function isEditableTarget(target: unknown): boolean {
  if (!target || typeof target !== 'object') return false;
  const element = target as EditableTargetLike;
  const tagName = typeof element.tagName === 'string' ? element.tagName.toLowerCase() : '';
  return tagName === 'input'
    || tagName === 'textarea'
    || tagName === 'select'
    || element.isContentEditable === true
    || element.getAttribute?.('contenteditable') === 'true';
}

export function matchesShortcut(
  event: KeyboardShortcutEvent,
  shortcut: ShortcutDefinition,
): boolean {
  return !event.defaultPrevented
    && !event.repeat
    && event.ctrlKey
    && event.shiftKey
    && !event.altKey
    && !event.metaKey
    && !isEditableTarget(event.target)
    && event.key.toLowerCase() === shortcut.key.toLowerCase();
}

const SHORTCUT_ACTIONS: Array<[ShortcutActionName, ShortcutDefinition]> = [
  ['addRoom', APP_SHORTCUTS.addRoom],
  ['toggleWorkspace', APP_SHORTCUTS.toggleWorkspace],
  ['toggleMonitoring', APP_SHORTCUTS.toggleMonitoring],
  ['toggleDanmakuSettings', APP_SHORTCUTS.toggleDanmakuSettings],
  ['toggleSidebar', APP_SHORTCUTS.toggleSidebar],
  ['refreshMainRoom', APP_SHORTCUTS.refreshMainRoom],
];

export function createAppShortcutListener(
  actions: AppShortcutActions,
  pushToast: PushToast,
): (event: KeyboardShortcutEvent) => void {
  const runAction = (action: () => void | Promise<void>) => {
    try {
      const result = action();
      if (result && typeof (result as Promise<void>).catch === 'function') {
        void (result as Promise<void>).catch(() => {
          pushToast({ level: 'error', message: '快捷键操作失败，请稍后重试' });
        });
      }
    } catch {
      pushToast({ level: 'error', message: '快捷键操作失败，请稍后重试' });
    }
  };

  return (event) => {
    const match = SHORTCUT_ACTIONS.find(([, shortcut]) => matchesShortcut(event, shortcut));
    if (!match) return;
    event.preventDefault();
    const [actionName] = match;
    if (actionName === 'refreshMainRoom' && !actions.hasPrimaryRoom) {
      pushToast({ level: 'info', message: '请先设置主直播间' });
      return;
    }
    runAction(actions[actionName]);
  };
}
