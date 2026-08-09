import {
  useCallback,
  useEffect,
  useRef,
  useState,
  type CSSProperties,
  type RefObject,
} from 'react';
import type { DanmakuMessage } from '../../shared/danmaku-contract';
import {
  calculateLanes,
  selectLane,
} from '../danmaku/danmaku-lane-scheduler';
import {
  getDanmakuDensityProfile,
  type DanmakuFontFamily,
  type DanmakuSettings,
} from '../danmaku/danmaku-settings';
import { useDanmakuRoom, useDanmakuTake } from '../store/danmaku-context';

interface DanmakuOverlayProps {
  roomId: string;
  enabled: boolean;
  settings: DanmakuSettings;
}

interface ElementSize {
  width: number;
  height: number;
}

export interface LaunchedDanmaku {
  message: DanmakuMessage;
  laneIndex: number;
  top: number;
  width: number;
  containerWidth: number;
  launchedAt: number;
  durationMs: number;
  fontSize: number;
}

function getFontFamily(fontFamily: DanmakuFontFamily): string {
  return fontFamily === 'simhei'
    ? 'SimHei, sans-serif'
    : '"Microsoft YaHei", sans-serif';
}

function estimateMessageWidth(message: DanmakuMessage, fontSize: number): number {
  return Math.max(fontSize, Array.from(`${message.nickname}：${message.text}`).length * fontSize);
}

function measureMessageWidth(
  message: DanmakuMessage,
  fontSize: number,
  fontFamily: DanmakuFontFamily,
): number {
  if (typeof document === 'undefined' || !document.body) {
    return estimateMessageWidth(message, fontSize);
  }

  const measure = document.createElement('span');
  measure.textContent = `${message.nickname}：${message.text}`;
  Object.assign(measure.style, {
    position: 'fixed',
    top: '-10000px',
    left: '-10000px',
    width: 'max-content',
    fontFamily: getFontFamily(fontFamily),
    fontSize: `${fontSize}px`,
    fontWeight: '700',
    lineHeight: '1.35',
    whiteSpace: 'nowrap',
    visibility: 'hidden',
  });

  try {
    document.body.append(measure);
    const measuredWidth = Math.ceil(measure.getBoundingClientRect().width);
    return measuredWidth > 0
      ? measuredWidth
      : estimateMessageWidth(message, fontSize);
  } catch {
    return estimateMessageWidth(message, fontSize);
  } finally {
    measure.remove();
  }
}

function useElementSize(ref: RefObject<HTMLElement | null>): ElementSize {
  const [size, setSize] = useState<ElementSize>({ width: 0, height: 0 });

  useEffect(() => {
    const element = ref.current;
    if (!element) return undefined;
    const update = () => {
      const bounds = element.getBoundingClientRect();
      setSize({ width: Math.max(0, bounds.width), height: Math.max(0, bounds.height) });
    };
    update();

    if (typeof ResizeObserver !== 'undefined') {
      const observer = new ResizeObserver(update);
      observer.observe(element);
      return () => observer.disconnect();
    }

    window.addEventListener('resize', update);
    return () => window.removeEventListener('resize', update);
  }, [ref]);

  return size;
}

export function DanmakuOverlay({ roomId, enabled, settings }: DanmakuOverlayProps) {
  const room = useDanmakuRoom(roomId);
  const takePending = useDanmakuTake();
  const overlayRef = useRef<HTMLDivElement>(null);
  const size = useElementSize(overlayRef);
  const [active, setActive] = useState<LaunchedDanmaku[]>([]);
  const activeRef = useRef<LaunchedDanmaku[]>([]);
  const lastLaunchRef = useRef(0);
  const [scheduleRevision, setScheduleRevision] = useState(0);

  useEffect(() => {
    activeRef.current = active;
  }, [active]);

  useEffect(() => {
    if (enabled) return;
    activeRef.current = [];
    lastLaunchRef.current = 0;
    setActive([]);
  }, [enabled]);

  const nextMessage = room.pending[0];
  const densityProfile = getDanmakuDensityProfile(settings.density);
  useEffect(() => {
    if (!enabled || !nextMessage || size.width <= 0 || size.height <= 0) return undefined;
    const now = Date.now();
    const delay = Math.max(0, lastLaunchRef.current + densityProfile.intervalMs - now);
    const timer = globalThis.setTimeout(() => {
      const launchedAt = Date.now();
      const width = measureMessageWidth(nextMessage, settings.fontSize, settings.fontFamily);
      const durationMs = settings.durationSeconds * 1000;
      const lanes = calculateLanes(
        size.height,
        settings.fontSize,
        settings.region,
        settings.density,
      );
      const lane = selectLane(lanes, activeRef.current, {
        width,
        containerWidth: size.width,
        launchedAt,
        durationMs,
        fontSize: settings.fontSize,
      });

      if (lane && takePending(roomId, nextMessage.id)) {
        const launched: LaunchedDanmaku = {
          message: nextMessage,
          laneIndex: lane.index,
          top: lane.top,
          width,
          containerWidth: size.width,
          launchedAt,
          durationMs,
          fontSize: settings.fontSize,
        };
        lastLaunchRef.current = launchedAt;
        activeRef.current = [...activeRef.current, launched];
        setActive(activeRef.current);
      }
      setScheduleRevision((value) => value + 1);
    }, delay);

    return () => globalThis.clearTimeout(timer);
  }, [
    densityProfile.intervalMs,
    enabled,
    nextMessage,
    roomId,
    scheduleRevision,
    settings.density,
    settings.durationSeconds,
    settings.fontFamily,
    settings.fontSize,
    settings.region,
    size.height,
    size.width,
    takePending,
  ]);

  useEffect(() => {
    if (active.length === 0) return undefined;
    const nextExpiry = Math.min(...active.map((item) => (
      item.launchedAt + item.durationMs + 1000
    )));
    const timer = globalThis.setTimeout(() => {
      const now = Date.now();
      setActive((current) => current.filter((item) => (
        item.launchedAt + item.durationMs + 1000 > now
      )));
    }, Math.max(0, nextExpiry - Date.now()));
    return () => globalThis.clearTimeout(timer);
  }, [active]);

  const expire = useCallback((messageId: string) => {
    setActive((current) => current.filter((item) => item.message.id !== messageId));
  }, []);

  if (!enabled) return null;
  return (
    <DanmakuLines
      messages={active}
      settings={settings}
      onExpire={expire}
      overlayRef={overlayRef}
    />
  );
}

export function DanmakuLines({
  messages,
  settings,
  onExpire,
  overlayRef,
}: {
  messages: LaunchedDanmaku[];
  settings: DanmakuSettings;
  onExpire: (messageId: string) => void;
  overlayRef?: RefObject<HTMLDivElement | null>;
}) {
  const fontFamily = getFontFamily(settings.fontFamily);
  return (
    <div ref={overlayRef} className="danmaku-overlay" aria-label="弹幕" aria-live="off">
      {messages.map((item) => (
        <span
          className={`danmaku-line danmaku-rendering-${settings.rendering}`}
          data-message-id={item.message.id}
          key={item.message.id}
          onAnimationEnd={() => onExpire(item.message.id)}
          style={{
            '--danmaku-top': `${item.top}px`,
            '--danmaku-duration': `${item.durationMs}ms`,
            '--danmaku-travel': `${-(item.containerWidth + item.width)}px`,
            '--danmaku-font-size': `${item.fontSize}px`,
            '--danmaku-font-family': fontFamily,
            '--danmaku-opacity': settings.opacity,
          } as CSSProperties}
        >
          <strong>{item.message.nickname}：</strong>{item.message.text}
        </span>
      ))}
    </div>
  );
}
