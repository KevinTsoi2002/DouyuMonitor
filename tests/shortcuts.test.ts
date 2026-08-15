import { describe, expect, it, vi } from 'vitest';
import {
  APP_SHORTCUTS,
  createAppShortcutListener,
  isEditableTarget,
  matchesShortcut,
  type AppShortcutActions,
  type KeyboardShortcutEvent,
} from '../src/renderer/shortcuts';

function keyboardEvent(overrides: Partial<KeyboardShortcutEvent> = {}): KeyboardShortcutEvent {
  return {
    key: 'a',
    ctrlKey: true,
    shiftKey: true,
    altKey: false,
    metaKey: false,
    repeat: false,
    target: { tagName: 'BUTTON' },
    preventDefault: vi.fn(),
    ...overrides,
  };
}

function actions(overrides: Partial<AppShortcutActions> = {}): AppShortcutActions {
  return {
    addRoom: vi.fn(),
    toggleWorkspace: vi.fn(),
    toggleMonitoring: vi.fn(),
    toggleDanmakuSettings: vi.fn(),
    toggleSidebar: vi.fn(),
    hasPrimaryRoom: true,
    refreshMainRoom: vi.fn(),
    ...overrides,
  };
}

describe('app shortcuts', () => {
  it('matches Ctrl+Shift shortcuts and rejects extra modifiers or repeats', () => {
    expect(matchesShortcut(keyboardEvent(), APP_SHORTCUTS.addRoom)).toBe(true);
    expect(matchesShortcut(keyboardEvent({ key: 'A' }), APP_SHORTCUTS.addRoom)).toBe(true);
    expect(matchesShortcut(keyboardEvent({ altKey: true }), APP_SHORTCUTS.addRoom)).toBe(false);
    expect(matchesShortcut(keyboardEvent({ metaKey: true }), APP_SHORTCUTS.addRoom)).toBe(false);
    expect(matchesShortcut(keyboardEvent({ repeat: true }), APP_SHORTCUTS.addRoom)).toBe(false);
    expect(matchesShortcut(keyboardEvent({ shiftKey: false }), APP_SHORTCUTS.addRoom)).toBe(false);
  });

  it('ignores editable targets including contenteditable elements', () => {
    expect(isEditableTarget({ tagName: 'INPUT' })).toBe(true);
    expect(isEditableTarget({ tagName: 'textarea' })).toBe(true);
    expect(isEditableTarget({ tagName: 'select' })).toBe(true);
    expect(isEditableTarget({ tagName: 'div', isContentEditable: true })).toBe(true);
    expect(isEditableTarget({ tagName: 'button' })).toBe(false);
    expect(matchesShortcut(keyboardEvent({ target: { tagName: 'input' } }), APP_SHORTCUTS.addRoom)).toBe(false);
  });

  it('routes each shortcut to its action and prevents browser defaults', () => {
    const appActions = actions();
    const listener = createAppShortcutListener(appActions, vi.fn());
    const cases: Array<[string, keyof AppShortcutActions]> = [
      ['a', 'addRoom'],
      ['w', 'toggleWorkspace'],
      ['m', 'toggleMonitoring'],
      ['d', 'toggleDanmakuSettings'],
      ['s', 'toggleSidebar'],
      ['r', 'refreshMainRoom'],
    ];

    for (const [key, action] of cases) {
      const event = keyboardEvent({ key });
      listener(event);
      expect(appActions[action]).toHaveBeenCalledTimes(1);
      expect(event.preventDefault).toHaveBeenCalledTimes(1);
    }
  });

  it('shows a toast instead of refreshing when there is no primary room', () => {
    const appActions = actions({ hasPrimaryRoom: false });
    const pushToast = vi.fn();
    const listener = createAppShortcutListener(appActions, pushToast);

    listener(keyboardEvent({ key: 'r' }));

    expect(appActions.refreshMainRoom).not.toHaveBeenCalled();
    expect(pushToast).toHaveBeenCalledWith(expect.objectContaining({
      level: 'info',
      message: '请先设置主直播间',
    }));
  });
});
