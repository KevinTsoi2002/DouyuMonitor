import { renderToStaticMarkup } from 'react-dom/server';
import { describe, expect, it } from 'vitest';
import { createMockDouyuAdapter } from '../src/infrastructure/mock-douyu-adapter';
import { WorkspacePresetPanel } from '../src/renderer/components/WorkspacePresetPanel';
import { createWorkspaceStore } from '../src/renderer/store/workspace-store';
import { WorkspaceProvider } from '../src/renderer/store/workspace-context';

function createMemoryStorage() {
  const values = new Map<string, string>();
  return {
    getItem(key: string) { return values.get(key) ?? null; },
    setItem(key: string, value: string) { values.set(key, value); },
    removeItem(key: string) { values.delete(key); },
  };
}

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

  it('renders management controls for an empty room set', () => {
    const html = renderToStaticMarkup(
      <WorkspaceProvider adapter={createMockDouyuAdapter()} demoMode initialRooms={[]}>
        <WorkspacePresetPanel open onClose={() => {}} />
      </WorkspaceProvider>,
    );

    expect(html).toMatch(/aria-label="保存当前工作区"/);
    expect(html).toContain('aria-label="关闭工作区预设"');
    expect(html).toContain('当前 0 个房间');
  });

  it('renders a saved preset with room count and management controls', () => {
    const storage = createMemoryStorage();
    const source = createWorkspaceStore(createMockDouyuAdapter(), {
      storage,
      initialRooms: [{
        roomId: '63136',
        anchorName: '星河',
        title: '星河直播',
        category: '英雄联盟',
        online: true,
        viewerLabel: '18.6 万',
      }],
      createPresetId: () => 'preset-1',
    });
    source.getState().saveWorkspacePreset('比赛视角');

    const html = renderToStaticMarkup(
      <WorkspaceProvider adapter={createMockDouyuAdapter()} demoMode storage={storage}>
        <WorkspacePresetPanel open onClose={() => {}} />
      </WorkspaceProvider>,
    );

    expect(html).toContain('比赛视角');
    expect(html).toContain('1 个房间');
    expect(html).toContain('比赛视角 加载');
    expect(html).toContain('比赛视角 重命名');
    expect(html).toContain('比赛视角 删除');
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
