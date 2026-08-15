import { useState } from 'react';
import { AppHeader } from './components/AppHeader';
import { AddRoomDialog } from './components/AddRoomDialog';
import { GroupManagerDialog } from './components/GroupManagerDialog';
import { RoomSidebar } from './components/RoomSidebar';
import { ToastViewport } from './components/ToastViewport';
import { NotificationProvider } from './notifications/notification-context';
import { ToastProvider } from './notifications/toast-context';
import { WorkspaceGrid } from './components/WorkspaceGrid';
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
  const sidebarOpen = useWorkspace((state) => state.sidebarOpen);
  const setSidebarOpen = useWorkspace((state) => state.setSidebarOpen);

  return (
    <>
      <div className="app-shell">
        <AppHeader
          onAddRoom={() => setAddDialogOpen(true)}
          sidebarOpen={sidebarOpen}
          onToggleSidebar={() => setSidebarOpen(!sidebarOpen)}
        />
        <div className="app-body">
          <RoomSidebar
            isOpen={sidebarOpen}
            onAddRoom={() => setAddDialogOpen(true)}
            onManageGroups={() => setGroupManagerOpen(true)}
          />
          <WorkspaceGrid onAddRoom={() => setAddDialogOpen(true)} />
        </div>
        <AddRoomDialog open={addDialogOpen} onClose={() => setAddDialogOpen(false)} />
        <GroupManagerDialog open={groupManagerOpen} onClose={() => setGroupManagerOpen(false)} />
      </div>
      <ToastViewport />
    </>
  );
}
