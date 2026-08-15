import { renderToStaticMarkup } from 'react-dom/server';
import { describe, expect, it, vi } from 'vitest';
import { createMockDouyuAdapter } from '../src/infrastructure/mock-douyu-adapter';
import { MonitoringStatusPanel } from '../src/renderer/components/MonitoringStatusPanel';
import { WorkspaceProvider } from '../src/renderer/store/workspace-context';
import type { RoomCandidate } from '../src/domain/douyu-adapter';

function candidate(roomId: string, online = true): RoomCandidate {
  return {
    roomId,
    anchorName: `主播 ${roomId}`,
    title: `直播间 ${roomId}`,
    category: '综合直播',
    online,
    viewerLabel: '100',
  };
}

describe('MonitoringStatusPanel', () => {
  it('renders a compact room health list and aggregate counters', () => {
    const html = renderToStaticMarkup(
      <WorkspaceProvider
        adapter={createMockDouyuAdapter()}
        demoMode
        initialRooms={[candidate('101'), candidate('202', false)]}
      >
        <MonitoringStatusPanel open onClose={vi.fn()} />
      </WorkspaceProvider>,
    );

    expect(html).toContain('监控状态');
    expect(html).toContain('在线');
    expect(html).toContain('主播 101');
    expect(html).toContain('主播 202');
    expect(html).toContain('最近检查');
    expect(html).toContain('自动恢复');
    expect(html).toContain('aria-label="主播 101 刷新播放源"');
    expect(html).toMatch(/aria-label="主播 202 刷新播放源"[^>]*disabled=""/);
  });

  it('renders an empty state when there are no rooms', () => {
    const html = renderToStaticMarkup(
      <WorkspaceProvider adapter={createMockDouyuAdapter()} demoMode>
        <MonitoringStatusPanel open onClose={vi.fn()} />
      </WorkspaceProvider>,
    );

    expect(html).toContain('暂无直播间');
    expect(html).not.toContain('class="monitor-room-row"');
  });
});
