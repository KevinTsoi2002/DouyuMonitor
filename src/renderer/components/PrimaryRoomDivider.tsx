import { useEffect, useRef, useState, type KeyboardEvent, type PointerEvent } from 'react';
import {
  DEFAULT_PRIMARY_ROOM_RATIO,
  PRIMARY_ROOM_RATIO_MAX,
  PRIMARY_ROOM_RATIO_MIN,
  PRIMARY_ROOM_RATIOS,
  type PrimaryLayoutOrientation,
  type PrimaryRoomRatio,
} from '../../domain/layout-engine';

interface PrimaryRoomDividerProps {
  orientation: PrimaryLayoutOrientation;
  value: number;
  availableRatios: PrimaryRoomRatio[];
  onPreviewChange: (ratio: number | undefined) => void;
  onCommit: (ratio: PrimaryRoomRatio) => void;
  onDragStateChange: (dragging: boolean) => void;
}

export function clampPrimaryRoomPreview(value: number): number {
  return Math.min(PRIMARY_ROOM_RATIO_MAX, Math.max(PRIMARY_ROOM_RATIO_MIN, value));
}

export function snapPrimaryRoomRatio(
  value: number,
  availableRatios: readonly PrimaryRoomRatio[],
): PrimaryRoomRatio {
  if (availableRatios.length === 0) return DEFAULT_PRIMARY_ROOM_RATIO;
  const ratios = availableRatios;
  return ratios.reduce((closest, ratio) => (
    Math.abs(ratio - value) < Math.abs(closest - value) ? ratio : closest
  ));
}

export function stepPrimaryRoomRatio(
  value: number,
  availableRatios: readonly PrimaryRoomRatio[],
  direction: -1 | 1,
): PrimaryRoomRatio {
  const ratios = availableRatios.length > 0 ? [...availableRatios] : [...PRIMARY_ROOM_RATIOS];
  const current = snapPrimaryRoomRatio(value, ratios);
  const currentIndex = ratios.indexOf(current);
  const nextIndex = Math.min(ratios.length - 1, Math.max(0, currentIndex + direction));
  return ratios[nextIndex];
}

export function PrimaryRoomDivider({
  orientation,
  value,
  availableRatios,
  onPreviewChange,
  onCommit,
  onDragStateChange,
}: PrimaryRoomDividerProps) {
  const [previewRatio, setPreviewRatio] = useState<number>();
  const frameRef = useRef<number | undefined>(undefined);
  const pointerIdRef = useRef<number | undefined>(undefined);

  const cancelFrame = () => {
    if (frameRef.current !== undefined) cancelAnimationFrame(frameRef.current);
    frameRef.current = undefined;
  };

  const finishPreview = () => {
    cancelFrame();
    pointerIdRef.current = undefined;
    setPreviewRatio(undefined);
    onPreviewChange(undefined);
    onDragStateChange(false);
  };

  const ratioFromPointer = (event: PointerEvent<HTMLDivElement>): number => {
    const bounds = event.currentTarget.parentElement?.getBoundingClientRect();
    if (!bounds || bounds.width <= 0 || bounds.height <= 0) return value;
    const ratio = orientation === 'horizontal'
      ? (event.clientX - bounds.left) / bounds.width
      : (event.clientY - bounds.top) / bounds.height;
    return clampPrimaryRoomPreview(ratio);
  };

  const publishPreview = (ratio: number) => {
    cancelFrame();
    frameRef.current = requestAnimationFrame(() => {
      frameRef.current = undefined;
      setPreviewRatio(ratio);
      onPreviewChange(ratio);
    });
  };

  const handlePointerDown = (event: PointerEvent<HTMLDivElement>) => {
    if (event.button !== 0) return;
    event.preventDefault();
    pointerIdRef.current = event.pointerId;
    event.currentTarget.setPointerCapture(event.pointerId);
    onDragStateChange(true);
    publishPreview(ratioFromPointer(event));
  };

  const handlePointerMove = (event: PointerEvent<HTMLDivElement>) => {
    if (pointerIdRef.current !== event.pointerId) return;
    publishPreview(ratioFromPointer(event));
  };

  const handlePointerUp = (event: PointerEvent<HTMLDivElement>) => {
    if (pointerIdRef.current !== event.pointerId) return;
    const nextRatio = snapPrimaryRoomRatio(ratioFromPointer(event), availableRatios);
    if (event.currentTarget.hasPointerCapture(event.pointerId)) {
      event.currentTarget.releasePointerCapture(event.pointerId);
    }
    finishPreview();
    onCommit(nextRatio);
  };

  const handleKeyDown = (event: KeyboardEvent<HTMLDivElement>) => {
    if (event.key === 'Escape' && pointerIdRef.current !== undefined) {
      event.preventDefault();
      if (event.currentTarget.hasPointerCapture(pointerIdRef.current)) {
        event.currentTarget.releasePointerCapture(pointerIdRef.current);
      }
      finishPreview();
      return;
    }
    const smaller = event.key === 'ArrowLeft' || event.key === 'ArrowUp';
    const larger = event.key === 'ArrowRight' || event.key === 'ArrowDown';
    if (smaller || larger) {
      event.preventDefault();
      onCommit(stepPrimaryRoomRatio(value, availableRatios, smaller ? -1 : 1));
      return;
    }
    const ratios = availableRatios.length > 0 ? availableRatios : [...PRIMARY_ROOM_RATIOS];
    if (event.key === 'Home') {
      event.preventDefault();
      onCommit(ratios[0]);
    }
    if (event.key === 'End') {
      event.preventDefault();
      onCommit(ratios[ratios.length - 1]);
    }
  };

  useEffect(() => () => {
    cancelFrame();
    onPreviewChange(undefined);
    onDragStateChange(false);
  }, [orientation]);

  const renderedValue = previewRatio ?? value;
  return (
    <div
      className={`primary-room-divider is-${orientation}`}
      role="separator"
      tabIndex={0}
      aria-label="调整主画面大小"
      aria-orientation={orientation === 'horizontal' ? 'vertical' : 'horizontal'}
      aria-valuemin={Math.round(PRIMARY_ROOM_RATIO_MIN * 100)}
      aria-valuemax={Math.round(PRIMARY_ROOM_RATIO_MAX * 100)}
      aria-valuenow={Math.round(renderedValue * 100)}
      title="调整主画面大小"
      onPointerDown={handlePointerDown}
      onPointerMove={handlePointerMove}
      onPointerUp={handlePointerUp}
      onPointerCancel={finishPreview}
      onKeyDown={handleKeyDown}
      onDoubleClick={() => onCommit(snapPrimaryRoomRatio(
        DEFAULT_PRIMARY_ROOM_RATIO,
        availableRatios,
      ))}
    >
      <span aria-hidden="true" />
    </div>
  );
}
