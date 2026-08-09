import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';
import { createMockDanmakuSource } from '../infrastructure/mock-danmaku-source';
import { createRendererDouyuAdapter } from '../infrastructure/renderer-douyu-adapter';
import { createRendererDanmakuSource } from '../infrastructure/renderer-danmaku-source';
import { App, getInitialSidebarOpen } from './App';
import { getInitialRoomsForRuntime } from './runtime-mode';
import { DanmakuProvider } from './store/danmaku-context';
import { WorkspaceProvider } from './store/workspace-context';
import './styles.css';

const root = document.getElementById('root');
if (!root) throw new Error('Missing root element');
const appApi = typeof window === 'undefined' ? undefined : window.appApi;
const electronMode = Boolean(appApi);
const danmakuSource = appApi
  ? createRendererDanmakuSource(appApi)
  : createMockDanmakuSource();

createRoot(root).render(
  <StrictMode>
    <WorkspaceProvider
      adapter={createRendererDouyuAdapter()}
      demoMode={!electronMode}
      initialRooms={getInitialRoomsForRuntime(electronMode)}
      initialSidebarOpen={getInitialSidebarOpen(window.innerWidth)}
    >
      <DanmakuProvider source={danmakuSource}>
        <App />
      </DanmakuProvider>
    </WorkspaceProvider>
  </StrictMode>,
);
