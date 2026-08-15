import { AlertCircle, CheckCircle2, Info, TriangleAlert, X } from 'lucide-react';
import { useToast, type ToastLevel } from '../notifications/toast-context';

const LEVEL_ICONS: Record<ToastLevel, typeof Info> = {
  info: Info,
  success: CheckCircle2,
  warning: TriangleAlert,
  error: AlertCircle,
};

export function ToastViewport() {
  const { toasts, dismissToast } = useToast();

  return (
    <div className="toast-viewport" aria-label="应用通知">
      {toasts.map((toast) => {
        const Icon = LEVEL_ICONS[toast.level];
        return (
          <div
            className={`toast-item is-${toast.level}`}
            key={toast.id}
            role={toast.level === 'error' ? 'alert' : 'status'}
            data-toast-id={toast.id}
          >
            <Icon className="toast-icon" size={16} aria-hidden="true" />
            <span className="toast-message">{toast.message}</span>
            {toast.action ? (
              <button
                className="toast-action"
                type="button"
                onClick={() => {
                  try {
                    toast.action?.run();
                  } finally {
                    dismissToast(toast.id);
                  }
                }}
              >
                {toast.action.label}
              </button>
            ) : null}
            <button
              className="toast-dismiss icon-button"
              type="button"
              aria-label="关闭通知"
              title="关闭通知"
              onClick={() => dismissToast(toast.id)}
            >
              <X size={14} aria-hidden="true" />
            </button>
          </div>
        );
      })}
    </div>
  );
}
