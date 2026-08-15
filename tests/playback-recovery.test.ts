import { describe, expect, it } from 'vitest';
import {
  PLAYBACK_RECOVERY_DELAYS_MS,
  PLAYBACK_RECOVERY_MAX_ATTEMPTS,
  createPlaybackRecoveryController,
  getPlaybackRecoveryPlan,
} from '../src/renderer/playback-recovery';

function createManualTimers() {
  const timers: Array<{ callback: () => void; delayMs: number; cancelled: boolean }> = [];
  return {
    timers,
    setTimeout(callback: () => void, delayMs: number) {
      const timer = { callback, delayMs, cancelled: false };
      timers.push(timer);
      return timer;
    },
    clearTimeout(timer: unknown) {
      (timer as { cancelled: boolean }).cancelled = true;
    },
    runNext() {
      const timer = timers.find((item) => !item.cancelled);
      if (!timer) throw new Error('expected a scheduled timer');
      timer.cancelled = true;
      timer.callback();
    },
  };
}

describe('playback recovery', () => {
  it('uses exponential delays for each automatic recovery attempt', () => {
    expect(PLAYBACK_RECOVERY_DELAYS_MS).toEqual([1_000, 2_000, 4_000, 8_000]);
    expect(getPlaybackRecoveryPlan(0)).toEqual({ attempt: 1, delayMs: 1_000 });
    expect(getPlaybackRecoveryPlan(1)).toEqual({ attempt: 2, delayMs: 2_000 });
    expect(getPlaybackRecoveryPlan(2)).toEqual({ attempt: 3, delayMs: 4_000 });
    expect(getPlaybackRecoveryPlan(3)).toEqual({ attempt: 4, delayMs: 8_000 });
  });

  it('stops scheduling after the maximum number of automatic attempts', () => {
    expect(PLAYBACK_RECOVERY_MAX_ATTEMPTS).toBe(4);
    expect(getPlaybackRecoveryPlan(4)).toEqual({
      attempt: 4,
      exhausted: true,
    });
    expect(getPlaybackRecoveryPlan(99)).toEqual({
      attempt: 4,
      exhausted: true,
    });
  });

  it('suppresses duplicate failures and retries with the full backoff sequence', () => {
    const timers = createManualTimers();
    const retries: number[] = [];
    const states: unknown[] = [];
    const controller = createPlaybackRecoveryController({
      onRetry: () => retries.push(retries.length + 1),
      onStateChange: (state) => states.push(state),
      setTimeout: timers.setTimeout,
      clearTimeout: timers.clearTimeout,
    });

    for (const expectedDelay of PLAYBACK_RECOVERY_DELAYS_MS) {
      controller.reportFailure();
      controller.reportFailure();
      expect(timers.timers.at(-1)?.delayMs).toBe(expectedDelay);
      timers.runNext();
    }

    controller.reportFailure();

    expect(retries).toEqual([1, 2, 3, 4]);
    expect(states.at(-1)).toEqual({ attempt: 4, exhausted: true });
  });

  it('cancels a pending retry and resets the attempt budget after playback resumes', () => {
    const timers = createManualTimers();
    const retries: number[] = [];
    const states: unknown[] = [];
    const controller = createPlaybackRecoveryController({
      onRetry: () => retries.push(retries.length + 1),
      onStateChange: (state) => states.push(state),
      setTimeout: timers.setTimeout,
      clearTimeout: timers.clearTimeout,
    });

    controller.reportFailure();
    controller.markPlaying();
    expect(timers.timers[0].cancelled).toBe(true);

    controller.reportFailure();
    expect(timers.timers.at(-1)?.delayMs).toBe(1_000);
    timers.runNext();

    expect(retries).toEqual([1]);
    expect(states).toContainEqual(undefined);
  });
});
