import { renderToStaticMarkup } from 'react-dom/server';
import { describe, expect, it } from 'vitest';
import type { RoomCandidate } from '../src/domain/douyu-adapter';
import { createMockDouyuAdapter } from '../src/infrastructure/mock-douyu-adapter';
import { RoomSidebar } from '../src/renderer/components/RoomSidebar';
import { WorkspaceProvider } from '../src/renderer/store/workspace-context';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

function renderSidebar(initialRooms: RoomCandidate[]) {
  return renderToStaticMarkup(
    <WorkspaceProvider adapter={createMockDouyuAdapter()} demoMode initialRooms={initialRooms}>
      <RoomSidebar isOpen onAddRoom={() => {}} onManageGroups={() => {}} />
    </WorkspaceProvider>,
  );
}

describe('RoomSidebar', () => {
  it('keeps library rows content-sized inside a scrollable list', () => {
    const styles = readFileSync(resolve(process.cwd(), 'src/renderer/styles.css'), 'utf8');
    expect(styles).toContain('.room-library-list { display: grid; min-height: 0; flex: 1 1 0;');
    expect(styles).toContain('grid-auto-rows: max-content;');
    expect(styles).toContain('align-content: start;');
  });

  it('renders the room library navigation without legacy monitoring copy', () => {
    const html = renderSidebar([{
      roomId: '1',
      anchorName: '示例主播',
      title: '示例直播间',
      category: '综合直播',
      online: true,
      viewerLabel: '12.8 万',
    }]);

    expect(html).toContain('直播间列表');
    expect(html).toContain('收藏');
    expect(html).toContain('历史');
    expect(html).toContain('管理分组');
    expect(html).not.toContain('监看列表');
    expect(html).not.toContain('路信号');
    expect(html).not.toContain('点击主播设为主画面');
    expect(html).not.toContain('12.8 万');
    expect(html).toContain('room-row-primary');
    expect(html).not.toContain('综合直播');
    expect(html).not.toContain('>直播中<');
    expect(html).not.toContain('直播中 ·');
  });

  it('uses a red status marker for an offline room', () => {
    const html = renderSidebar([{
      roomId: '2',
      anchorName: '离线主播',
      title: '尚未开播',
      category: '综合直播',
      online: false,
      viewerLabel: '0',
    }]);

    expect(html).toContain('status-dot-offline');
    expect(html).toContain('未开播');
  });
});
