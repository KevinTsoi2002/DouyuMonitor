import { LayoutGrid, Menu, MessageCircle, MessageCircleOff, Plus, Radio, Settings2, Volume2, VolumeX, X } from 'lucide-react';
import { useState } from 'react';
import { LayoutMenu } from './LayoutMenu';
import { DanmakuSettingsPanel } from './DanmakuSettingsPanel';
import { WindowControls } from './WindowControls';
import { getLayoutOption } from '../ui-model';
import { useWorkspace } from '../store/workspace-context';

interface AppHeaderProps {
  onAddRoom: () => void;
  sidebarOpen: boolean;
  onToggleSidebar: () => void;
}

export function AppHeader({ onAddRoom, sidebarOpen, onToggleSidebar }: AppHeaderProps) {
  const [danmakuSettingsOpen, setDanmakuSettingsOpen] = useState(false);
  const layoutId = useWorkspace((state) => state.layoutId);
  const globalDanmakuEnabled = useWorkspace((state) => state.globalDanmakuEnabled);
  const globalMuted = useWorkspace((state) => state.globalMuted);
  const setGlobalDanmakuEnabled = useWorkspace((state) => state.setGlobalDanmakuEnabled);
  const setGlobalMuted = useWorkspace((state) => state.setGlobalMuted);

  return (
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
            onClick={() => setDanmakuSettingsOpen((open) => !open)}
          >
            <Settings2 size={16} />
          </button>
          <DanmakuSettingsPanel
            open={danmakuSettingsOpen}
            onClose={() => setDanmakuSettingsOpen(false)}
          />
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
        <div className="layout-control">
          <LayoutGrid size={16} aria-hidden="true" />
          <span className="layout-control-label">{getLayoutOption(layoutId).shortLabel}</span>
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
  );
}
