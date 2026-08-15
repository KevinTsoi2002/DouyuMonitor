import { renderToStaticMarkup } from 'react-dom/server';
import { describe, expect, it } from 'vitest';
import { App, getInitialSidebarOpen } from '../src/renderer/App';
import { WorkspaceProvider } from '../src/renderer/store/workspace-context';
import { MOCK_ROOM_CANDIDATES } from '../src/infrastructure/mock-douyu-adapter';
import { createRendererDouyuAdapter } from '../src/infrastructure/renderer-douyu-adapter';

describe('App smoke render', () => {
  it('starts with the room sidebar closed on narrow viewports', () => {
    expect(getInitialSidebarOpen(390)).toBe(false);
    expect(getInitialSidebarOpen(1440)).toBe(true);
  });

  it('renders a fully collapsed desktop sidebar with only the header restore button', () => {
    const html = renderToStaticMarkup(
      <WorkspaceProvider
        adapter={createRendererDouyuAdapter()}
        demoMode
        initialRooms={MOCK_ROOM_CANDIDATES.slice(0, 1)}
        initialSidebarOpen={false}
      >
        <App />
      </WorkspaceProvider>,
    );

    expect(html).toMatch(/class="room-sidebar is-closed"[^>]*aria-hidden="true"/);
    expect(html).not.toContain('class="sidebar-edge-toggle"');
    expect(html).toMatch(/aria-label="展开房间列表"[^>]*aria-expanded="false"/);
  });

  it('keeps the header add-room icon accessible on compact layouts', () => {
    const html = renderToStaticMarkup(
      <WorkspaceProvider
        adapter={createRendererDouyuAdapter()}
        demoMode
        initialRooms={MOCK_ROOM_CANDIDATES.slice(0, 1)}
      >
        <App />
      </WorkspaceProvider>,
    );

    expect(html).toMatch(/class="button button-primary"[^>]*aria-label="添加直播间"/);
    expect(html).toMatch(/aria-label="关闭全局弹幕"/);
    expect(html).toMatch(/aria-label="打开弹幕设置"/);
    expect(html).toMatch(/aria-label="打开监控状态"[^>]*aria-controls="monitoring-status-panel"/);
    expect(html).toMatch(/aria-label="工作区：未保存工作区"[^>]*aria-controls="workspace-presets-panel"/);
    expect(html).toMatch(/aria-label="全局静音"/);
  });

  it('offers a direct way to enter the resizable primary layout', () => {
    const html = renderToStaticMarkup(
      <WorkspaceProvider
        adapter={createRendererDouyuAdapter()}
        demoMode
        initialRooms={MOCK_ROOM_CANDIDATES.slice(0, 2)}
      >
        <App />
      </WorkspaceProvider>,
    );

    expect(html).toMatch(/aria-label="进入主画面布局并调整大小"/);
  });

  it('keeps the toast viewport inside the application shell', () => {
    const html = renderToStaticMarkup(
      <WorkspaceProvider
        adapter={createRendererDouyuAdapter()}
        demoMode
        initialRooms={MOCK_ROOM_CANDIDATES.slice(0, 1)}
      >
        <App />
      </WorkspaceProvider>,
    );

    expect(html).toContain('class="toast-viewport"');
  });

  it('resolves automatic layout to a stable CSS layout class', () => {
    const html = renderToStaticMarkup(
      <WorkspaceProvider
        adapter={createRendererDouyuAdapter()}
        demoMode
        initialRooms={MOCK_ROOM_CANDIDATES.slice(0, 1)}
      >
        <App />
      </WorkspaceProvider>,
    );

    expect(html).toContain('workspace-grid layout-single');
  });

  it('renders the monitoring shell and seeded rooms', () => {
    const html = renderToStaticMarkup(
      <WorkspaceProvider
        adapter={createRendererDouyuAdapter()}
        demoMode
        initialRooms={MOCK_ROOM_CANDIDATES.slice(0, 2)}
      >
        <App />
      </WorkspaceProvider>,
    );

    expect(html).toContain('斗鱼视界');
    expect(html).toContain('星河');
    expect(html).toContain('林深');
    expect(html).toContain('draggable="true"');
    expect(html).toContain('data-room-id="63136"');
    expect(html).toContain('data-room-id="270888"');
    expect(html).toMatch(/aria-label="星河 更多操作"[^>]*aria-expanded="false"/);
  });

  it('uses truthful production copy and disables unavailable sidebar audio', () => {
    const html = renderToStaticMarkup(
      <WorkspaceProvider
        adapter={createRendererDouyuAdapter()}
        demoMode={false}
        initialRooms={MOCK_ROOM_CANDIDATES.slice(0, 1)}
      >
        <App />
      </WorkspaceProvider>,
    );

    expect(html).not.toContain('公开数据模式');
    expect(html).not.toContain('模拟信号正常');
    expect(html).toMatch(/aria-label="暂无可用音频"[^>]*disabled=""/);
  });

  it('omits decorative header and workspace copy', () => {
    const html = renderToStaticMarkup(
      <WorkspaceProvider
        adapter={createRendererDouyuAdapter()}
        demoMode
        initialRooms={MOCK_ROOM_CANDIDATES.slice(0, 1)}
      >
        <App />
      </WorkspaceProvider>,
    );

    expect(html).not.toContain('MULTI-VIEW DESK');
    expect(html).not.toContain('/ 9 路');
    expect(html).not.toContain('LIVE CANVAS');
    expect(html).not.toContain('多视角监看');
    expect(html).not.toContain('实时同步');
    expect(html).not.toContain('布局切换只移动');
  });
});
