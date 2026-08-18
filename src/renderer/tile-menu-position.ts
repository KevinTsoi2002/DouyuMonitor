interface RectLike {
  top: number;
  right: number;
  bottom: number;
}

interface Size {
  width: number;
  height: number;
}

export interface TileMenuPosition {
  left: number;
  top: number;
  placement: 'top' | 'bottom';
}

export function resolveTileMenuPosition(
  triggerRect: RectLike,
  menuSize: Size,
  viewport: Size,
): TileMenuPosition {
  const gap = 6;
  const viewportMargin = 8;
  const spaceBelow = viewport.height - triggerRect.bottom - gap - viewportMargin;
  const spaceAbove = triggerRect.top - gap - viewportMargin;
  const placement = spaceBelow < menuSize.height && spaceAbove > spaceBelow ? 'top' : 'bottom';
  const preferredTop = placement === 'top'
    ? triggerRect.top - gap - menuSize.height
    : triggerRect.bottom + gap;
  const maxLeft = Math.max(viewportMargin, viewport.width - menuSize.width - viewportMargin);
  const maxTop = Math.max(viewportMargin, viewport.height - menuSize.height - viewportMargin);

  return {
    left: Math.min(Math.max(triggerRect.right - menuSize.width, viewportMargin), maxLeft),
    top: Math.min(Math.max(preferredTop, viewportMargin), maxTop),
    placement,
  };
}
