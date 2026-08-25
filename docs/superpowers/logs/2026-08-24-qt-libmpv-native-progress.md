# Qt + libmpv Native Progress Log

**Date:** 2026-08-24
**Worktree:** `D:/DouyuMonitor/.worktrees/codex/qt-libmpv-m0`
**Branch:** `codex/qt-libmpv-m0`

## Audit Scope

The native implementation was compared with:

- `native/README.md` and `native/CMakeLists.txt`;
- the latest Electron design and plan:
  `docs/superpowers/specs/2026-08-19-multi-stream-capacity-and-adaptive-quality-design.md`;
  `docs/superpowers/plans/2026-08-19-multi-stream-capacity-and-adaptive-quality.md`.

The Electron plan targets eight-room loopback proxy playback. Its explicit
non-goal says not to introduce mpv. The Qt + libmpv work is therefore tracked
as a separate native migration line and must not be marked as completion of the
Electron multi-stream plan.

## Completed Native Scope

- Reproducible Qt 6.8.3 MSVC 2022 x64 SDK bootstrap.
- Pinned libmpv development package, SHA-256 verification, and generated MSVC
  import library.
- CMake presets, dependency probe, and runtime DLL deployment.
- GUI-thread-owned `PlayerSurface` with OpenGL render context.
- Local media loading with generated PPM fixture and first-frame detection.
- Explicit playback states: `Idle`, `Loading`, `Playing`, `Paused`, `Ended`,
  and `Error`.
- Pause/resume through libmpv and a Qt toolbar button in the single-window
  prototype.
- `--media <path>` and `--self-test`; with media supplied, self-test waits for
  the first rendered frame.

## Verification Evidence

- CMake/MSVC Debug build: passed.
- CTest: `6/6` passed (`media_source_test`, `player_surface_test`,
  `main_window_test`, `native_self_test_media`, native self-test, and
  dependency probe).
- CLI self-test through CTest: `douyu_monitor_native.exe --self-test` passed
  with the fixed `native self-test passed: render` summary.
- CLI local-media lifecycle self-test through `native_self_test_media`: exit
  code `0`, covering `load`, `first-frame`, `stop`, and `release`.
- `git diff --check`: passed; only the expected `.gitignore` line-ending
  warning was emitted.
- No build/test processes remained after verification.

No playback URL, token, cookie, request header, or sidecar raw output was
recorded.

## Remaining Gaps

- No real remote playback source has been connected to the native client.
- No native multi-room workspace, quality policy, loopback proxy, or Electron
  IPC bridge has been implemented.
- Hardware-decoder selection and software fallback are not yet measured.
- The native line has no dedicated Notion page or approved native design page.

## Notion Sync

Notion MCP is connected. Incremental pages were created and re-read for:

- M1-1 `MediaSource` contract;
- M1-2 `PlayerSurface` stop/release lifecycle;
- M1-3 `MainWindow` source validation and controls.

The local file remains an audit mirror; the Notion pages contain the measured
evidence and the next planned step.

## M1 Incremental Evidence

- M1-1: local-only `MediaSource`, URI rejection, and stable redacted
  description; full CTest `5/5` passed.
- M1-2: deterministic `stop()` and `release()` with stale-state cleanup;
  full MSVC build and CTest `5/5` passed.
- M1-3: MainWindow validates descriptors before mpv, adds the Qt standard stop
  control, and synchronizes pause state after load/stop; full MSVC build and
  CTest `5/5` passed in about 39.47 seconds.
- M1-4: native self-test now covers load/first-frame/stop/release, emits only a
  fixed sanitized summary, and documents the CLI/CTest commands; full MSVC
  build and CTest `6/6` passed.
- `git diff --check` passed with only the existing `.gitignore` line-ending
  warning, and no native test processes remained.

Notion M1-4 log: https://app.notion.com/p/3c60f4bdec4881c5ad4be4ab60abb720?pvs=204

## Prepared Next Step

M1 task 4 is complete. Before starting another implementation phase, review
the branch diff against the M1 design acceptance criteria and create an
approved native next-stage design. Keep remote-source authorization and
sensitive-data handling explicit; do not connect a live Douyu URL as part of
this review.

## M2 Incremental Evidence

- M2-3: Qt JSONL value types and codec committed as `28b7467`; focused
  protocol CTest passed `1/1`.
- M2-4: `StreamgetProcessClient` owns one lazy `QProcess`, a FIFO queue, two
  in-flight slots, per-request deadlines, cancellation, stale-response
  suppression, malformed-output failure, crash failure, one clean restart, and
  bounded shutdown. The fake child is deterministic and network-free.
- M2-4 focused CTest passed `2/2`; full native CTest passed `8/8` after the
  MSVC x64 build. Python service tests passed `19/19`.
- The protocol also decodes typed ping, shutdown, cancel acknowledgements, and
  room search results with HTTP(S) avatar validation.
- Commit: `438d13a feat: add bounded Qt StreamGet process client`.
- No playback URL, token, cookie, request header, traceback, or diagnostic
  text was emitted by the client/fake-child test harness. No matching service,
  fake-child, native app, or client test process remained after verification.

Notion M2-3 log: https://app.notion.com/p/3c60f4bdec488184a288c3c432ae8302?pvs=204

## Prepared Next Step

M2-4 is complete. Next is M2-5: pin the Python/runtime dependencies, create the
service bootstrap and PyInstaller build scripts, wire optional Python tests into
CMake, and update the Qt-only README. Keep remote-source authorization and
sensitive-data handling explicit; do not connect a live Douyu URL as part of
this packaging phase.

## M2-6 Acceptance Evidence

- Fresh MSVC x64 preset configure and build passed with exit code `0`:
  `cmake --preset windows-x64` and
  `cmake --build --preset windows-x64-debug --parallel 4` from `native`.
- Fresh venv Python service tests passed `19/19` from the repository root.
- Fresh CTest preset passed `9/9`, including
  `streamget_service_python_tests`, the Qt protocol/client tests, and all six
  M1 tests.
- Packaged `native/out/service/streamget_service.exe` smoke test returned
  ping/shutdown JSONL, exit code `0`, and empty stderr.
- Captured configure, build, Python, CTest, and smoke output was scanned for
  `playbackUrl`, query strings, Cookie, token, signature, and raw Python
  traceback; scan was clean.
- `git diff --check` reported no whitespace errors; only the known `.gitignore`
  LF/CRLF warning remained. No service, fake child, native app, or client test
  process remained.
- M2 is complete. M3 is next: single-room remote `MediaSource` lifecycle and
  first-frame integration, still without Electron/Node/React/TypeScript.

## M2-5 Evidence

- Added exact runtime pins: `streamget==4.0.10` and `pyinstaller==6.22.0`.
- `bootstrap-streamget-service.ps1` created `native/.venv` and installed both
  requirement files successfully under Python 3.14.3.
- `build-streamget-service.ps1` produced
  `native/out/service/streamget_service.exe`; PyInstaller spec files stay under
  the ignored build output directory.
- Packaged service smoke test sent only `ping` and `shutdown`: exit code `0`,
  fixed JSONL responses, and empty stderr.
- CMake prefers `native/.venv` when present and registers
  `streamget_service_python_tests` with the repository root on `PYTHONPATH`.
- Full CTest after M2-5 configuration passed `9/9`; direct venv Python tests
  passed `19/19`.
- README states Qt + libmpv is the sole maintained runtime, one service child is
  used per Qt application, URLs remain memory-only, and no Node/Electron runtime
  is required.
- Commit: `5b83f87 build: package Qt StreamGet service`.

Notion M2-4 log: https://app.notion.com/p/3c60f4bdec4881e6bfb4f66e73564549?pvs=204

## Prepared Next Step

M2-5 is complete. M2-6 is the final acceptance pass: rerun the configured MSVC
build and all CTest entries, scan captured output for sensitive URL or
credential material, verify the package path, and confirm no child processes
remain before marking M2 complete.

## M3 GUI Lifecycle Fix And Acceptance Evidence

**Date:** 2026-08-25

- Reproduced the remaining `main_window_test` crash on the original GUI path.
- Root cause: `QToolButton` instances were constructed with `MainWindow` as
  parent and then inserted into `QToolBar`; teardown could delete the same
  widget through two ownership paths. The buttons now use the toolbar as their
  parent before `addWidget`.
- Kept the libmpv teardown hardening in `PlayerSurface`: stop the event timer,
  clear the update callback, remove queued callbacks, then free the render
  context and terminate mpv.
- Rebuilt the full MSVC x64 Debug target successfully through the Visual Studio
  x64 developer environment.
- Full CTest passed `10/10`, including the Python service suite (`19/19`),
  `main_window_test`, `native_self_test_media`, the CLI self-test, and the
  libmpv dependency probe. Total CTest time was `51.55s`.
- Focused `streamget_process_client_test` rerun passed after one earlier
  transient failure; no service, fake child, native app, or test process
  remained after verification.
- `git diff --check` reported no whitespace errors; only the existing
  `.gitignore` LF/CRLF warning remained.
- Sensitive-output scan found only fixed fake/test literals and documentation;
  no live playback URL, token, cookie, request header, traceback, or raw mpv
  diagnostics were emitted by the implementation or test logs.

Notion M3 log: https://app.notion.com/p/3c70f4bdec4881b387c8e8bd767c7a79?pvs=204

## Prepared Next Step

M3 single-room remote playback integration is implemented and verified. The
next stage is a design/acceptance review for multi-room composition and quality
policy, without connecting real Douyu traffic or introducing Electron.

## M4 Qt-only Multi-room Evidence

**Date:** 2026-08-25

- Implemented the approved Qt + libmpv multi-room workspace with one shared
  StreamGet process client, independent room sessions, adaptive quality policy,
  and a deterministic 1/2/3-column grid supporting up to nine rooms.
- The 1–4 room range preserves each user's requested quality. At 5–9 rooms,
  the primary room uses `Original` and other rooms use `Standard`; dropping
  back to four rooms restores user quality.
- MainWindow GUI coverage now verifies 1, 4, and 9 rooms, rejects the tenth,
  preserves remaining surfaces after removal, and keeps local media controls.
- Fresh MSVC x64 preset configure completed with exit code `0`.
- Fresh MSVC x64 Debug build completed with exit code `0` after adding the
  workspace sources to the formal application target.
- Full CTest passed `13/13`, including the new quality policy, room session,
  coordinator, and MainWindow tests; Python service tests passed `19/19`.
- Captured configure, build, CTest, and Python output was scanned for
  `playbackUrl`, `wsAuth`, `Cookie`, `token`, `signature`, `Traceback`, raw
  exceptions, and raw mpv diagnostics; no matches were found.
- `git diff --check` reported no whitespace errors; only the existing
  `.gitignore` LF/CRLF warning remained. No service, fake child, native app,
  or test process remained after verification.
- Task 4 commit: `50448a2 feat: integrate the Qt multi-room grid`.
- M4 design commit: `aa4df27`; M4 implementation plan commit: `2684e6c`.

Notion M4 log: https://app.notion.com/p/3c70f4bdec488103bfaafc5fa3d99ef7?pvs=204
Created and reread after the Notion OAuth reconnect; its scope, commits,
verification counts, and offline-test limitation match this local evidence.

## Prepared Next Step

M4 implementation and acceptance checks are complete. The Qt-only branch is
ready for review or integration; live Douyu traffic and a real nine-room smoke
test remain intentionally out of scope for this offline verification.

## M5 Design And Plan Evidence

**Date:** 2026-08-25

- The approved M5 scope adds a Qt-native right-side room-management dock for
  numeric room ID entry, removal, primary-room selection, requested quality,
  effective quality, and basic fixed lifecycle status.
- M4 remains the source of the maximum nine-room limit and the quality policy:
  one through four rooms use the user's request; five through nine use
  `Original` for the primary room and `Standard` for the others. Returning to
  four rooms restores each saved request.
- The confirmed design is
  `docs/superpowers/specs/2026-08-25-qt-only-m5-room-management-design.md`,
  committed as `3fec208`.
- The executable M5 plan is
  `docs/superpowers/plans/2026-08-25-qt-only-m5-room-management.md`, committed
  as `e2cf85a`.
- The plan maps the design to three implementation slices: coordinator snapshots
  and fixed command results, the isolated Qt management dock, and MainWindow
  integration. It requires failing tests before each implementation slice,
  then full native/Python verification and a sensitive-output scan.
- Design/plan comparison: all confirmed UI, data-flow, quality-policy,
  validation, safety, testing, and Notion logging requirements have a concrete
  plan task. No production source file has changed during this planning phase.
- The next gate is execution approval. M5 still excludes search, room metadata,
  persistence, playback retry, live Douyu traffic, credentials, Electron, and
  browser runtimes.

## M5 Task 1 Coordinator Snapshots And Command Results

**Date:** 2026-08-25

- Commit: `cf4cc1e feat: expose room workspace snapshots`.
- Added UI-safe `RoomSnapshot` and `RoomSnapshots` types plus stable
  `RoomCommandResult` values. `MultiRoomCoordinator` now exposes ordered
  snapshots and detailed add, remove, primary, and requested-quality commands;
  its existing Boolean APIs remain compatibility wrappers.
- `RoomSession::setRequestedQuality()` retains the user's request without
  resolving or changing the effective quality. The coordinator reuses the M4
  policy: one through four rooms use saved requests; at five through nine the
  primary is `Original` and other rooms are `Standard`; returning to four
  restores each saved request.
- Snapshot publication covers successful room, primary, quality, and session
  state changes. A nine-room teardown regression test exposed stale session
  access during destruction. The coordinator now disconnects each session before
  release, preventing a release-triggered state signal from traversing deleted
  session pointers.
- Fresh focused CTest passed `2/2` on this worktree:
  `room_session_test` and `multi_room_coordinator_test` (7.19 seconds).
- Design and plan comparison: Task 1 matches the approved M5 contracts,
  quality policy, offline test scope, and compatibility requirement. The
  teardown regression adds coverage without expanding product scope.
- Offline limitation: no live Douyu traffic, credentials, cookies, playback
  URLs, tokens, signatures, raw service output, tracebacks, or raw mpv
  diagnostics were used or recorded.

Notion M5 Task 1 log: pending creation and reread in this execution step.

## Prepared Next Step

Create and verify the M5 Task 1 Notion page, then implement the isolated
`RoomManagementDock` through the approved test-first Task 2 steps.
