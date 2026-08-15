export const PLAYBACK_RECOVERY_DELAYS_MS = [1_000, 2_000, 4_000, 8_000] as const;
export const PLAYBACK_RECOVERY_MAX_ATTEMPTS = PLAYBACK_RECOVERY_DELAYS_MS.length;

export type PlaybackRecoveryPlan =
  | { attempt: number; delayMs: number }
  | { attempt: number; exhausted: true };

export interface PlaybackRecoveryState {
  attempt: number;
  delayMs?: number;
  exhausted?: boolean;
}

export interface PlaybackRecoveryControllerOptions {
  onRetry: () => void;
  onStateChange: (state: PlaybackRecoveryState | undefined) => void;
  setTimeout?: (callback: () => void, delayMs: number) => unknown;
  clearTimeout?: (timer: unknown) => void;
}

export interface PlaybackRecoveryController {
  reportFailure(): void;
  markPlaying(): void;
  retryNow(): void;
  dispose(): void;
}

export function getPlaybackRecoveryPlan(completedAttempts: number): PlaybackRecoveryPlan {
  const attemptIndex = Number.isFinite(completedAttempts)
    ? Math.max(0, Math.floor(completedAttempts))
    : PLAYBACK_RECOVERY_MAX_ATTEMPTS;
  const delayMs = PLAYBACK_RECOVERY_DELAYS_MS[attemptIndex];

  if (delayMs === undefined) {
    return { attempt: PLAYBACK_RECOVERY_MAX_ATTEMPTS, exhausted: true };
  }

  return { attempt: attemptIndex + 1, delayMs };
}

export function createPlaybackRecoveryController({
  onRetry,
  onStateChange,
  setTimeout: scheduleTimer = (callback, delayMs) => globalThis.setTimeout(callback, delayMs),
  clearTimeout: cancelTimer = (timer) => globalThis.clearTimeout(timer as ReturnType<typeof globalThis.setTimeout>),
}: PlaybackRecoveryControllerOptions): PlaybackRecoveryController {
  let completedAttempts = 0;
  let timer: unknown;
  let timerActive = false;
  let disposed = false;

  const cancelScheduledRetry = () => {
    if (!timerActive) return;
    cancelTimer(timer);
    timer = undefined;
    timerActive = false;
  };

  const reset = () => {
    cancelScheduledRetry();
    completedAttempts = 0;
    onStateChange(undefined);
  };

  return {
    reportFailure() {
      if (disposed || timerActive) return;

      const plan = getPlaybackRecoveryPlan(completedAttempts);
      if ('exhausted' in plan) {
        onStateChange({ attempt: plan.attempt, exhausted: true });
        return;
      }

      completedAttempts = plan.attempt;
      onStateChange({ attempt: plan.attempt, delayMs: plan.delayMs });
      timerActive = true;
      timer = scheduleTimer(() => {
        timer = undefined;
        timerActive = false;
        if (disposed) return;
        onStateChange(undefined);
        onRetry();
      }, plan.delayMs);
    },

    markPlaying() {
      if (disposed) return;
      reset();
    },

    retryNow() {
      if (disposed) return;
      reset();
      onRetry();
    },

    dispose() {
      if (disposed) return;
      disposed = true;
      cancelScheduledRetry();
    },
  };
}
