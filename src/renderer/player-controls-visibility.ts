export const PLAYER_CONTROLS_HIDE_DELAY_MS = 3_000;

export function scheduleControlsHide({
  locked,
  onHide,
}: {
  locked: boolean;
  onHide: () => void;
}): () => void {
  if (locked) return () => {};
  const timer = globalThis.setTimeout(onHide, PLAYER_CONTROLS_HIDE_DELAY_MS);
  return () => globalThis.clearTimeout(timer);
}
