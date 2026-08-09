import { Check, ChevronDown, LayoutGrid } from 'lucide-react';
import { useState } from 'react';
import { useWorkspace } from '../store/workspace-context';
import { LAYOUT_OPTIONS } from '../ui-model';

export function LayoutMenu() {
  const [open, setOpen] = useState(false);
  const layoutId = useWorkspace((state) => state.layoutId);
  const setLayout = useWorkspace((state) => state.setLayout);

  return (
    <div className="layout-menu-wrap">
      <button
        className="layout-menu-trigger"
        type="button"
        aria-label="选择画面布局"
        aria-expanded={open}
        onClick={() => setOpen((value) => !value)}
      >
        <ChevronDown size={14} aria-hidden="true" />
      </button>
      {open ? (
        <div className="layout-menu" role="menu" aria-label="画面布局">
          <div className="layout-menu-heading"><LayoutGrid size={14} /> 画面布局</div>
          {LAYOUT_OPTIONS.map((option) => (
            <button
              key={option.id}
              className={`layout-option ${option.id === layoutId ? 'is-selected' : ''}`}
              type="button"
              role="menuitemradio"
              aria-checked={option.id === layoutId}
              onClick={() => {
                setLayout(option.id);
                setOpen(false);
              }}
            >
              <span className="layout-option-icon">{option.shortLabel}</span>
              <span className="layout-option-copy">
                <strong>{option.label}</strong>
                <small>{option.hint}</small>
              </span>
              {option.id === layoutId ? <Check size={15} aria-hidden="true" /> : null}
            </button>
          ))}
        </div>
      ) : null}
    </div>
  );
}

