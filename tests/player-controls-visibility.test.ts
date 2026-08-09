import { afterEach, describe, expect, it, vi } from 'vitest';
import {
  PLAYER_CONTROLS_HIDE_DELAY_MS,
  scheduleControlsHide,
} from '../src/renderer/player-controls-visibility';

afterEach(() => vi.useRealTimers());

describe('player controls visibility', () => {
  it('hides controls after three seconds', () => {
    vi.useFakeTimers();
    const onHide = vi.fn();
    scheduleControlsHide({ locked: false, onHide });

    vi.advanceTimersByTime(PLAYER_CONTROLS_HIDE_DELAY_MS - 1);
    expect(onHide).not.toHaveBeenCalled();
    vi.advanceTimersByTime(1);
    expect(onHide).toHaveBeenCalledOnce();
  });

  it('does not schedule hiding while interaction is locked', () => {
    vi.useFakeTimers();
    const onHide = vi.fn();
    scheduleControlsHide({ locked: true, onHide });

    vi.runAllTimers();

    expect(onHide).not.toHaveBeenCalled();
  });
});
