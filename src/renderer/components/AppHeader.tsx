import { Activity, LayoutGrid, Menu, MessageCircle, MessageCircleOff, PanelLeftOpen, PanelsTopLeft, Plus, Radio, Settings2, Volume2, VolumeX, X } from 'lucide-react';
import { LayoutMenu } from './LayoutMenu';
import { DanmakuSettingsPanel } from './DanmakuSettingsPanel';
import { MonitoringStatusPanel } from './MonitoringStatusPanel';
import { WorkspacePresetPanel } from './WorkspacePresetPanel';
import { WindowControls } from './WindowControls';
import { getMonitoringSummary } from '../monitoring-status';
import { useDanmakuIssueCount } from '../store/danmaku-context';
import { getLayoutOption } from '../ui-model';
import { useWorkspace } from '../store/workspace-context';

interface AppHeaderProps {
  onAddRoom: () => void;
  sidebarOpen: boolean;
  onToggleSidebar: () => void;
  danmakuSettingsOpen: boolean;
  onToggleDanmakuSettings: () => void;
  monitoringOpen: boolean;
  onToggleMonitoring: () => void;
  workspaceOpen: boolean;
  onToggleWorkspace: () => void;
}

export function AppHeader({
  onAddRoom,
  sidebarOpen,
  onToggleSidebar,
  danmakuSettingsOpen,
  onToggleDanmakuSettings,
  monitoringOpen,
  onToggleMonitoring,
  workspaceOpen,
  onToggleWorkspace,
}: AppHeaderProps) {
  const rooms = useWorkspace((state) => state.rooms);
  const roomCount = rooms.length;
  const roomIdsKey = rooms.map((room) => room.roomId).join('|');
  const playbackIssueCount = getMonitoringSummary(rooms).playbackIssues;
  const danmakuIssueCount = useDanmakuIssueCount(roomIdsKey);
  const monitoringIssueCount = playbackIssueCount + danmakuIssueCount;
  const layoutId = useWorkspace((state) => state.layoutId);
  const globalDanmakuEnabled = useWorkspace((state) => state.globalDanmakuEnabled);
  const globalMuted = useWorkspace((state) => state.globalMuted);
  const audioMode = useWorkspace((state) => state.audioMode);
  const activeWorkspacePresetId = useWorkspace((state) => state.activeWorkspacePresetId);
  const workspacePresets = useWorkspace((state) => state.workspacePresets);
  const setGlobalDanmakuEnabled = useWorkspace((state) => state.setGlobalDanmakuEnabled);
  const setGlobalMuted = useWorkspace((state) => state.setGlobalMuted);
  const setAudioMode = useWorkspace((state) => state.setAudioMode);
  const setLayout = useWorkspace((state) => state.setLayout);
  const workspaceName = workspacePresets.find((preset) => preset.id === activeWorkspacePresetId)?.name ?? '未保存工作区';

  return (
    <>
    <header className="app-header">
      <div className="header-leading">
        <button
          className="icon-button"
          type="button"
          aria-label={sidebarOpen ? '收起房间列表' : '展开房间列表'}
          aria-expanded={sidebarOpen}
          title={sidebarOpen ? '收起房间列表' : '展开房间列表'}
          onClick={onToggleSidebar}
        >
          {sidebarOpen ? <X size={18} /> : <Menu size={18} />}
        </button>
        <div className="brand-mark" aria-hidden="true"><Radio size={19} /></div>
        <p className="brand-name">斗鱼视界</p>
      </div>

      <div className="header-actions">
        <button
          className={`icon-button header-toggle ${globalDanmakuEnabled ? 'is-active' : ''}`}
          type="button"
          aria-label={globalDanmakuEnabled ? '关闭全局弹幕' : '开启全局弹幕'}
          aria-pressed={globalDanmakuEnabled}
          title={globalDanmakuEnabled ? '关闭全局弹幕' : '开启全局弹幕'}
          onClick={() => setGlobalDanmakuEnabled(!globalDanmakuEnabled)}
        >
          {globalDanmakuEnabled ? <MessageCircle size={16} /> : <MessageCircleOff size={16} />}
        </button>
        <div className="danmaku-settings-wrap">
          <button
            className="icon-button header-toggle"
            type="button"
            aria-label={danmakuSettingsOpen ? '关闭弹幕设置' : '打开弹幕设置'}
            aria-expanded={danmakuSettingsOpen}
            aria-controls="danmaku-settings-panel"
            title="弹幕设置"
          onClick={onToggleDanmakuSettings}
          >
            <Settings2 size={16} />
          </button>
          <DanmakuSettingsPanel
            open={danmakuSettingsOpen}
          onClose={onToggleDanmakuSettings}
          />
        </div>
        <button
          className={`icon-button header-toggle ${monitoringOpen ? 'is-active' : ''}`}
          type="button"
          aria-label={monitoringOpen ? '关闭监控状态' : '打开监控状态'}
          aria-expanded={monitoringOpen}
          aria-controls="monitoring-status-panel"
          title="监控状态"
          onClick={onToggleMonitoring}
        >
          <Activity size={16} />
          {monitoringIssueCount ? <span className="monitoring-header-badge" aria-label={`${monitoringIssueCount} 个监控异常`}>{monitoringIssueCount}</span> : null}
        </button>
        <div className="workspace-presets-wrap">
          <button
            className={`icon-button header-toggle workspace-trigger ${workspaceOpen ? 'is-active' : ''}`}
            type="button"
            aria-label={`工作区：${workspaceName}`}
            aria-expanded={workspaceOpen}
            aria-controls="workspace-presets-panel"
            title={workspaceName}
          onClick={onToggleWorkspace}
          >
            <PanelsTopLeft size={16} />
          </button>
          <WorkspacePresetPanel open={workspaceOpen} onClose={onToggleWorkspace} />
        </div>
        <button
          className={`icon-button header-toggle ${globalMuted ? 'is-active' : ''}`}
          type="button"
          aria-label={globalMuted ? '取消全局静音' : '全局静音'}
          aria-pressed={globalMuted}
          title={globalMuted ? '取消全局静音' : '全局静音'}
          onClick={() => setGlobalMuted(!globalMuted)}
        >
          {globalMuted ? <VolumeX size={16} /> : <Volume2 size={16} />}
        </button>
        <div className="audio-mode-control" role="group" aria-label="声音播放模式">
          <button
            className={`audio-mode-button ${audioMode === 'single' ? 'is-active' : ''}`}
            type="button"
            aria-label="单声道"
            aria-pressed={audioMode === 'single'}
            title="单声道：只播放选中房间"
            onClick={() => setAudioMode('single')}
          >
            单声道
          </button>
          <button
            className={`audio-mode-button ${audioMode === 'multi' ? 'is-active' : ''}`}
            type="button"
            aria-label="多声道"
            aria-pressed={audioMode === 'multi'}
            title="多声道：同时播放可用房间"
            onClick={() => setAudioMode('multi')}
          >
            多声道
          </button>
        </div>
        <div className="layout-control">
          <LayoutGrid size={16} aria-hidden="true" />
          <span className="layout-control-label">{getLayoutOption(layoutId).shortLabel}</span>
          {roomCount > 1 && layoutId !== 'primary-two' ? (
            <button
              className="layout-primary-shortcut"
              type="button"
              aria-label="进入主画面布局并调整大小"
              title="进入主画面布局并调整大小"
              onClick={() => setLayout('primary-two')}
            >
              <PanelLeftOpen size={14} aria-hidden="true" />
            </button>
          ) : null}
          <LayoutMenu />
        </div>
        <button
          className="button button-primary"
          type="button"
          aria-label="添加直播间"
          title="添加直播间"
          onClick={onAddRoom}
        >
          <Plus size={16} aria-hidden="true" />
          <span>添加直播间</span>
        </button>
        <WindowControls />
      </div>
    </header>
    {monitoringOpen ? <MonitoringStatusPanel open onClose={onToggleMonitoring} /> : null}
    </>
  );
}
