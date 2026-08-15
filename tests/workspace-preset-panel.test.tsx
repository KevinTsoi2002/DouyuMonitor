import { renderToStaticMarkup } from 'react-dom/server';
import { describe, expect, it } from 'vitest';
import { createMockDouyuAdapter } from '../src/infrastructure/mock-douyu-adapter';
import { WorkspacePresetPanel } from '../src/renderer/components/WorkspacePresetPanel';
import { WorkspaceProvider } from '../src/renderer/store/workspace-context';

describe('WorkspacePresetPanel', () => {
  it('renders the empty state and all preset actions', () => {
    const html = renderToStaticMarkup(
      <WorkspaceProvider adapter={createMockDouyuAdapter()} demoMode>
        <WorkspacePresetPanel open onClose={() => {}} />
      </WorkspaceProvider>,
    );

    expect(html).toContain('id="workspace-presets-panel"');
    expect(html).toContain('工作区');
    expect(html).toContain('未保存工作区');
    expect(html).toContain('保存当前工作区');
    expect(html).toContain('还没有工作区预设');
    expect(html).toContain('保存当前工作区');
    expect(html).toContain('输入预设名称');
  });

  it('renders a persisted preset with room count and management controls', () => {
    const storage = {
      value: '',
      getItem() { return this.value || null; },
      setItem(_key: string, value: string) { this.value = value; },
      removeItem() { this.value = ''; },
    };
    const html = renderToStaticMarkup(
      <WorkspaceProvider adapter={createMockDouyuAdapter()} demoMode initialRooms={[]}>
        <WorkspacePresetPanel open onClose={() => {}} />
      </WorkspaceProvider>,
    );

    expect(html).toMatch(/aria-label="保存当前工作区"/);
    expect(html).toContain('aria-label="关闭工作区预设"');
    expect(storage).toBeDefined();
  });

  it('renders nothing while closed', () => {
    const html = renderToStaticMarkup(
      <WorkspaceProvider adapter={createMockDouyuAdapter()} demoMode>
        <WorkspacePresetPanel open={false} onClose={() => {}} />
      </WorkspaceProvider>,
    );

    expect(html).toBe('');
  });
});
