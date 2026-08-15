import { useEffect, useRef, useState } from 'react';
import {
  durationToSliderValue,
  parseDanmakuGovernanceSettings,
  resolveDanmakuGovernance,
  sliderValueToDuration,
  type DanmakuGovernanceSettings,
  type DanmakuSettings,
} from '../danmaku/danmaku-settings';
import {
  useDanmakuControls,
  useDanmakuGovernanceStats,
  useDanmakuRoom,
} from '../store/danmaku-context';
import { useWorkspace } from '../store/workspace-context';

interface DanmakuSettingsPanelProps {
  open: boolean;
  onClose: () => void;
}

interface Option<T extends string> {
  value: T;
  label: string;
}

type TabId = 'display' | 'governance' | 'stats';
type ScopeId = 'global' | string;

function OptionGroup<T extends string>({
  label,
  value,
  options,
  onChange,
}: {
  label: string;
  value: T;
  options: readonly Option<T>[];
  onChange: (value: T) => void;
}) {
  return (
    <div className="danmaku-setting-options">
      <span>{label}</span>
      <div role="group" aria-label={label}>
        {options.map((option) => (
          <button
            type="button"
            aria-pressed={value === option.value}
            key={option.value}
            onClick={() => onChange(option.value)}
          >
            {option.label}
          </button>
        ))}
      </div>
    </div>
  );
}

function StatCard({ label, value }: { label: string; value: string | number }) {
  return (
    <div className="danmaku-stat-card">
      <strong>{value}</strong>
      <span>{label}</span>
    </div>
  );
}

export function DanmakuSettingsPanel({ open, onClose }: DanmakuSettingsPanelProps) {
  const panelRef = useRef<HTMLDivElement>(null);
  const [activeTab, setActiveTab] = useState<TabId>('display');
  const [scope, setScope] = useState<ScopeId>('global');
  const [keywordDraft, setKeywordDraft] = useState('');
  const settings = useWorkspace((state) => state.danmakuSettings);
  const rooms = useWorkspace((state) => state.rooms);
  const overrides = useWorkspace((state) => state.danmakuGovernanceOverrides);
  const setSettings = useWorkspace((state) => state.setDanmakuSettings);
  const setGovernance = useWorkspace((state) => state.setDanmakuGovernance);
  const setRoomOverride = useWorkspace((state) => state.setRoomDanmakuGovernanceOverride);
  const clearRoomOverride = useWorkspace((state) => state.clearRoomDanmakuGovernanceOverride);
  const roomIdsKey = rooms.map((room) => room.roomId).join('|');
  const globalStats = useDanmakuGovernanceStats(roomIdsKey);
  const selectedRoom = useDanmakuRoom(scope === 'global' ? '' : scope);
  const { clearGovernanceStats } = useDanmakuControls();
  const resetDanmakuSetting = useWorkspace((state) => state.resetDanmakuSetting);
  const globalGovernance = settings.governance;
  const roomOverride = scope === 'global' ? undefined : overrides[scope];
  const governance = resolveDanmakuGovernance(globalGovernance, roomOverride);
  const stats = scope === 'global' ? globalStats : selectedRoom.governanceStats;

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

  useEffect(() => {
    if (scope !== 'global' && !rooms.some((room) => room.roomId === scope)) setScope('global');
  }, [rooms, scope]);

  if (!open) return null;

  const update = <K extends keyof DanmakuSettings>(key: K, value: DanmakuSettings[K]) => {
    setSettings({ [key]: value });
  };

  const updateGovernance = <K extends keyof DanmakuGovernanceSettings>(
    key: K,
    value: DanmakuGovernanceSettings[K],
  ) => {
    if (scope === 'global') setGovernance({ [key]: value });
    else setRoomOverride(scope, { [key]: value });
  };

  const addKeyword = (value: string) => {
    const trimmed = value.trim();
    if (!trimmed) return;
    const parsed = parseDanmakuGovernanceSettings({
      ...governance,
      keywordBlacklist: [...governance.keywordBlacklist, trimmed],
    });
    updateGovernance('keywordBlacklist', parsed.keywordBlacklist);
    setKeywordDraft('');
  };

  const removeKeyword = (keyword: string) => {
    updateGovernance(
      'keywordBlacklist',
      governance.keywordBlacklist.filter((item) => item !== keyword),
    );
  };

  const clearStats = () => {
    if (scope === 'global') {
      for (const room of rooms) clearGovernanceStats(room.roomId);
    } else {
      clearGovernanceStats(scope);
    }
  };

  return (
    <div
      className="danmaku-settings-panel"
      id="danmaku-settings-panel"
      ref={panelRef}
      role="dialog"
      aria-label="弹幕设置"
    >
      <div className="danmaku-settings-tabs" role="tablist" aria-label="弹幕设置分类">
        {([
          ['display', '显示'],
          ['governance', '治理'],
          ['stats', '统计'],
        ] as const).map(([tab, label]) => (
          <button
            type="button"
            role="tab"
            data-tab={tab}
            aria-selected={activeTab === tab}
            key={tab}
            onClick={() => setActiveTab(tab)}
          >
            {label}
          </button>
        ))}
      </div>

      <section hidden={activeTab !== 'display'} aria-label="显示设置">
        <label className="danmaku-setting-slider">
          <span>弹幕速度</span>
          <input
            type="range"
            min="0"
            max="100"
            step="1"
            value={durationToSliderValue(settings.durationSeconds)}
            onChange={(event) => update(
              'durationSeconds',
              sliderValueToDuration(Number(event.currentTarget.value)),
            )}
          />
          <button type="button" onClick={() => resetDanmakuSetting('durationSeconds')}>重置</button>
        </label>
        <label className="danmaku-setting-slider">
          <span>弹幕大小</span>
          <input
            type="range"
            min="14"
            max="36"
            step="1"
            value={settings.fontSize}
            onChange={(event) => update('fontSize', Number(event.currentTarget.value))}
          />
          <button type="button" onClick={() => resetDanmakuSetting('fontSize')}>重置</button>
        </label>
        <label className="danmaku-setting-slider">
          <span>不透明度</span>
          <input
            type="range"
            min="0.3"
            max="1"
            step="0.05"
            value={settings.opacity}
            onChange={(event) => update('opacity', Number(event.currentTarget.value))}
          />
          <button type="button" onClick={() => resetDanmakuSetting('opacity')}>重置</button>
        </label>

        <div className="danmaku-settings-divider" />
        <OptionGroup
          label="弹幕区域"
          value={settings.region}
          options={[
            { value: 'full', label: '全屏' },
            { value: 'top', label: '上方' },
            { value: 'bottom', label: '下方' },
          ]}
          onChange={(value) => update('region', value)}
        />
        <OptionGroup
          label="弹幕数量"
          value={settings.density}
          options={[
            { value: 'massive', label: '海量' },
            { value: 'normal', label: '正常' },
            { value: 'reduced', label: '精简' },
          ]}
          onChange={(value) => update('density', value)}
        />
        <OptionGroup
          label="弹幕字体"
          value={settings.fontFamily}
          options={[
            { value: 'simhei', label: '黑体' },
            { value: 'microsoft-yahei', label: '微软雅黑' },
          ]}
          onChange={(value) => update('fontFamily', value)}
        />
        <OptionGroup
          label="字体渲染"
          value={settings.rendering}
          options={[
            { value: 'native', label: '原生' },
            { value: 'advanced', label: '高级' },
          ]}
          onChange={(value) => update('rendering', value)}
        />
      </section>

      <section hidden={activeTab !== 'governance'} aria-label="弹幕治理">
        <label className="danmaku-governance-scope">
          <span>作用域</span>
          <select value={scope} onChange={(event) => setScope(event.currentTarget.value)}>
            <option value="global">全局默认</option>
            {rooms.map((room) => (
              <option value={room.roomId} key={room.roomId}>{room.anchorName}</option>
            ))}
          </select>
        </label>
        <div className="danmaku-governance-form">
          <label className="danmaku-toggle-row">
            <input
              type="checkbox"
              checked={governance.enabled}
              onChange={(event) => updateGovernance('enabled', event.currentTarget.checked)}
            />
            <span>启用弹幕治理</span>
          </label>
          <label className="danmaku-toggle-row">
            <input
              type="checkbox"
              checked={governance.peakProtectionEnabled}
              onChange={(event) => updateGovernance('peakProtectionEnabled', event.currentTarget.checked)}
            />
            <span>高峰保护</span>
          </label>
          <label className="danmaku-setting-slider">
            <span>重复抑制窗口</span>
            <input
              type="range"
              min="1"
              max="10"
              step="1"
              value={governance.duplicateWindowSeconds}
              onChange={(event) => updateGovernance('duplicateWindowSeconds', Number(event.currentTarget.value))}
            />
            <output>{governance.duplicateWindowSeconds} 秒</output>
          </label>
          <div className="danmaku-keyword-field">
            <span>关键词黑名单</span>
            <div className="danmaku-keyword-list">
              {governance.keywordBlacklist.map((keyword) => (
                <button
                  className="danmaku-keyword-chip"
                  type="button"
                  key={keyword}
                  aria-label={`移除关键词 ${keyword}`}
                  onClick={() => removeKeyword(keyword)}
                >
                  {keyword} ×
                </button>
              ))}
            </div>
            <input
              value={keywordDraft}
              placeholder="输入后按 Enter 添加"
              aria-label="添加关键词"
              onChange={(event) => setKeywordDraft(event.currentTarget.value)}
              onKeyDown={(event) => {
                if (event.key === 'Enter' || event.key === ',') {
                  event.preventDefault();
                  addKeyword(keywordDraft);
                }
              }}
            />
          </div>
        </div>
        {scope !== 'global' ? (
          <div className="danmaku-governance-actions">
            <span>{overrides[scope] ? '房间覆盖已启用' : '当前继承全局'}</span>
            <button type="button" onClick={() => clearRoomOverride(scope)}>继承全局</button>
          </div>
        ) : null}
      </section>

      <section hidden={activeTab !== 'stats'} aria-label="弹幕治理统计">
        <div className="danmaku-stats-heading">
          <span>当前状态</span>
          <strong className={`danmaku-level-${stats.level}`}>{stats.level === 'burst' ? '爆发' : stats.level === 'crowded' ? '拥挤' : '正常'}</strong>
        </div>
        <div className="danmaku-stat-grid">
          <StatCard label="近 60 秒速率" value={`${stats.recentRate.toFixed(2)} 条/秒`} />
          <StatCard label="最近峰值" value={`${stats.peakRate.toFixed(2)} 条/秒`} />
          <StatCard label="过滤" value={stats.filtered} />
          <StatCard label="重复" value={stats.duplicates} />
          <StatCard label="限流" value={stats.rateLimited} />
          <StatCard label="队列淘汰" value={stats.queueOverflow} />
        </div>
        <button className="danmaku-stats-clear" type="button" onClick={clearStats}>清零当前统计</button>
      </section>
    </div>
  );
}
