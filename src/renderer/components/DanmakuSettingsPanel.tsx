import { useEffect, useRef } from 'react';
import {
  durationToSliderValue,
  sliderValueToDuration,
  type DanmakuSettings,
} from '../danmaku/danmaku-settings';
import { useWorkspace } from '../store/workspace-context';

interface DanmakuSettingsPanelProps {
  open: boolean;
  onClose: () => void;
}

interface Option<T extends string> {
  value: T;
  label: string;
}

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

export function DanmakuSettingsPanel({ open, onClose }: DanmakuSettingsPanelProps) {
  const panelRef = useRef<HTMLDivElement>(null);
  const settings = useWorkspace((state) => state.danmakuSettings);
  const setSettings = useWorkspace((state) => state.setDanmakuSettings);
  const resetSetting = useWorkspace((state) => state.resetDanmakuSetting);

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
  const update = <K extends keyof DanmakuSettings>(key: K, value: DanmakuSettings[K]) => {
    setSettings({ [key]: value });
  };

  return (
    <div
      className="danmaku-settings-panel"
      id="danmaku-settings-panel"
      ref={panelRef}
      role="dialog"
      aria-label="弹幕设置"
    >
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
        <button type="button" onClick={() => resetSetting('durationSeconds')}>重置</button>
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
        <button type="button" onClick={() => resetSetting('fontSize')}>重置</button>
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
        <button type="button" onClick={() => resetSetting('opacity')}>重置</button>
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
    </div>
  );
}
