import { renderToStaticMarkup } from 'react-dom/server';
import { describe, expect, it, vi } from 'vitest';
import {
  PrimaryRoomDivider,
  clampPrimaryRoomPreview,
  snapPrimaryRoomRatio,
  stepPrimaryRoomRatio,
} from '../src/renderer/components/PrimaryRoomDivider';

describe('PrimaryRoomDivider', () => {
  it('clamps free drag preview to the hard range', () => {
    expect(clampPrimaryRoomPreview(0.2)).toBe(0.42);
    expect(clampPrimaryRoomPreview(0.64)).toBe(0.64);
    expect(clampPrimaryRoomPreview(0.9)).toBe(0.7);
  });

  it('snaps to the nearest available recommendation', () => {
    expect(snapPrimaryRoomRatio(0.64, [0.5, 0.6, 0.67])).toBe(0.67);
    expect(snapPrimaryRoomRatio(0.64, [0.5, 0.6])).toBe(0.6);
    expect(snapPrimaryRoomRatio(0.64, [])).toBe(0.6);
  });

  it('steps through available ratios for keyboard input', () => {
    expect(stepPrimaryRoomRatio(0.6, [0.5, 0.6, 0.67], -1)).toBe(0.5);
    expect(stepPrimaryRoomRatio(0.6, [0.5, 0.6, 0.67], 1)).toBe(0.67);
    expect(stepPrimaryRoomRatio(0.67, [0.5, 0.6, 0.67], 1)).toBe(0.67);
  });

  it('renders an accessible horizontal separator', () => {
    const html = renderToStaticMarkup(
      <PrimaryRoomDivider
        orientation="horizontal"
        value={0.6}
        availableRatios={[0.5, 0.6, 0.67]}
        onPreviewChange={vi.fn()}
        onCommit={vi.fn()}
        onDragStateChange={vi.fn()}
      />,
    );

    expect(html).toContain('role="separator"');
    expect(html).toContain('aria-orientation="vertical"');
    expect(html).toContain('aria-valuemin="42"');
    expect(html).toContain('aria-valuemax="70"');
    expect(html).toContain('aria-valuenow="60"');
  });
});
