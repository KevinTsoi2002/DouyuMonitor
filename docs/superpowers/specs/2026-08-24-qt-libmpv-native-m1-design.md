# Qt + libmpv Native M1 Design

## Context

The repository's current 2026-08-19 design is for the Electron eight-room
loopback proxy path and explicitly excludes mpv. The Qt + libmpv client is a
separate migration track. M0 now proves reproducible SDK setup, a single
OpenGL-backed surface, local first-frame rendering, lifecycle state, and
pause/resume controls.

## Goal

Define a stable native media-source and lifecycle boundary before connecting
any remote service. The boundary must keep source validation, player state,
cleanup, and UI controls testable without exposing URLs or credentials.

## Scope

- Add a `MediaSource` value describing an approved local file source and a
  future authorized stream source without accepting arbitrary renderer URLs.
- Add explicit `load`, `stop`, and `release` behavior to `PlayerSurface`.
- Preserve the existing states: `Idle`, `Loading`, `Playing`, `Paused`,
  `Ended`, and `Error`.
- Sanitize player errors before they reach UI or logs.
- Keep the test source local and generated at runtime.
- Keep the single-window toolbar behavior stable.

## Non-goals

- No Douyu URL discovery, signature generation, cookie reuse, or remote URL
  logging.
- No Electron IPC changes.
- No multi-room layout or adaptive quality policy.
- No claim about hardware-decoder performance or software fallback.

## Acceptance Criteria

- A valid local source loads and reaches `Playing` or `Ended` after a rendered
  first frame.
- `stop` returns the player to `Ended` or `Idle` without leaving a render or
  event timer alive.
- Releasing/reloading a source clears stale first-frame and error state.
- Invalid source descriptors fail before issuing an mpv command.
- Repeated load/stop/release calls are deterministic and idempotent.
- CTest remains green and the native self-test still exits successfully.

## Review Gate

Before implementation, compare this native M1 contract with the latest Notion
development design. If the product decision is to continue only the Electron
line, pause native implementation and record that decision instead.
