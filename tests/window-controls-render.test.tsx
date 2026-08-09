import { renderToStaticMarkup } from 'react-dom/server';
import { describe, expect, it } from 'vitest';
import {
  WindowControls,
  type WindowControlApi,
} from '../src/renderer/components/WindowControls';

function createApi(): WindowControlApi {
  return {
    minimizeWindow: async () => {},
    toggleMaximizeWindow: async () => {},
    closeWindow: async () => {},
    onMaximizedChanged: () => () => {},
  };
}

describe('WindowControls', () => {
  it('renders nothing without the Electron API', () => {
    expect(renderToStaticMarkup(<WindowControls />)).toBe('');
  });

  it('renders fixed window commands when the Electron API exists', () => {
    const html = renderToStaticMarkup(<WindowControls api={createApi()} />);

    expect(html).toContain('aria-label="最小化"');
    expect(html).toContain('aria-label="最大化"');
    expect(html).toContain('aria-label="关闭"');
  });

  it('renders restore state for a maximized window', () => {
    const html = renderToStaticMarkup(
      <WindowControls api={createApi()} initialMaximized />,
    );

    expect(html).toContain('aria-label="还原"');
    expect(html).not.toContain('aria-label="最大化"');
  });
});
