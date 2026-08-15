import { renderToStaticMarkup } from 'react-dom/server';
import { describe, expect, it, vi } from 'vitest';
import { createMockDouyuAdapter } from '../src/infrastructure/mock-douyu-adapter';
import { MonitoringStatusPanel } from '../src/renderer/components/MonitoringStatusPanel';
import { NotificationProvider } from '../src/renderer/notifications/notification-context';
import { ToastProvider } from '../src/renderer/notifications/toast-context';
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

function renderPanel(initialRooms?: RoomCandidate[]) {
  return renderToStaticMarkup(
    <WorkspaceProvider
      adapter={createMockDouyuAdapter()}
      demoMode
      initialRooms={initialRooms}
    >
      <ToastProvider>
        <NotificationProvider>
          <MonitoringStatusPanel open onClose={vi.fn()} />
        </NotificationProvider>
      </ToastProvider>
    </WorkspaceProvider>
  );
}

describe('MonitoringStatusPanel', () => {
  it('renders a compact room health list and aggregate counters', () => {
    const html = renderPanel([candidate('101'), candidate('202', false)]);

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
    const html = renderPanel();

    expect(html).toContain('暂无直播间');
    expect(html).not.toContain('class="monitor-room-row"');
    expect(html).toContain('系统通知');
    expect(html).toMatch(/type="checkbox"[^>]*disabled=""/);
    expect(html).toContain('正在检查系统通知支持性');
  });

  it('keeps the notification preference control keyboard-accessible', () => {
    const html = renderPanel([candidate('101')]);

    expect(html).toMatch(/<input[^>]*type="checkbox"[^>]*aria-label="系统通知"/);
    expect(html).toContain('启用');
  });
});
