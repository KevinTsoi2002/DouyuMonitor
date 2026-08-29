# Qt + libmpv Native M1 Implementation Plan

> Execute only after the native M1 design is approved and the Notion progress
> page has been created or the sync blocker is explicitly accepted.

**Goal:** Add a validated native media-source boundary with deterministic load,
stop, release, and sanitized lifecycle behavior.

## Task 1: Define the source contract

Files:

- Create `native/src/media/media_source.h/.cpp`.
- Create `native/tests/media_source_test.cpp`.

Steps:

1. Write failing tests for accepted local paths, rejected empty paths,
   rejected unsupported schemes, and stable source descriptions that never
   contain query strings or credentials.
2. Implement a small value type for local sources only; leave a clearly named
   extension point for a future authorized remote source.
3. Run the focused test and then CTest.

## Task 2: Add stop and release behavior

Files:

- Modify `native/src/media/player_surface.h/.cpp`.
- Modify `native/tests/player_surface_test.cpp`.

Steps:

1. Write failing tests for `stop()` after first frame, repeated `stop()`, and
   `release()` followed by a fresh load.
2. Implement the smallest mpv command path and reset stale media, error, and
   first-frame state.
3. Verify that the render context remains owned by the GUI thread and the
   event timer is not duplicated.

## Task 3: Wire the source boundary into MainWindow

Files:

- Modify `native/src/app/main_window.h/.cpp`.
- Modify `native/tests/main_window_test.cpp`.

Steps:

1. Add failing tests for invalid-source rejection, stop-button behavior, and
   pause-button state synchronization.
2. Add a compact stop control using a Qt standard media icon and tooltip.
3. Keep the central `PlayerSurface` and current toolbar behavior unchanged.

## Task 4: Extend native self-test and documentation

Files:

- Modify `native/app/main.cpp`.
- Modify `native/README.md`.

Steps:

1. Extend `--self-test --media` to cover load, first frame, stop, and release.
2. Keep all output free of media paths, URLs, tokens, cookies, or raw mpv
   diagnostics.
3. Run the complete MSVC build, CTest, CLI self-test, `git diff --check`, and
   process cleanup check.
4. Append the measured evidence to the Notion progress page and re-read it.
