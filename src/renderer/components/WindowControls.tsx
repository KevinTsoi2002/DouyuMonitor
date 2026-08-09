import { Copy, Minus, Square, X } from 'lucide-react';
import { useEffect, useState } from 'react';
import type { AppApi } from '../../preload/bridge';

export type WindowControlApi = Pick<
  AppApi,
  | 'minimizeWindow'
  | 'toggleMaximizeWindow'
  | 'closeWindow'
  | 'onMaximizedChanged'
>;

interface WindowControlsProps {
  api?: WindowControlApi;
  initialMaximized?: boolean;
}

export function WindowControls({ api, initialMaximized = false }: WindowControlsProps) {
  const resolvedApi = api ?? (
    typeof window === 'undefined' ? undefined : window.appApi
  );
  const [maximized, setMaximized] = useState(initialMaximized);

  useEffect(() => {
    if (!resolvedApi) return undefined;
    return resolvedApi.onMaximizedChanged(setMaximized);
  }, [resolvedApi]);

  if (!resolvedApi) return null;
  const run = (operation: () => Promise<void>) => {
    void operation().catch(() => undefined);
  };

  return (
    <div className="window-controls" aria-label="窗口控制">
      <button
        type="button"
        aria-label="最小化"
        title="最小化"
        onClick={() => run(() => resolvedApi.minimizeWindow())}
      >
        <Minus size={16} />
      </button>
      <button
        type="button"
        aria-label={maximized ? '还原' : '最大化'}
        title={maximized ? '还原' : '最大化'}
        onClick={() => run(() => resolvedApi.toggleMaximizeWindow())}
      >
        {maximized ? <Copy size={14} /> : <Square size={14} />}
      </button>
      <button
        className="window-control-close"
        type="button"
        aria-label="关闭"
        title="关闭"
        onClick={() => run(() => resolvedApi.closeWindow())}
      >
        <X size={17} />
      </button>
    </div>
  );
}
