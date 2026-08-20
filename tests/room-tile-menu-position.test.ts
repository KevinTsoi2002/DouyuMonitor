import { describe, expect, it } from 'vitest';
import { resolveTileMenuPosition } from '../src/renderer/tile-menu-position';

describe('room tile menu positioning', () => {
  const viewport = { width: 1280, height: 720 };
  const menuSize = { width: 205, height: 210 };

  it('opens below the trigger when the viewport has enough room', () => {
    expect(resolveTileMenuPosition(
      { top: 80, right: 1180, bottom: 107 },
      menuSize,
      viewport,
    )).toEqual({ left: 975, top: 113, placement: 'bottom' });
  });

  it('opens above the trigger when a short tile leaves too little room below', () => {
    expect(resolveTileMenuPosition(
      { top: 650, right: 1180, bottom: 677 },
      menuSize,
      viewport,
    )).toEqual({ left: 975, top: 434, placement: 'top' });
  });

  it('keeps the menu inside the viewport near the right edge', () => {
    expect(resolveTileMenuPosition(
      { top: 80, right: 1350, bottom: 107 },
      menuSize,
      viewport,
    )).toEqual({ left: 1067, top: 113, placement: 'bottom' });
  });
});
