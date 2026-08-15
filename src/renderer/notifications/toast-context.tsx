import { createContext, useCallback, useContext, useEffect, useRef, useReducer, type PropsWithChildren } from 'react';

export type ToastLevel = 'info' | 'success' | 'warning' | 'error';

export interface ToastInput {
  level: ToastLevel;
  message: string;
  durationMs?: number;
  action?: { label: string; run: () => void };
}

export interface ToastItem extends ToastInput {
  id: string;
  createdAt: number;
}

export type ToastAction =
  | { type: 'push'; item: ToastItem }
  | { type: 'dismiss'; id: string }
  | { type: 'clear' };

export const DEFAULT_TOAST_DURATION_MS = 3500;
export const DEFAULT_ERROR_TOAST_DURATION_MS = 6000;
export const MAX_VISIBLE_TOASTS = 3;

export function toastReducer(state: ToastItem[], action: ToastAction): ToastItem[] {
  if (action.type === 'clear') return [];
  if (action.type === 'dismiss') {
    const next = state.filter((item) => item.id !== action.id);
    return next.length === state.length ? state : next;
  }
  return [...state, action.item].slice(-MAX_VISIBLE_TOASTS);
}

interface ToastContextValue {
  toasts: ToastItem[];
  pushToast(input: ToastInput): string;
  dismissToast(id: string): void;
  clearToasts(): void;
}

const ToastContext = createContext<ToastContextValue | null>(null);

export function ToastProvider({ children }: PropsWithChildren) {
  const [toasts, dispatch] = useReducer(toastReducer, []);
  const timersRef = useRef(new Map<string, ReturnType<typeof setTimeout>>());
  const idRef = useRef(0);

  const dismissToast = useCallback((id: string) => {
    const timer = timersRef.current.get(id);
    if (timer) clearTimeout(timer);
    timersRef.current.delete(id);
    dispatch({ type: 'dismiss', id });
  }, []);

  const pushToast = useCallback((input: ToastInput) => {
    const id = `toast-${Date.now()}-${idRef.current++}`;
    const durationMs = input.durationMs
      ?? (input.level === 'error' ? DEFAULT_ERROR_TOAST_DURATION_MS : DEFAULT_TOAST_DURATION_MS);
    dispatch({
      type: 'push',
      item: { ...input, durationMs, id, createdAt: Date.now() },
    });
    const timer = setTimeout(() => dismissToast(id), durationMs);
    timersRef.current.set(id, timer);
    return id;
  }, [dismissToast]);

  const clearToasts = useCallback(() => {
    for (const timer of timersRef.current.values()) clearTimeout(timer);
    timersRef.current.clear();
    dispatch({ type: 'clear' });
  }, []);

  useEffect(() => () => {
    for (const timer of timersRef.current.values()) clearTimeout(timer);
    timersRef.current.clear();
  }, []);

  return (
    <ToastContext.Provider value={{ toasts, pushToast, dismissToast, clearToasts }}>
      {children}
    </ToastContext.Provider>
  );
}

export function useToast(): ToastContextValue {
  const context = useContext(ToastContext);
  if (!context) throw new Error('ToastProvider is required');
  return context;
}
