# Playback Freeze Recovery

Date: 2026-10-06. Application version remains 0.2.16 in both editions.

## Confirmed Recovery Gaps

Remote EOF previously ended playback without notifying RoomSession of a
failure. A remote stream could also stop updating without an error event;
the player had no progress monitor and the session had no automatic source
refresh. These paths are reproduced by regression tests. The user's exact
long-running freeze has not been reproduced, so a CDN, network or decoder
cause remains unconfirmed.

## Behavior

- Sample remote media time asynchronously every two seconds, with at most one
  outstanding progress query per player. Ignore replies from replaced sources.
- Count newly presented video frames on the render thread. Redraws and repeated
  frames do not count as new video progress.
- Report a fixed playback failure when the media clock or rendered video stops
  progressing for 30 seconds, or when a remote stream ends with EOF/error.
- Set libmpv's HTTP network timeout to 30 seconds.
- Re-resolve the room's source using its current effective quality and rate.
  Preserve the attached player, audio focus and volume.
- Retry after 3, 6, 12, 24 and 48 seconds, allowing at most five consecutive
  attempts. Reset the budget after observed media/video progress or a manual
  source check. A successful source response alone does not reset the budget.
- Exclude local media, intentional stop, user pause and suspended rendering.
  Cancel scheduled recovery on stop, cancel, release, player detachment or
  confirmed offline status.

Logs contain fixed messages, room IDs and attempt counts. They do not contain
playback addresses, query parameters or service diagnostics. Settings and
workspace schemas are unchanged.

## Verification Scope

Regression tests cover unexpected and natural EOF, deadline expiry, continued
video rendering, video stall while the media clock advances, stale replies,
pause/suspension, intentional stop, retry bounds, retained quality, failed
re-resolution and stop/offline cancellation. Local media fixtures exercise
libmpv rendering and its natural end-of-file events.

The release verification attachment records fresh full-build, CTest and package
results. Long-running real-network playback still requires target-environment
acceptance; deterministic recovery tests do not establish its root cause.
