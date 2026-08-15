import {
  Check,
  FolderOpen,
  Pencil,
  Save,
  Trash2,
  Upload,
  X,
} from 'lucide-react';
import { useEffect, useRef, useState, type FormEvent } from 'react';
import { getLayoutOption } from '../ui-model';
import { useWorkspace } from '../store/workspace-context';

interface WorkspacePresetPanelProps {
  open: boolean;
  onClose: () => void;
}

function formatPresetTime(value: string): string {
  const time = new Date(value);
  if (!Number.isFinite(time.getTime())) return '时间未知';
  return new Intl.DateTimeFormat('zh-CN', {
    month: 'numeric',
    day: 'numeric',
    hour: '2-digit',
    minute: '2-digit',
  }).format(time);
}

export function WorkspacePresetPanel({ open, onClose }: WorkspacePresetPanelProps) {
  const panelRef = useRef<HTMLDivElement>(null);
  const [nameDraft, setNameDraft] = useState('');
  const [error, setError] = useState<string>();
  const [busyId, setBusyId] = useState<string>();
  const presets = useWorkspace((state) => state.workspacePresets);
  const activeId = useWorkspace((state) => state.activeWorkspacePresetId);
  const hasUnsaved = useWorkspace((state) => state.hasUnsavedWorkspaceChanges);
  const rooms = useWorkspace((state) => state.rooms);
  const savePreset = useWorkspace((state) => state.saveWorkspacePreset);
  const updatePreset = useWorkspace((state) => state.updateWorkspacePreset);
  const loadPreset = useWorkspace((state) => state.loadWorkspacePreset);
  const renamePreset = useWorkspace((state) => state.renameWorkspacePreset);
  const deletePreset = useWorkspace((state) => state.deleteWorkspacePreset);

  useEffect(() => {
    if (!open) return undefined;
    const handlePointerDown = (event: PointerEvent) => {
      if (event.target instanceof Node && !panelRef.current?.contains(event.target)) onClose();
    };
    const handleKeyDown = (event: KeyboardEvent) => {
      if (event.key === 'Escape') onClose();
    };
    document.addEventListener('pointerdown', handlePointerDown);
    document.addEventListener('keydown', handleKeyDown);
    return () => {
      document.removeEventListener('pointerdown', handlePointerDown);
      document.removeEventListener('keydown', handleKeyDown);
    };
  }, [onClose, open]);

  if (!open) return null;

  const showError = (message: string) => {
    setError(message);
    window.setTimeout(() => setError(undefined), 2600);
  };

  const handleSave = (event: FormEvent<HTMLFormElement>) => {
    event.preventDefault();
    const id = savePreset(nameDraft);
    if (!id) {
      showError('名称不能为空、不能重复，且最多保存 20 个预设。');
      return;
    }
    setNameDraft('');
    setError(undefined);
  };

  const handleLoad = async (id: string) => {
    if (id !== activeId && hasUnsaved && !window.confirm('当前工作区有未保存变化，仍要加载此预设吗？')) return;
    setBusyId(id);
    const loaded = await loadPreset(id);
    setBusyId(undefined);
    if (!loaded) {
      showError('预设加载失败，当前工作区未改变。');
      return;
    }
    onClose();
  };

  const handleRename = (id: string, currentName: string) => {
    const nextName = window.prompt('重命名工作区预设', currentName);
    if (nextName === null) return;
    if (!renamePreset(id, nextName)) showError('名称不能为空、不能重复，且最多 40 个字符。');
  };

  const handleDelete = (id: string, currentName: string) => {
    if (!window.confirm(`删除工作区预设“${currentName}”？房间库、历史、收藏和分组不会受影响。`)) return;
    deletePreset(id);
  };

  return (
    <div
      className="workspace-presets-panel"
      id="workspace-presets-panel"
      ref={panelRef}
      role="dialog"
      aria-modal="false"
      aria-labelledby="workspace-presets-title"
    >
      <div className="workspace-presets-heading">
        <div>
          <span className="section-kicker">WORKSPACE PRESETS</span>
          <h2 id="workspace-presets-title">工作区</h2>
        </div>
        <button className="icon-button" type="button" aria-label="关闭工作区预设" title="关闭" onClick={onClose}>
          <X size={16} />
        </button>
      </div>

      <div className="workspace-presets-current">
        <span>当前工作区</span>
        <strong>{activeId ? presets.find((preset) => preset.id === activeId)?.name ?? '未保存工作区' : '未保存工作区'}</strong>
        {hasUnsaved ? <em>有未保存变化</em> : null}
      </div>

      <form className="workspace-presets-save" onSubmit={handleSave}>
        <label htmlFor="workspace-preset-name">保存当前工作区</label>
        <div>
          <input
            id="workspace-preset-name"
            value={nameDraft}
            maxLength={40}
            placeholder="输入预设名称"
            onChange={(event) => setNameDraft(event.currentTarget.value)}
          />
          <button className="button button-primary" type="submit" aria-label="保存当前工作区" title="保存当前工作区">
            <Save size={14} aria-hidden="true" />
            <span>保存</span>
          </button>
        </div>
      </form>

      {activeId ? (
        <button
          className="workspace-presets-update"
          type="button"
          disabled={!hasUnsaved}
          onClick={() => {
            if (!updatePreset(activeId)) showError('当前预设已不存在。');
          }}
        >
          <Upload size={14} aria-hidden="true" />
          <span>{hasUnsaved ? '更新当前预设' : '当前预设已同步'}</span>
        </button>
      ) : null}

      {error ? <p className="workspace-presets-error" role="alert">{error}</p> : null}

      {presets.length ? (
        <ul className="workspace-presets-list" aria-label="工作区预设列表">
          {presets.map((preset) => (
            <li className={`workspace-preset-item ${preset.id === activeId ? 'is-active' : ''}`} key={preset.id}>
              <div className="workspace-preset-copy">
                <strong title={preset.name}>{preset.name}</strong>
                <span>{preset.rooms.length} 个房间 · {getLayoutOption(preset.layoutId).shortLabel} · {formatPresetTime(preset.updatedAt)}</span>
              </div>
              <div className="workspace-preset-actions">
                {preset.id === activeId ? <Check size={14} aria-label="当前预设" /> : null}
                <button className="tiny-icon-button" type="button" aria-label={`${preset.name} 加载`} title="加载" disabled={busyId === preset.id} onClick={() => { void handleLoad(preset.id); }}>
                  <FolderOpen size={14} />
                </button>
                <button className="tiny-icon-button" type="button" aria-label={`${preset.name} 重命名`} title="重命名" onClick={() => handleRename(preset.id, preset.name)}>
                  <Pencil size={14} />
                </button>
                <button className="tiny-icon-button danger" type="button" aria-label={`${preset.name} 删除`} title="删除" onClick={() => handleDelete(preset.id, preset.name)}>
                  <Trash2 size={14} />
                </button>
              </div>
            </li>
          ))}
        </ul>
      ) : (
        <div className="workspace-presets-empty">
          <FolderOpen size={20} aria-hidden="true" />
          <strong>还没有工作区预设</strong>
          <span>保存当前房间、布局和弹幕设置，之后可以一键切换。</span>
        </div>
      )}
      <span className="workspace-presets-room-count" aria-hidden="true">当前 {rooms.length} 个房间</span>
    </div>
  );
}
