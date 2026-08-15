import { useCallback, useMemo, useState } from 'react';
import { AppHeader } from './components/AppHeader';
import { AddRoomDialog } from './components/AddRoomDialog';
import { GroupManagerDialog } from './components/GroupManagerDialog';
import { RoomSidebar } from './components/RoomSidebar';
import { ToastViewport } from './components/ToastViewport';
import { NotificationProvider } from './notifications/notification-context';
import { ToastProvider } from './notifications/toast-context';
import { WorkspaceGrid } from './components/WorkspaceGrid';
import { useAppShortcuts } from './hooks/use-app-shortcuts';
import type { AppShortcutActions } from './shortcuts';
import { useToast } from './notifications/toast-context';
import { useWorkspace } from './store/workspace-context';

const MOBILE_SIDEBAR_BREAKPOINT = 820;

export function getInitialSidebarOpen(viewportWidth: number) {
  return viewportWidth > MOBILE_SIDEBAR_BREAKPOINT;
}

export function App() {
  return (
    <ToastProvider>
      <NotificationProvider>
        <AppContent />
      </NotificationProvider>
    </ToastProvider>
  );
}

function AppContent() {
  const [addDialogOpen, setAddDialogOpen] = useState(false);
  const [groupManagerOpen, setGroupManagerOpen] = useState(false);
  const [danmakuSettingsOpen, setDanmakuSettingsOpen] = useState(false);
  const [monitoringOpen, setMonitoringOpen] = useState(false);
  const [workspaceOpen, setWorkspaceOpen] = useState(false);
  const sidebarOpen = useWorkspace((state) => state.sidebarOpen);
  const setSidebarOpen = useWorkspace((state) => state.setSidebarOpen);
  const primaryRoomId = useWorkspace((state) => state.primaryRoomId);
  const refreshStreamAvailability = useWorkspace((state) => state.refreshStreamAvailability);
  const { pushToast } = useToast();

  const openAddRoom = useCallback(() => setAddDialogOpen(true), []);
  const toggleSidebar = useCallback(() => setSidebarOpen(!sidebarOpen), [setSidebarOpen, sidebarOpen]);
  const togglePanel = useCallback((panel: 'danmaku' | 'monitoring' | 'workspace') => {
    setDanmakuSettingsOpen((open) => panel === 'danmaku' ? !open : false);
    setMonitoringOpen((open) => panel === 'monitoring' ? !open : false);
    setWorkspaceOpen((open) => panel === 'workspace' ? !open : false);
  }, []);
  const toggleDanmakuSettings = useCallback(() => togglePanel('danmaku'), [togglePanel]);
  const toggleMonitoring = useCallback(() => togglePanel('monitoring'), [togglePanel]);
  const toggleWorkspace = useCallback(() => togglePanel('workspace'), [togglePanel]);
  const shortcutActions = useMemo<AppShortcutActions>(() => ({
    addRoom: openAddRoom,
    toggleWorkspace,
    toggleMonitoring,
    toggleDanmakuSettings,
    toggleSidebar,
    hasPrimaryRoom: Boolean(primaryRoomId),
    refreshMainRoom: () => {
      if (primaryRoomId) return refreshStreamAvailability(primaryRoomId);
    },
  }), [openAddRoom, primaryRoomId, refreshStreamAvailability, toggleDanmakuSettings, toggleMonitoring, toggleSidebar, toggleWorkspace]);
  useAppShortcuts(shortcutActions, pushToast);

  return (
    <>
      <div className="app-shell">
        <AppHeader
          onAddRoom={openAddRoom}
          sidebarOpen={sidebarOpen}
          onToggleSidebar={toggleSidebar}
          danmakuSettingsOpen={danmakuSettingsOpen}
          onToggleDanmakuSettings={toggleDanmakuSettings}
          monitoringOpen={monitoringOpen}
          onToggleMonitoring={toggleMonitoring}
          workspaceOpen={workspaceOpen}
          onToggleWorkspace={toggleWorkspace}
        />
        <div className="app-body">
          <RoomSidebar
            isOpen={sidebarOpen}
            onAddRoom={openAddRoom}
            onManageGroups={() => setGroupManagerOpen(true)}
          />
          <WorkspaceGrid onAddRoom={openAddRoom} />
        </div>
        <AddRoomDialog open={addDialogOpen} onClose={() => setAddDialogOpen(false)} />
        <GroupManagerDialog open={groupManagerOpen} onClose={() => setGroupManagerOpen(false)} />
      </div>
      <ToastViewport />
    </>
  );
}
