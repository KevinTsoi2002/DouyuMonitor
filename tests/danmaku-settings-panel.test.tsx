import { renderToStaticMarkup } from 'react-dom/server';
import { describe, expect, it } from 'vitest';
import { createMockDouyuAdapter } from '../src/infrastructure/mock-douyu-adapter';
import { DanmakuSettingsPanel } from '../src/renderer/components/DanmakuSettingsPanel';
import { WorkspaceProvider } from '../src/renderer/store/workspace-context';

describe('DanmakuSettingsPanel', () => {
  it('renders every approved setting with the default selections', () => {
    const html = renderToStaticMarkup(
      <WorkspaceProvider adapter={createMockDouyuAdapter()} demoMode>
        <DanmakuSettingsPanel open onClose={() => {}} />
      </WorkspaceProvider>,
    );

    for (const label of [
      '显示',
      '治理',
      '统计',
      '全局默认',
      '关键词黑名单',
      '重复抑制窗口',
      '高峰保护',
      '过滤',
      '限流',
      '弹幕速度',
      '弹幕大小',
      '不透明度',
      '弹幕区域',
      '弹幕数量',
      '弹幕字体',
      '字体渲染',
    ]) {
      expect(html).toContain(label);
    }
    expect(html.match(/type="range"/g)).toHaveLength(4);
    expect(html.match(/>重置<\/button>/g)).toHaveLength(3);
    expect(html).toMatch(/aria-pressed="true"[^>]*>全屏<\/button>/);
    expect(html).toMatch(/aria-pressed="true"[^>]*>正常<\/button>/);
    expect(html).toMatch(/aria-pressed="true"[^>]*>微软雅黑<\/button>/);
    expect(html).toMatch(/aria-pressed="true"[^>]*>原生<\/button>/);
    expect(html).toContain('data-tab="display"');
    expect(html).toContain('data-tab="governance"');
    expect(html).toContain('data-tab="stats"');
  });

  it('renders nothing while closed', () => {
    const html = renderToStaticMarkup(
      <WorkspaceProvider adapter={createMockDouyuAdapter()} demoMode>
        <DanmakuSettingsPanel open={false} onClose={() => {}} />
      </WorkspaceProvider>,
    );

    expect(html).toBe('');
  });
});
