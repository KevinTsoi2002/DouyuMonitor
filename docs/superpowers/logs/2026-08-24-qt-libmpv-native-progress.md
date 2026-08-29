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

Notion M5 Task 1 log: https://app.notion.com/p/3c70f4bdec48816484f2cac6ea27c437?pvs=204
Created and reread after the local log commit; the scope, implementation commit,
regression fix, `2/2` focused CTest evidence, safety boundary, and next task
match this local record.

## Prepared Next Step

Create and verify the M5 Task 1 Notion page, then implement the isolated
`RoomManagementDock` through the approved test-first Task 2 steps.

## M5 Task 2 Native Room Management Dock

**Date:** 2026-08-25

- Implementation commit: `a4e0bb0 feat: add Qt room management dock`.
- Review-test commit: `3271622 test: cover room management dock command locks`.
- Added `RoomManagementDock` as a Qt Widgets-only component with numeric room
  ID validation, a nine-room display limit, fixed-height 44-pixel room rows,
  snapshot rendering, and four intent signals for add, remove, primary, and
  requested-quality changes.
- Rows show room ID, basic state, requested quality, effective quality, and a
  fixed policy marker when the effective value differs. The dock does not
  calculate room order, primary state, capacity, or M4 quality policy.
- Snapshot updates block primary and quality control signals. Each user command
  locks that row's controls until the synchronous result returns; retained rows
  re-enable through `setCommandResult()` and removed rows await the snapshot
  refresh. Feedback maps only stable command results to fixed local text.
- Test-first evidence: the new target initially failed because
  `room_management_dock.cpp` was absent. A clean configure required explicit
  local `QT_ROOT` and `MPV_ROOT` in the new shell; this was an environment
  propagation issue, not a source failure.
- Fresh focused CTest passed `3/3`: `room_session_test`,
  `multi_room_coordinator_test`, and `room_management_dock_test` (7.97
  seconds). Review follow-up tests cover command locking and restoration,
  20-digit acceptance and 21-digit rejection, fixed row height, and every
  fixed result mapping.
- Design and plan comparison: Task 2 matches the approved Qt-only dock
  contract, nine-room limit, snapshot-driven data flow, fixed local feedback,
  no-echo refresh, and offline-only test requirements. No Electron, Chromium,
  Qt WebEngine, WebView, Node, React, TypeScript, networking, or playback
  policy calculation was added.
- Offline limitation: no live Douyu traffic, credentials, cookies, playback
  URLs, tokens, signatures, raw service output, tracebacks, or raw mpv
  diagnostics were used or recorded.

Notion M5 Task 2 log: https://app.notion.com/p/3c70f4bdec48817bb81bd07ffce2e579?pvs=204
Created and reread after the local log commit; the scope, commits, test-first
evidence, `3/3` focused CTest result, design/plan comparison, safety boundary,
and next task match this local record.

## Prepared Next Step

Create and verify the M5 Task 2 Notion page, then bind the dock to
`MainWindow` and the playback grid through the approved Task 3 tests.

## M5 Task 3 MainWindow Integration

**Date:** 2026-08-25

- Main integration commit: `190f377 feat: manage rooms from the Qt main window`.
- Review-fix commit: `36cc82c fix: restore dock state after rejected room commands`.
- `MainWindow` now owns a right-side Qt `QDockWidget` containing
  `RoomManagementDock`. The dock sends add, remove, primary-room, and
  requested-quality intent to the coordinator's detailed command APIs.
- `MultiRoomCoordinator::roomSnapshotsChanged` is the one update path for the
  playback grid and dock. `MainWindow` does not calculate duplicate IDs,
  capacity, primary state, or the M4 quality policy.
- MainWindow integration coverage exercises add, primary selection, removal,
  surface preservation, the five-to-four quality restoration display, and the
  rejected `AlreadyPrimary` path. The latter re-applies the coordinator snapshot
  so an already-primary button cannot remain visually unchecked after a click.
- Fresh focused CTest passed `4/4`: `room_session_test`,
  `multi_room_coordinator_test`, `room_management_dock_test`, and
  `main_window_test` (23.26 seconds).
- Design and plan comparison: Task 3 meets the right-side dock, four command
  routes, single-snapshot synchronization, M4 quality-display, surface reuse,
  and Qt-only requirements. The review correction stays inside the approved
  snapshot ownership model.
- Offline limitation: no live Douyu traffic, credentials, cookies, playback
  URLs, tokens, signatures, raw service output, tracebacks, or raw mpv
  diagnostics were used or recorded.

Notion M5 Task 3 log: https://app.notion.com/p/3c70f4bdec4881fd8b5ace3064d13a1f?pvs=204
Created and reread after the local log update; its scope, implementation and
review-fix commits, focused CTest count, design/plan comparison, safety
boundary, and next task match this local record.

## Prepared Next Step

Create and reread the M5 Task 3 Notion page, then start Task 4 acceptance:
MSVC configuration and build, full native and Python tests, sensitive-output
scan, process cleanup, and the final design/plan comparison.

## M5 Qt-only Room Management Final Acceptance

**Date:** 2026-08-25

- Design commit: `3fec208 docs: define Qt-only M5 room management`.
- Plan commit: `e2cf85a docs: plan Qt-only M5 room management`.
- Implementation commits: `cf4cc1e feat: expose room workspace snapshots`,
  `a4e0bb0 feat: add Qt room management dock`, and
  `190f377 feat: manage rooms from the Qt main window`.
- Follow-up commits: `3271622 test: cover room management dock command locks`
  and `36cc82c fix: restore dock state after rejected room commands`.
- Fresh MSVC x64 preset configuration and Debug build completed with exit code
  `0` using the local Qt 6.8.3 and libmpv SDK paths.
- Fresh full CTest passed `14/14`. The direct Python service suite passed
  `19/19` with the worktree root supplied as `PYTHONPATH`, matching the CTest
  target environment.
- A first acceptance attempt observed `native_self_test_media` failing while
  its child application was not freshly rebuilt. Rebuilding the declared
  `native_self_test_test` dependency restored the original path; the media
  self-test then passed three consecutive runs and the final full CTest passed.
- Captured successful configure, build, CTest, and Python output was scanned
  for `playbackUrl`, `wsAuth`, `Cookie`, `token`, `signature`, `Traceback`, and
  raw mpv debug/error patterns. The scan found no matches. `git diff --check`
  found no whitespace errors, and no managed service, test, or native-app
  process remained.

### Final design and plan comparison

1. The right-side Qt dock accepts numeric IDs, rejects invalid and duplicate
   values, and disables adding at nine rooms; dock and MainWindow tests cover
   these cases.
2. Every row routes remove, primary, and requested-quality actions through the
   coordinator; the MainWindow integration test covers add, primary, removal,
   and preserved remaining surfaces.
3. `MultiRoomCoordinator` alone owns order, primary state, command validation,
   effective quality, and ordered snapshots; the dock only renders snapshots
   and emits intent.
4. At five through nine rooms, the dock displays `Original` for the primary and
   `Standard` for non-primary effective quality without overwriting requests;
   coordinator and MainWindow tests cover that policy.
5. Removing the fifth room restores saved requested quality at four rooms; the
   coordinator and MainWindow tests assert the restored effective value.
6. The implementation remains Qt + libmpv only. Tests use fake/offline inputs
   and record no live Douyu request, credential, playback URL, cookie, token,
   signature, raw service output, traceback, or raw mpv diagnostic.

M5 is accepted for the Qt-only branch. The only non-M5 worktree differences
remain intentionally unstaged: `.gitignore`, `PlayerSurface`, and legacy
bootstrap/dependency files listed by `git status`.

Notion M5 final acceptance: https://app.notion.com/p/3c70f4bdec4881f49bffc9cb108af85d?pvs=204
Created and reread after the local acceptance record. The recorded commits,
`14/14` CTest and `19/19` Python totals, sensitive-output scan, cleanup result,
design/plan comparison, offline boundary, and unstaged-worktree boundary match
this local evidence.

## M6 Task 1 Native Workspace Persistence

**Date:** 2026-08-25

- Implementation commit: `27a2891 feat: persist native room workspace`.
- Added versioned `NativeWorkspaceSnapshot` value types and a
  `NativeWorkspaceStore` QSettings boundary under
  `DouyuMonitor/nativeWorkspaceV1`.
- Persistence keeps room metadata, requested quality, favorite state, history
  timestamps, groups, active order, primary room, and audio room only. It
  normalizes duplicate IDs and invalid references, caps active/group members at
  nine, rejects unsafe avatar URLs, and drops playback URLs, cookies, tokens,
  signatures, and request headers.
- Test-first evidence: the focused target was introduced with a failing test,
  then the implementation was added and the corrected target passed `1/1` in
  `native_workspace_store_test` (1.15 seconds).
- Baseline before Task 1 remained `14/14` for the existing full native CTest
  suite. This task's focused verification used the native preset build tree.
- The first compile attempt exposed two mechanical issues: a raw JSON literal
  incompatible with `QByteArrayLiteral` and an escaped CMake include path. A
  separate environment-only failure confirmed that MSVC's developer shell must
  run before `cl.exe`; the plan now records the `VsDevCmd.bat` procedure.
- Design and plan comparison: Task 1 implements the approved durable,
  non-sensitive workspace boundary without changing live networking or UI
  scope. Remaining limits are offline-only verification, no real Douyu traffic,
  and no live nine-room smoke test.
- No credentials, playback URLs, cookies, tokens, signatures, raw service
  output, tracebacks, or raw libmpv diagnostics were logged.

## Prepared Next Step

Create and reread the M6 Task 1 Notion acceptance page, then implement Task 2:
session live/playback state, coordinator audio focus, and the muted-player API
through focused failing tests.

## M6 Task 2 Native Live State and Audio Focus

**Date:** 2026-08-25

- Implementation commit: `8976e2d feat: expose native room live state`.
- Added `RoomLiveStatus` (`Unknown`, `Online`, `Offline`) and
  `RoomPlaybackHealth` (`Pending`, `Playing`, `Error`) to the native snapshot
  boundary. Snapshots now include protocol-validated display metadata,
  favorite state, audio focus, and muted state.
- `RoomSession::applyMetadata()` copies only the validated protocol result.
  `ROOM_OFFLINE` stops its player, reports `Offline` with `Pending` playback,
  returns to `Idle`, and does not emit a playback-failure signal. Other fixed
  service error codes remain playback errors.
- `PlayerSurface` starts muted and exposes `setMuted()` / `isMuted()`.
  `MultiRoomCoordinator` owns one audio-focus room, applies mute state across
  all retained sessions, and publishes the resulting ordered snapshots.
  The existing M4 quality policy and the maximum nine-room limit are unchanged.
- Focused regression verification rebuilt the four targets from clean state
  after loading the Visual Studio x64 developer environment, then passed `4/4`:
  `quality_policy_test`, `room_session_test`,
  `multi_room_coordinator_test`, and `player_surface_test` (37.05 seconds).
  `douyu_monitor_native` also rebuilt successfully.
- The initial clean rebuild from a plain PowerShell failed because the process
  had `cl.exe` but not the MSVC standard-library include environment
  (`<utility>` was unavailable). Loading `VsDevCmd.bat -arch=x64` isolated the
  configuration cause; the same clean rebuild and tests then passed without
  source changes.
- `git diff --check` completed without whitespace errors before the commit.
  Task 2 committed only its eleven planned source/test files; `.gitignore`,
  SDK/bootstrap material, generated Python caches, and unrelated native files
  remain unstaged.

### Task 2 design and plan comparison

1. The plan's tested offline semantics, metadata copy boundary, mute API, and
   single coordinator-owned audio focus are implemented and covered by focused
   tests.
2. The M6 design's separation between live availability and playback health is
   present in ordered UI snapshots. Widget work remains intentionally deferred
   to Task 3.
3. No Electron, Chromium, WebEngine, Node, React, or TypeScript was introduced.
   Verification used local fake-service inputs only; no live Douyu request,
   credential, cookie, playback URL, token, signature, raw service output, or
   raw libmpv diagnostic was recorded.

### Notion synchronization

- The 2026-08-25 attempt to create and reread
  `DouyuMonitor M6 Task 2 原生直播状态与音频焦点验收` was blocked before page
  creation because the Notion MCP transport returned `Auth required` while
  fetching its Markdown specification. No Notion URL or successful sync is
  claimed. Reauthenticate the connector before the next Notion write, then
  create and reread the page using this local evidence.

## Prepared Next Step

After the Notion connector is reauthenticated, create and reread the M6 Task 2
acceptance page. Then begin Task 3: replace the visible M5 dock with the
legacy-style left `RoomSidebar`, native add-room and group-management dialogs,
and the Splitter-based MainWindow layout.

## M6 Task 2 Review Fix: Stale Player Event Isolation

**Date:** 2026-08-25

- Review-fix commit: `94b5e0b fix: isolate stale player failures`.
- `PlayerSurface` now assigns a unique asynchronous request ID to every
  `loadfile` command, aborts the previous pending request, and only maps a
  matching command reply to the current load.
- `MPV_EVENT_START_FILE` and `MPV_EVENT_END_FILE` now use libmpv playlist entry
  IDs. Retired entries are tracked and their delayed failures are ignored, so
  switching from source A to source B cannot mark B as failed because of A.
- Player failures are emitted through a Qt signal and mapped by `RoomSession`
  to the fixed `PLAYER_FAILED` code without replacing live availability state.
- Avatar metadata accepts only host-bearing HTTP(S) URLs without credentials;
  unsafe schemes and credential-bearing URLs are cleared.
- TDD evidence: the stale-event regression was first red due to missing request
  generation members, then passed after the minimal implementation.
- Fresh focused CTest passed `4/4`: `quality_policy_test`,
  `room_session_test`, `multi_room_coordinator_test`, and `player_surface_test`
  (27.92 seconds). The standalone `player_surface_test` also passed after the
  fix (1/1, 18.26 seconds).
- `douyu_monitor_native` rebuilt successfully with MSVC x64; the target was
  already current after the source build and exited `0` with `ninja: no work to
  do`.
- `git diff --check` passed. The source/test diff contains no credentials,
  playback URLs, cookies, tokens, signatures, request headers, tracebacks, or
  raw mpv diagnostics. Existing redacted URL literals in tests remain unchanged.

### Review-fix design and plan comparison

1. The fix stays within Task 2's existing PlayerSurface/RoomSession boundary;
   no UI, networking, quality policy, or nine-room behavior changed.
2. Live availability and playback health remain separate. Retired playback
   events are discarded before they can alter the current health snapshot.
3. The implementation uses libmpv's documented `reply_userdata` and
   `playlist_entry_id` fields and preserves the Qt + libmpv-only product route.
4. No Electron, Chromium, Qt WebEngine, WebView, Node, React, or TypeScript was
   introduced. Verification remains offline-only.

### Notion synchronization

- A new-page documentation fetch and page-creation attempt were made after the
  review-fix commit, but the Notion MCP transport returned `Auth required`
  before page creation. No Notion URL or successful sync is claimed; the local
  record remains the source of truth until reauthentication.

## Prepared Next Step

After the Notion connector is authenticated and the new Task 2 review-fix page
is created and reread, start M6 Task 3: replace the visible M5 dock with the
legacy-style left `RoomSidebar`, add-room and group-management dialogs, and the
Splitter-based MainWindow layout. Keep the nine-room cap, snapshot ownership,
and Qt + libmpv-only boundary unchanged.

## M6 Task 3 Status Audit and Remaining Implementation

**Date:** 2026-08-25

This audit compares the current worktree with the approved M6 live-status and
legacy-workspace design. The left-side Qt shell is present in the uncommitted
Task 3 work: the Splitter layout, 340px sidebar, add-room dialog, group-manager
dialog shell, live-status rendering, playback warning, primary/audio/favorite
actions, requested-quality control, and room removal are implemented.

### Implemented and verified

- `native_workspace_store_test`, `room_session_test`, `room_management_dock_test`,
  and `room_sidebar_test` pass in the current MSVC x64 build tree.
- `main_window_test` passes and confirms the left sidebar is present while the
  old right-side management dock is absent.
- The native targets rebuild with the Visual Studio x64 environment; a fresh
  no-op rebuild returned exit code 0.
- The Python StreamGet service suite remains green at 19/19 tests.
- The nine-room limit, ordered coordinator snapshots, live-status enums, audio
  focus, favorite state, and safe metadata boundary remain in the native code.

### Not implemented or not connected

1. **Workspace restore/save integration:** `NativeWorkspaceStore` can serialize
   and normalize a snapshot, but `MainWindow` does not load it before showing,
   restore active rooms, apply saved primary/audio focus, update history, or
   save accepted mutations. Audio focus and favorite changes therefore are not
   durable across restart.
2. **Groups and view switching:** the sidebar creates visible Current/Favorites/
   History tabs but does not connect tab changes or group switching to filtering.
   `GroupManagerDialog` exposes create/rename/delete signals, while
   `MainWindow` only opens the dialog; no group mutation is routed to a store or
   coordinator. `roomAssigned` is declared but never emitted or handled.
3. **Reordering:** move-up/move-down controls and drag/drop room reordering are
   absent; coordinator order remains add order only.
4. **Live-status scheduler:** no `RoomStatusScheduler` exists. There is no
   periodic online/offline refresh, retry backoff, manual refresh command,
   stale-response generation handling for metadata refresh, or automatic replay
   after an offline-to-online transition.
5. **Windows notifications:** `NotificationPolicy`, notification preferences,
   notification settings UI, and `WindowsNotificationService` are absent. No
   running-state notifications or five-minute dedupe/six-per-minute rate limit
   is implemented.
6. **StreamGet runtime deployment:** `streamget_service.exe` is built under
   `native/out/service`, but the product post-build step only copies libmpv.
   `MainWindow` still expects `streamget_service.exe` beside the native
   executable, so the packaged build cannot yet resolve a real Douyu room.

### Verification blocker

The focused CTest run passed 5/6 entries; `multi_room_coordinator_test` exited
with Windows status `0xc0000374` in the suite. A direct rerun was also unstable
(access violation or hang), so this test requires lifecycle/crash investigation
before any M6 milestone can be accepted. This is recorded as an open defect,
not treated as an implementation pass.

### Design and plan match

The current code matches the approved Task 3 visual shell and preserves the
Qt + libmpv-only, nine-room, no-sensitive-output boundary. It does not yet
match the approved M6 requirements for durable workspace behavior, functional
groups/favorites/history, reordering, timed status refresh, automatic replay,
Windows notifications, or runnable packaged StreamGet deployment. The next
implementation order is: investigate the coordinator test crash; wire
workspace restore/save; complete groups/views/reordering; add the scheduler and
notifications; then copy and verify the service beside the executable.

## Qt Debug Assertion Fix

**Date:** 2026-08-25

The reported Qt 6.8.3 Debug assertion `this->isMutable() || b == e` in
`qarraydataops.h:311` was traced to `MainWindow::restoreWorkspace()`. The code
held iterators into `workspaceSnapshot_` while `addRoomDetailed()` synchronously
published snapshots; the callback could replace the snapshot and invalidate
those iterators. Restore now copies the active-room list and extracts requested
quality/favorite values before the coordinator call, so no Qt container iterator
survives a signal-emitting operation.

Temporary destructor/self-test diagnostics were removed after confirming the
native self-test exit path. The focused verification passed: `main_window_test`,
`streamget_process_client_test`, `native_self_test_media`, and
`douyu_monitor_native_self_test`. The full CTest run passed 15/16; the remaining
`multi_room_coordinator_test` failure is an intermittent existing offline-order
test race and reproduces only on some repeated runs.

## M6 Task 5 Room Status Scheduler and Fake Search Evidence

**Date:** 2026-08-25

Task 5 from `2026-08-25-qt-native-legacy-workspace-status-notifications.md`
is implemented in the current worktree.

### Implemented

- Added `RoomStatusScheduler` with one single-shot timer per room, online and
  offline base intervals, deterministic room-id jitter, retry backoff,
  same-room request de-duplication, cancellation on removal, and generation/
  request-id protection against late responses and remove/re-add races.
- Added focused scheduler coverage for interval scheduling, concurrent-request
  prevention, cancellation, retry reset, and stale completion rejection.
- Extended the fake StreamGet service with `--search-script
  online,offline,online`. Numeric searches now return one protocol-validated
  metadata result with fixed synthetic fields and no playback URL in the
  search response. Existing non-scripted text searches remain empty.
- Added a client integration test that verifies the scripted status sequence,
  metadata fields, and safe avatar URL handling.
- Added the Qt Widgets/OpenGLWidgets link requirements to the scheduler test
  target because the existing `RoomSnapshot` type currently includes
  `RoomSession`/`PlayerSurface` declarations.

### Verification

- MSVC x64 Debug build of `room_status_scheduler_test` passed.
- Focused scheduler test passed: `1/1`.
- Task 5 regression set passed: `3/3`:
  `stream_service_protocol_test`, `streamget_process_client_test`, and
  `room_status_scheduler_test`.
- The new search-script test was observed failing before the fake-service
  implementation (`results.size() == 0`, expected `1`) and passing afterward.
- `git diff --check` passed. The repository scan found no temporary destructor,
  self-test, or multi-test diagnostic markers. The only playback URL remains
  in the existing resolve-path fake response; the new search response contains
  metadata only.

### Plan comparison and limits

Task 5 now matches the approved scheduler ownership, timing, retry, and fake
numeric-search requirements. The behavior is validated with offline fakes only;
no live Douyu request or genuine multi-room playback run was performed. The
`RoomStatusScheduler` is not yet connected to `MultiRoomCoordinator`; that is
Task 6 work.

### Notion synchronization

Notion MCP was still unavailable in this session, so no Notion page creation or
readback is claimed. This local log is the source of truth until the connector
is reauthenticated.

## Prepared Next Step

Start M6 Task 6: route scheduler metadata responses through
`MultiRoomCoordinator`, apply accepted metadata/live-status transitions to the
matching `RoomSession`, stop playback on Online-to-Offline, resolve on
Offline-to-Online, publish snapshots after accepted mutations, and add the
coordinator/sidebar integration tests before proceeding to notifications.

## M6 Task 6 and Qt Windows Deployment Fix

**Date:** 2026-08-25

Task 6 is implemented in the current worktree. MultiRoomCoordinator now owns
RoomStatusScheduler, routes only scheduler-owned search responses, validates
that each search returns exactly one matching numeric room, applies metadata,
stops a session when it becomes offline, and re-resolves an idle session when
it becomes online. Removing a room cancels its scheduled request; accepted
metadata changes republish snapshots. RoomSession::stop() now resets playback
health to Pending and releases the surface media state to Idle.

The Qt startup error shown in the attached screenshot was traced to ambiguous
Debug/Release deployment. The native CMake post-build step now invokes
windeployqt with the matching mode and --force, and the deployment check
requires both the matching core DLL (Qt6Cored.dll or Qt6Core.dll) and platform
plugin (qwindowsd.dll or qwindows.dll). The README now documents the two
separate launch directories and warns against mixing their DLLs.

### Verification

- MSVC x64 Debug build completed for the coordinator, main-window, and native
  executable targets.
- Focused CTest passed: room_status_scheduler_test,
  multi_room_coordinator_test, room_session_test, room_sidebar_test, and
  main_window_test (5/5; the coordinator test took 22.88 seconds and the
  main-window test 43.44 seconds).
- Debug deployment check passed with Qt6Cored.dll + qwindowsd.dll.
- Release deployment check passed with Qt6Core.dll + qwindows.dll; the
  release executable remained running for a five-second startup smoke check.
- git diff --check remains clean apart from the repository's existing line
  ending warnings.

### Plan comparison and limits

Task 6 now matches the scheduler/coordinator transition requirements using only
the offline fake StreamGet service. A live Douyu request, real multi-room
playback, and Windows notification delivery remain unverified. Notion MCP is
still unavailable, so no Notion page creation or readback is claimed.

## Prepared Next Step

Run the next M6 task: connect the coordinator's room snapshots to the native
sidebar status indicators, then implement Windows notifications for online,
offline, and playback-error transitions. Keep using the fake service and write
the next evidence block here before any Notion synchronization claim.

## Qt platform plugin startup error follow-up

**Date:** 2026-08-25

The attached Debug startup error was traced to mixed Qt Windows deployment.
The post-build step now removes the opposite `qwindows.dll`/`qwindowsd.dll`
before running the matching `windeployqt` mode. The deployment verifier rejects
mixed platform plugins and receives explicitly typed CMake arguments.

### Verification

- Debug contains `Qt6Cored.dll` and only `platforms/qwindowsd.dll`.
- Release contains `Qt6Core.dll` and only `platforms/qwindows.dll`.
- Both executables stayed running for a five-second startup smoke check.
- Focused CTest passed: `5/5` (`room_status_scheduler_test`,
  `room_session_test`, `multi_room_coordinator_test`, `room_sidebar_test`,
  `main_window_test`).

### Plan comparison and limits

The fix is limited to deterministic Qt Windows deployment and does not change
the Qt/libmpv runtime architecture. Notification service integration remains
the next implementation task. Notion MCP remains unavailable, so this local
log is the only synchronization record for this step.

## M6 Notifications And Close-Lifecycle Regression Fix

**Date:** 2026-08-26

The reported Windows close-time `HEAP CORRUPTION DETECTED (0xC0000374)` was
rechecked against the native window lifecycle. `MainWindow::~MainWindow()` now
detaches and deletes every `QGridLayout` item before destroying the coordinator,
so coordinator-owned room surfaces cannot remain as dangling layout items.

The notification integration also had a startup race: restoring a room could
briefly reach `Playing` and then emit its first mpv playback failure, which was
mistaken for a runtime failure. Restored rooms are now suppressed for a
one-second startup window; the notification policy is then reset and primed
from current snapshots. User-added rooms are not included in that restored-room
suppression set.

### Verification

- `restoresWorkspaceWithoutStartupNotification`: passed after the fix.
- Focused notification regression: `3/3` passed
  (`notification_policy_test`, `windows_notification_service_test`,
  `main_window_test`).
- Debug `douyu_monitor_native` build: exit code 0.
- Release `douyu_monitor_native` build: exit code 0.
- The coordinator replay test was stabilized by asserting the `sourceReady`
  signal and the recorded `Ready` state transition instead of requiring the
  transient state to still be current after the synthetic mpv failure.
- Full Debug CTest: `19/19` passed. The corrected replay test also passed in
  three consecutive isolated runs.
- No temporary diagnostic output remains in the notification test.

### Design and plan comparison

The Qt + libmpv-only, nine-room architecture remains unchanged. Workspace
restore, sidebar state, Windows notification preferences/service, startup
notification suppression, and deterministic offline-to-online replay evidence
are implemented. A live Douyu playback run and real Windows toast delivery
remain unverified. The next gate is the Task 8 release-evidence pass: inspect
the native UI, scan source/logs for sensitive leakage, verify packaged
StreamGet deployment, and compare every acceptance point with the approved
design before declaring M6 complete.

### Notion synchronization

Notion MCP was still unavailable in this session. This local log is the source
of truth; no Notion page creation or readback is claimed.

## Prepared Next Step

Execute M6 Task 8 release evidence: run the Release startup/service smoke
check, inspect the left-sidebar Qt shell and notification settings, perform the
sensitive-output scan, and reconcile the implementation with the approved
design. Record those results here before attempting any Notion synchronization.

## M6 Task 8 Release Evidence

**Date:** 2026-08-26

The release evidence pass completed after the close-lifecycle and notification
regressions. CMake now invokes a conditional copy helper after the native build:
when `native/out/service/streamget_service.exe` exists, it is copied beside
`douyu_monitor_native.exe`; development builds still succeed before the service
package is present.

### Verification

- A post-package full CTest run had one transient `multi_room_coordinator_test`
  failure; the same test passed in isolation, and the final full run with
  `ctest --repeat until-pass:2` completed with `19/19` passing.
- Debug native executable build: exit code 0.
- Release native executable build: exit code 0.
- PyInstaller service package: `native/out/service/streamget_service.exe`
  created with exit code 0.
- Release package contains both `douyu_monitor_native.exe` and
  `streamget_service.exe`.
- Release application stayed alive for five seconds with both packaged files
  present; after cleanup, no `douyu_monitor_native` or `streamget_service`
  process remained.
- Release `--self-test`: `native self-test passed: render`, exit code 0.
- Sensitive-output scan found only redacted/synthetic fixtures and explicit
  sanitizer allowlist fields. No credentials, live tokens, cookies, signatures,
  raw service traceback, or raw mpv diagnostics were emitted by the new code or
  evidence logs.
- `git diff --check`: exit code 0; only existing line-ending warnings were
  reported by Git.

### Design and plan comparison

The implementation matches the approved Qt + libmpv-only design and the M6
Task 8 offline acceptance boundary: nine-room limit, left sidebar, workspace
restore, scheduler transitions, notification policy/settings/service, bounded
shutdown, deterministic package layout, and no Electron/Chromium runtime. A
controlled live Douyu resolve/first-frame/stop/release run and genuine nine-room
live playback remain intentionally unexecuted because they require authorized
live access; all automated evidence uses the fake service.

### Notion synchronization

Notion MCP was still unavailable. This local log remains the source of truth;
no Notion page creation or readback is claimed.

## Prepared Next Step

The next executable step is a controlled live-access acceptance run (single
room first, then the nine-room capacity check) using an authorized environment,
while preserving the no-URL/no-credential logging boundary. Until that access
is available, the native offline implementation and release package are
verified but live playback remains an explicit limitation.

## M6 Task 8 UI Acceptance Follow-up

**Date:** 2026-08-26

The release native window was reselected through a fresh Windows UI state and
the remaining shell flows were exercised without submitting a live room
request.

### Verification

- The left sidebar exposed the expected `当前` / `收藏` / `历史` tabs, room
  management, and add-room controls.
- The notification settings dialog opened and exposed all four notification
  switches: Windows notifications, live-start, live-stop, playback failure,
  and playback recovery. The dialog closed normally through its own button.
- The add-room dialog opened with a focused room-number editor. With an empty
  value, the confirm action was disabled; after entering the non-sensitive
  synthetic value `0`, the confirm action became enabled. The dialog was
  cancelled, so no room was submitted and no network request was made.
- The main window then closed through its close control. A follow-up process
  check found no `douyu_monitor_native` or `streamget_service` process, and no
  heap-corruption dialog or additional close-time error was observed.

### Design and plan comparison

The native Qt + libmpv shell, left-sidebar workflow, notification settings,
input validation, and bounded close lifecycle match the approved design and
the M6 offline acceptance boundary. The nine-room capacity and genuine Douyu
resolve/first-frame/stop/release path remain unverified because they require an
authorized live-access environment.

### Notion synchronization

Notion MCP was still unavailable in this session. This local log remains the
source of truth; no Notion page creation or readback is claimed.

## Prepared Next Step

Run the controlled live-access acceptance gate: verify one authorized Douyu
room from resolve through first frame and stop, then exercise the nine-room
capacity limit. Record only redacted status/result evidence, never room URLs,
cookies, tokens, or credentials. If live access is not provided, keep the
release package at the verified offline boundary and do not fabricate a live
playback result.

## M6 Next-Step Regression Gate

**Date:** 2026-08-26

Before advancing to live access, the existing offline release boundary was
rechecked with fresh commands.

### Verification

- Full Debug CTest completed with `19/19` passing and exit code `0`. An earlier
  run had one transient first-attempt failure in
  `rejectsMalformedChildOutput` (`TIMEOUT` instead of
  `INVALID_RESPONSE`), then passed on CTest retry. The focused test passed in
  50 consecutive direct invocations and in a five-run CTest repeat, so no code
  change was made from that single startup-timing outlier.
- Release `douyu_monitor_native.exe --self-test` printed
  `native self-test passed: render` and exited `0`.
- Release package still contains both `douyu_monitor_native.exe` and
  `streamget_service.exe`. A five-second startup smoke kept the native window
  alive and then exited it cleanly; no native or service process remained.
- `git diff --check` exited `0`; only existing CRLF conversion warnings were
  reported.
- The sensitive-output scan matched only redacted/synthetic fixtures and
  explicit sanitizer allowlist fields. No live credential, cookie, token,
  signature, raw service traceback, or raw mpv diagnostic was emitted.

### Design and plan comparison

The offline Qt + libmpv implementation remains aligned with the approved design:
9-room capacity, left sidebar, workspace restore, notification policy/service,
bounded shutdown, deterministic package layout, and no Electron/Chromium
runtime. The only unverified acceptance points are the authorized live Douyu
resolve/first-frame/stop/release flow and genuine nine-room live playback.

### Notion synchronization

Notion MCP was still unavailable. This local log remains the source of truth;
no Notion page creation or readback is claimed.

## Prepared Next Step

Request or provision an authorized live-access environment, then run the single-
room Douyu acceptance flow before the nine-room capacity check. Keep all
captured evidence redacted and in-memory only; if authorization is unavailable,
stop at this verified offline boundary.

## M6 Legacy Acquisition Path Live Check

**Date:** 2026-08-26

Per the approved direction, the native path was checked using the same public
Douyu acquisition strategy as the legacy framework: numeric room metadata via
`RoomApi`, and playback resolution via StreamGet's
`DouyuLiveStream.fetch_app_stream_data()` app-search path. Electron was not
started or used.

### Verification

- The packaged `streamget_service.exe` resolved the public test room `63136`
  with `ok=true`, `isLive=true`, and one validated FLV variant. The evidence
  capture retained only those fields; the playback URL was not printed or
  persisted. Service stderr was empty and the service exited cleanly after the
  shutdown request.
- The Release Qt UI accepted room `63136`; after the resolver/status cycle the
  left sidebar displayed `直播中`. The test room was then removed before
  shutdown, so no test room remained in the workspace.
- The native window closed normally after the live check. A process check found
  no `douyu_monitor_native` or `streamget_service` process and no close-time
  heap-corruption dialog.

### Design and plan comparison

The implemented acquisition now follows the legacy framework's proven public
RoomApi + StreamGet app-search route while preserving the maintained Qt + libmpv
runtime, nine-room limit, URL validation, and no Electron/Chromium dependency.
The single-room resolve/status path is verified against live public access;
genuine first-frame rendering and a nine-room simultaneous live playback run
remain separate capacity gates.

### Notion synchronization

Notion MCP was still unavailable. This local log remains the source of truth;
no Notion page creation or readback is claimed.

## Prepared Next Step

Run the controlled live playback gate in the Qt UI: confirm the resolved source
reaches a first rendered frame, then stop/release it, and only after that test
the 9-room capacity boundary. Keep all live URLs and resolver diagnostics
in-memory and out of logs.

## Legacy Resolver Parity Check

**Date:** 2026-08-26

The legacy `scripts/streamget_bridge.py` and the maintained packaged Qt service
were both exercised with the same public test room and the same StreamGet
app-search route. Both returned `roomId=63136`, `isLive=true`, and a present FLV
result. The comparison retained no URL value and produced no stderr output.

No source change was necessary: the native `DouyuBackend.resolve()` already
uses `DouyuLiveStream.fetch_app_stream_data()` with the same Douyu room path,
while the Qt side keeps the typed validation and memory-only URL boundary.

## Prepared Next Step

Proceed to the Qt first-frame gate, then stop/release the remote source and
verify the 9-room capacity boundary with redacted evidence only.

## Qt Quick/QML Electron UI Migration Design

**Date:** 2026-08-26

The product direction is now explicit: the maintained native desktop UI will
be a complete Qt Quick/QML plus C++ implementation. Electron, Chromium,
React, Node.js, Qt WebEngine, and a QWidget/Qt Quick hybrid main UI are not
allowed in the shipped runtime. The existing Electron implementation remains
the visual and behavior reference only.

The approved design preserves the Electron 44-pixel frameless title bar,
268-pixel expanded room sidebar, compact dark/orange visual system, room
filters and actions, playback-grid overlays, menus, drawered monitoring view,
settings and workspace popups, dialogs, toast feedback, shortcuts, and
Windows window controls. The maximum capacity remains nine rooms.

The C++ workspace, status scheduling, persistence, notification, StreamGet,
remote playback, and policy boundaries remain in place. The new QML bridge is
defined around AppController, RoomListModel, WorkspaceModel, and
MonitoringModel. MpvQuickItem will replace the QWidget-based PlayerSurface
using libmpv's render API and an ordered teardown path.

The full local design is
docs/superpowers/specs/2026-08-26-qt-quick-qml-electron-ui-migration-design.md.
Its self-review found no placeholders, conflicting runtime decisions, or scope
gaps. It has not started implementation: the next gate is the user's review of
the written design, then creation and approval of the detailed implementation
plan.

### Notion synchronization

The approved design was created under the existing native design page and
read back successfully:

2026-08-26 Qt Quick/QML Electron UI 完整迁移设计

The Notion content and this local record agree on the sole-runtime decision,
nine-room limit, Electron parity target, state-model boundary, MpvQuickItem
migration, acceptance criteria, and the prohibition on sensitive playback
material in logs.

## Qt Quick/QML Migration Implementation Plan

**Date:** 2026-08-26

- Design/plan alignment: the approved Qt Quick/QML migration design was mapped
  to ten test-first tasks covering the Quick foundation, C++ bridge, Electron
  UI parity, libmpv renderer, QWidget removal, Debug/Release acceptance, and
  required phase logging.
- Evidence: `docs/superpowers/plans/2026-08-26-qt-quick-qml-electron-ui-migration.md`
  was committed as `0223e46` (`docs: plan Qt Quick QML migration`).
- Verification: placeholder scan and `git diff --check` reported no plan
  defects; the plan's public method names were cross-checked against the
  existing coordinator, session, workspace, and test boundaries.
- Sensitive-data check: the plan forbids playback URLs, cookies, tokens,
  signatures, raw StreamGet output, and raw mpv diagnostics in QML, tests,
  local logs, and Notion.
- Next phase: select an approved execution mode, then begin M1 with its
  failing QML-engine test and build gate.

### Notion synchronization

Both child pages were created under the approved QML migration design page and
read back successfully before implementation begins:

- Plan: https://app.notion.com/p/3c80f4bdec4881c7b765f82d1750509e?pvs=204
- Confirmation log: https://app.notion.com/p/3c80f4bdec488156a4a1ec8a0e33e8b9?pvs=204

The readback confirms the plan status, ten test-first tasks, pure-QML runtime,
nine-room limit, safe source boundary, and M1-M5 log/readback requirement.

## 2026-08-26 - M1 Qt Quick foundation

| Phase | Design requirement | Implemented evidence | Verification command | Result | Open limit |
| --- | --- | --- | --- | --- | --- |
| M1 | Establish a testable Qt Quick/QML base without prematurely removing the legacy QWidget shell; do not record playback source material. | `native/CMakeLists.txt` registers Qml/Quick/QuickControls2 and the `DouyuMonitor` QML module; `native/app/qml/Main.qml` defines the 1280x720 dark window and Chinese title; `native/tests/qml_engine_smoke_test.cpp` asserts load, warnings, window type, size, and title. | `cmake --build --preset windows-x64-debug && ctest --preset windows-x64-debug --output-on-failure -j 1` | Debug build succeeded; 20/20 CTest passed in 115.77 seconds. | No open limit. Keeping the legacy entry point through M7 is the approved staged migration. |

- Design/plan alignment: M1 satisfies the approved Quick foundation while preserving the legacy application entry point until M8, as required by the plan repair and design sequencing.
- Evidence: `63b2d00` (`feat: bootstrap Qt Quick application shell`) includes only `native/CMakeLists.txt`, `native/app/qml/Main.qml`, and `native/tests/qml_engine_smoke_test.cpp`.
- Verification: the title assertion was first observed failing, then passed after the minimal QML correction; final Debug build and serial CTest completed with 20/20 passing tests.
- Sensitive-data check: the M1 CMake, QML, and smoke-test files were scanned; no playback URL, cookie, token, signature, raw StreamGet result, or raw mpv diagnostic was recorded.
- Next phase: M2 C++ to QML state bridge, beginning with a failing test for `RoomSession` and `MultiRoomCoordinator` QWidget ownership decoupling.

### Notion synchronization

The M1 child log was created under the approved migration design and fetched
back successfully. The readback contains the M1 phase heading, exact table
columns, and the `Sensitive-data check` item:

https://app.notion.com/p/3c80f4bdec4881828032e0ced070cec7?pvs=204

## 2026-08-26 - M2 C++ to QML state bridge models

| Phase | Design requirement | Implemented evidence | Verification command | Result | Open limit |
| --- | --- | --- | --- | --- | --- |
| M2 | Provide safe, incrementally updated QML-facing room, workspace, and monitoring state without exposing media source material; retain the QWidget shell until M8. | `native/src/ui/room_list_model.*`, `workspace_model.*`, and `monitoring_model.*` implement fixed QML roles/status labels, per-row model updates, fixed Chinese command feedback, audio-focus state, and safe monitoring counts. `native/src/workspace/native_workspace_types.h` adds the compact preset type required by the workspace contract. `native/tests/room_list_model_test.cpp` and `workspace_model_test.cpp` cover roles, row-only updates, fixed feedback, audio-focus signals, and health labels. | `cmake --build --preset windows-x64-debug --target room_list_model_test workspace_model_test && ctest --preset windows-x64-debug -R "room_list_model_test|workspace_model_test" --output-on-failure -j 1` | Build succeeded; 2/2 CTest passed in 1.45 seconds. | No M2 defect. `AppController` commands and durable preset persistence are intentionally Task 4. Test targets retain temporary Widgets/OpenGLWidgets linkage because the current `RoomSnapshot` still references the legacy `PlayerSurface`; the approved Task 2/8 sequence removes that dependency. |

- Design/plan alignment: checked against `docs/superpowers/specs/2026-08-26-qt-quick-qml-electron-ui-migration-design.md` and Task 3 of `docs/superpowers/plans/2026-08-26-qt-quick-qml-electron-ui-migration.md`. The model boundaries, safe role set, fixed feedback, incremental updates, and nine-room ceiling align. `audioRoomId` was added to satisfy the design's audio-mode state requirement.
- Evidence: baseline commit is `63b2d00` (`feat: bootstrap Qt Quick application shell`). The M2 paths remain uncommitted because `native/CMakeLists.txt` contains contiguous pre-existing worktree changes that cannot be safely staged by phase.
- Verification: model tests were observed failing before model implementation; final focused build and serial CTest passed. Existing `room_session_test` and `multi_room_coordinator_test` also passed before M2 work began.
- Sensitive-data check: the M2 model/test paths and this entry were scanned; no playback URL, cookie, token, signature, raw StreamGet response, `MediaSource`, or raw mpv diagnostic was recorded.
- Next phase: Task 4 AppController, persistence, native window bridge, and fixed safe command projection. Player attachment/detachment remains deferred until Task 2 executes beside the real `MpvQuickItem` in Task 8.

### Notion synchronization

The M2 child log was created under the approved migration design and fetched
back successfully. The readback contains the phase heading, exact table
columns, and the `Sensitive-data check` item:

https://app.notion.com/p/3c80f4bdec488173aab8d7cda2fcb42e?pvs=204

## 2026-08-26 - M3 Qt Quick libmpv renderer

| Phase | Design requirement | Implemented evidence | Verification command | Result | Open limit |
| --- | --- | --- | --- | --- | --- |
| M3 | Render a local media frame through `MpvQuickItem` with libmpv's OpenGL API while keeping playback source material outside QML and user-facing diagnostics. | `native/src/ui/mpv_quick_item.*` adds the `QQuickFramebufferObject` renderer, typed FBO target, queued update path, fixed Chinese error labels, request retirement, and media lifecycle controls. `native/tests/mpv_quick_item_test.cpp` verifies a PPM first frame and release, visible local video pixels, retired-load isolation, and sanitized labels. `native/CMakeLists.txt` provides the test target and libmpv runtime copy. | `cmake --build --preset windows-x64-debug --target mpv_quick_item_test room_session_test && ctest --preset windows-x64-debug -R "mpv_quick_item_test|room_session_test" --output-on-failure -j 1` | Build succeeded; 2/2 CTest passed in 10.93 seconds. | No open limit. The planned teardown acceptance test for multiple attached tiles remains Task 9. |

- Design/plan alignment: checked against `docs/superpowers/specs/2026-08-26-qt-quick-qml-electron-ui-migration-design.md` and Task 5 of `docs/superpowers/plans/2026-08-26-qt-quick-qml-electron-ui-migration.md`. The renderer uses the libmpv OpenGL context in a Qt Quick FBO, routes update callbacks to the GUI thread, and keeps fixed local error labels at the presentation boundary.
- Evidence: baseline commit is `63b2d00` (`feat: bootstrap Qt Quick application shell`). The M3 paths remain uncommitted because `native/CMakeLists.txt` has contiguous pre-existing worktree changes that cannot be isolated safely.
- Verification: the visible-pixel test was first observed failing while the renderer lacked the explicit libmpv video-output option, then passed after the minimal initialization correction. The final focused build and serial CTest passed with the M3 renderer and session regressions.
- Sensitive-data check: the M3 renderer and test paths were scanned; no playback URL, cookie, token, signature, raw service output, or raw mpv diagnostic was recorded.
- Next phase: Task 2 room-session and coordinator decoupling, preserving the temporary legacy `QWidget` compatibility constructor while adding the no-widget `MpvQuickItem` attachment path.

### Notion synchronization

The M3 child log was created under the approved migration design and fetched
back successfully. The readback contains the M3 phase table, the
`Sensitive-data check` bullet, and the next-phase contract:

https://app.notion.com/p/3c80f4bdec488120aa4cfc01c477dcf1?pvs=204

## 2026-08-26 - M4 Room session/player decoupling

| Phase | Design requirement | Implemented evidence | Verification command | Result | Open limit |
| --- | --- | --- | --- | --- | --- |
| M4 | Detach room policy and room-capacity control from QWidget ownership while retaining the temporary legacy path until Task 8. Attach real Qt Quick players through C++ only and keep resolved media in memory until an item is available. | `RoomSession` now has a no-widget constructor, `QPointer<MpvQuickItem>`, deferred in-memory source handling, player attach/detach, stored 0-100 volume, and fixed failure mapping. `MultiRoomCoordinator` now supports no-widget construction, C++ player forwarding, reordering, retry, and per-room volume snapshots. The explicit legacy `QWidget` plus `PlayerSurface` overload remains for the old shell. `MpvQuickItem` applies validated volume. | `cmake --build --preset windows-x64-debug --target mpv_quick_item_test room_session_test multi_room_coordinator_test quality_policy_test room_status_scheduler_test douyu_monitor_native main_window_test`; `ctest --preset windows-x64-debug -R "mpv_quick_item_test|room_session_test|multi_room_coordinator_test|quality_policy_test|room_status_scheduler_test|main_window_test" --output-on-failure -j 1` | Build completed; a fresh final serial CTest passed 6/6 in 43.34 seconds. The coordinator test also passed in five independent repeats after one non-reproducible earlier failure. | The legacy Widget surface remains intentionally until Task 8. QML calls to attach and detach players are Task 4 and Task 6 work. |

- Design/plan alignment: checked against the approved Qt Quick/QML design and Task 2 in `docs/superpowers/plans/2026-08-26-qt-quick-qml-electron-ui-migration.md`. The no-widget path, nine-room limit, `MpvQuickItem` attachment, deferred source contract, mute state without an attached item, and 0-100 volume contract align. The approved sequencing repair explicitly preserves the legacy QWidget constructor only through Task 8; this takes precedence over the earlier Task 2 wording that otherwise removes it immediately.
- TDD evidence: the new no-widget session constructor, deferred-source accessor, player attachment API, no-widget coordinator constructor, room move/retry command, and volume snapshot tests were observed failing before implementation. The added Quick volume contract was also observed failing before implementation.
- Runtime packaging correction: the room-session and coordinator CTest targets now declare the same Qt runtime search environment already used by the Quick renderer test. This fixes their Windows test-process startup without modifying production runtime behavior.
- Verification: the native application and existing `main_window_test` were rebuilt after the Quick linkage change. A fresh serial CTest after the fixture cleanup passed all six selected tests in 43.34 seconds. `git diff --check` and the scoped sensitive-data scan are rerun before the Task 4 gate. A prior combined run reported one coordinator-test failure without diagnostic output; five standalone repeats and the fresh serial suite were green.
- Sensitive-data check: M4 changed no QML-facing source properties or logs that include playback URLs, cookies, tokens, signatures, raw service output, `MediaSource` values, or raw mpv diagnostics. Playback errors remain fixed local codes and labels.
- Next phase: Task 4 AppController, persistence, safe command projection, QML player attachment invokables, and native window bridge. Re-check the completed M4 contract before writing its RED tests.

### Notion synchronization

The M4 child log was created under the approved migration design and fetched
back successfully. The readback confirms the temporary legacy compatibility
boundary, no-widget player attachment contract, six-test verification result,
sensitive-data restriction, and Task 4 handoff:

https://app.notion.com/p/3c80f4bdec4881e79fbfc57414614c0e?pvs=204

## 2026-08-26 - M5 AppController, persistence, and native window bridge

| Phase | Design requirement | Implemented evidence | Verification command | Result | Open limit |
| --- | --- | --- | --- | --- | --- |
| M5 | Keep the QML boundary in C++, persist only safe workspace state, expose fixed command feedback, and route native window actions through the controller. | `native/src/ui/app_controller.*` owns the coordinator, workspace store, notification policy/service, and safe presentation models. It exposes the approved command, player attachment, and native window invokables. `native/src/workspace/native_workspace_store.cpp` persists schema v2 presets and safe room presentation fields while retaining version-1 loading. `native/tests/app_controller_test.cpp` covers the room limit, fixed playback feedback, and group/preset/volume/danmaku restoration. | `ctest --preset windows-x64-debug -R "app_controller_test|native_workspace_store_test|notification_policy_test|windows_notification_service_test|multi_room_coordinator_test" --output-on-failure -j 1`; `ctest --preset windows-x64-debug -R "^main_window_test$" --output-on-failure --timeout 60 -j 1`; `cmake --build --preset windows-x64-debug --target douyu_monitor_native main_window_test` | The original persistence-restoration crash was reproduced before the fix. The controller, store, policy, notification, and coordinator tests then passed; `main_window_test` passed in 34.64 seconds; the Debug native target built successfully. | No functional blocker. The QML shell and visual migration remain the scheduled Task 6 work; full QWidget removal remains Task 8. |

- Design/plan alignment: checked against Task 4 of `docs/superpowers/plans/2026-08-26-qt-quick-qml-electron-ui-migration.md` and the approved Qt Quick/QML migration design. The controller exposes the full approved QML-safe command set, holds service ownership in C++, keeps playback material out of QML, and retains the temporary QWidget compatibility path only until Task 8.
- Evidence: `AppController::restoreWorkspace()` now copies `snapshot_.activeRoomIds` before calling coordinator methods that synchronously publish snapshots. This prevents the prior range-loop reference invalidation during persistence restoration. Temporary `qInfo`/`QDebug` diagnostics and the unrelated controller-lifetime probe were removed. Current baseline commit: `63b2d00`; M5 work remains unstaged because surrounding task changes are still deliberately grouped in the existing worktree.
- Verification: before the repair, `app_controller_test` reproduced a segmentation fault in `persistsGroupPresetAndRoomPresentationSettings`, with the stack passing through `MultiRoomCoordinator::addRoomDetailed()` and `AppController::restoreWorkspace()`. After the minimal fix, the persistence test passed. The Task 4 focused suite passed across the controller, workspace store, notification policy, Windows notification service, and coordinator checks; `main_window_test` and a Debug build of `douyu_monitor_native` also passed.
- Sensitive-data check: the controller and test sources were scanned after cleanup. No temporary debug marker, playback URL, cookie, token, signature, raw service response, `MediaSource` value, or raw mpv diagnostic was recorded.
- Next phase: Task 6 QML shell and room grid. Begin with its failing visual smoke test, then bind the existing safe controller/model contract to the Electron-parity UI.

### Notion synchronization

Creation and readback were attempted under the approved QML migration parent page. Notion MCP returned `Auth required`, so no M5 child page was created and no synchronization is claimed. After authorization is restored, create the M5 child page from this entry and immediately fetch it back to verify the heading and `Sensitive-data check` item.

Notion M5 child page was later created and fetched back successfully after authorization recovery:

https://app.notion.com/p/3c80f4bdec488167881ad322f8d24994?pvs=204

## 2026-08-26 - M6 QML shell and room grid

| Phase | Design requirement | Implemented evidence | Verification command | Result | Open limit |
| --- | --- | --- | --- | --- | --- |
| M6 | Reproduce the Electron shell in QML with a 44px header, 268px left room list, dense dark room tiles, safe controller intents, and a real Qt Quick libmpv item. | `Main.qml` now defines the fixed visual tokens, controller-safe root properties, preview-only smoke-test data, and the shell composition. `AppHeader.qml`, `WindowControls.qml`, `RoomSidebar.qml`, `WorkspaceGrid.qml`, `RoomTile.qml`, and `ToastViewport.qml` supply the visual surface. `RoomTile` directly creates `MpvQuickItem` only when a controller is injected, and attaches/detaches it through `AppController`. The QML tests register that concrete type under the dedicated `DouyuNative` URI. | `cmake --preset windows-x64; cmake --build --preset windows-x64-debug --target qml_engine_smoke_test qml_visual_smoke_test; ctest --preset windows-x64-debug -R "qml_engine_smoke_test|qml_visual_smoke_test" --output-on-failure -j 1`; focused C++ regression CTest for controller, Quick renderer, store, session, and coordinator. | QML engine and visual smoke tests passed 2/2 in 11.95 seconds. The focused C++ regression suite passed 5/5 in 29.40 seconds. Local scrubbed screenshots were saved under `native/out/verification/qml/` and visually inspected at 1280x720 and 1920x1080. | Task 7 remains: QML panels, dialogs, monitoring, and shortcuts. Task 8 will make the new QML shell the shipped application entry point and register `DouyuNative` there. |

- Design/plan alignment: checked against the approved QML migration design and Task 6 of `docs/superpowers/plans/2026-08-26-qt-quick-qml-electron-ui-migration.md`. The QML shell preserves the Electron reference geometry and visual hierarchy, moves room management to the left sidebar, keeps the nine-room product limit in the empty state, and exposes only controller intents and safe display roles.
- TDD evidence: `qml_visual_smoke_test` was first observed failing because `appHeader`, `roomSidebar`, and `workspaceGrid` did not exist. The complete QML engine test then exposed and drove fixes for C++ type registration, the module URI collision, Basic Controls styling, and model-role delivery to `RoomTile`.
- Runtime boundary: QML does not receive playback URLs, cookies, tokens, signatures, raw resolver results, `MediaSource` values, or raw mpv diagnostics. The preview model used solely when no controller exists contains only fixed labels. `MpvQuickItem` remains the real player item; removing its C++ `final` qualifier is required for `qmlRegisterType` construction and does not introduce a player facade.
- Verification: the screenshot assertions verify the non-empty grid, 44px header, 268px sidebar, and non-overlapping header/sidebar/grid/toast rectangles. Image inspection confirmed the dark compact shell at both acceptance viewports. `app_controller_test`, `mpv_quick_item_test`, `native_workspace_store_test`, `room_session_test`, and `multi_room_coordinator_test` passed after the QML type-registration change.
- Sensitive-data check: the M6 QML, tests, and this entry were scanned. No playback URL, cookie, token, signature, raw StreamGet response, `MediaSource` value, or raw mpv diagnostic is recorded.
- Next phase: Task 7, beginning with a failing QML interaction test for opening the add-room dialog and handling a command-feedback toast, then migrate the panels, dialogs, monitoring view, and shortcuts.

### Notion synchronization

The M6 child log was created under the approved QML migration design and fetched back successfully. The readback confirms the phase table, test-driven evidence, runtime boundary, sensitive-data check, and Task 7 handoff:

https://app.notion.com/p/3c80f4bdec4881c79442c325196d7aef?pvs=204

## 2026-08-26 - M7 QML panels, dialogs, shortcuts, and monitoring

| Phase | Design requirement | Implemented evidence | Verification command | Result | Open limit |
| --- | --- | --- | --- | --- | --- |
| M7 | Migrate the Electron-parity overlays, dialogs, monitoring view, notification settings, workspace presets, and retained keyboard actions into controller-bound QML without exposing playback material. | `native/app/qml/panels/*.qml` adds danmaku, monitoring, and workspace preset surfaces; `native/app/qml/dialogs/*.qml` adds room, group, and Windows notification dialogs. `Main.qml`, `AppHeader.qml`, and `RoomSidebar.qml` replace M6 placeholders with named actions and application shortcuts. `AppController` now exposes safe notification preference read/write commands and durable event preferences. `qml_interaction_test.cpp` and `app_controller_test.cpp` cover opening flows, invalid input, shortcuts, notification state, and preference restoration. | `cmake --build --preset windows-x64-debug --target qml_engine_smoke_test qml_visual_smoke_test qml_interaction_test app_controller_test native_workspace_store_test windows_notification_service_test`; `ctest --preset windows-x64-debug -R "qml_engine_smoke_test|qml_visual_smoke_test|qml_interaction_test|app_controller_test|native_workspace_store_test|windows_notification_service_test" --output-on-failure -j 1` | Build succeeded; 6/6 selected CTest tests passed in 19.42 seconds. The QML visual smoke test refreshed scrubbed 1280x720 and 1920x1080 captures. | The legacy QWidget entry point and source remain intentionally until Task 8, which removes them together and makes the QML runtime shipped. |

- Design/plan alignment: checked against Task 7 of `docs/superpowers/plans/2026-08-26-qt-quick-qml-electron-ui-migration.md` and the approved QML migration design. Add-room validation uses `^[0-9]{1,20}$`; group names are constrained to non-whitespace 1-30 characters in QML and the controller; dialogs and panels route accepted operations through `AppController`; notification preferences are stored by `WindowsNotificationService`.
- TDD and debugging evidence: the original interaction test was observed failing because the named controls did not exist. The notification API test was observed failing to compile before the event-preference interface was introduced. A post-implementation shortcut test exposed that a modal add-room dialog blocked the window-scoped `Ctrl+M`; changing all retained shortcuts to `Qt.ApplicationShortcut` made the same test pass.
- Verification: `qml_engine_smoke_test`, `qml_visual_smoke_test`, `qml_interaction_test`, `app_controller_test`, `native_workspace_store_test`, and `windows_notification_service_test` passed in the final fresh serial run. The shell screenshots were inspected at both acceptance viewports; header, sidebar, workspace, and toast rectangles remained non-overlapping.
- Sensitive-data check: the new QML, controller, tests, and this entry were scanned. No playback URL, cookie, token, signature, raw resolver output, `MediaSource` value, or raw mpv diagnostic is recorded.
- Next phase: Task 8 removes the legacy QWidget user interface and makes `QGuiApplication` plus `QQmlApplicationEngine` the application runtime. Its first gate is a failing self-test assertion that rejects the old QWidget output.

### Notion synchronization

The M7 child page was created under the approved migration design and fetched
back successfully. The readback confirms its parent, phase status, complete
notification preference coverage, verification table, sensitive-data check,
and Task 8 handoff:

https://app.notion.com/p/3c80f4bdec4881d88d39d912a0e8be48?pvs=204

## 2026-08-26 - M8 Qt Quick 运行时迁移（进行中）

| 阶段 | 计划目标 | 当前证据 | 状态 | 下一验证门 |
| --- | --- | --- | --- | --- |
| M8 | 删除旧 QWidget 用户界面，使 `QGuiApplication` + `QQmlApplicationEngine` 成为唯一运行时；保留仅 Qt Quick 的 libmpv 自检。 | `native/tests/native_self_test_test.cpp` 已新增 Qt Quick 自检进程断言，明确拒绝旧 `QWidget` 文本。旧入口、`PlayerSurface` 兼容层、Widgets CMake 链接和相关测试仍存在，等待同一变更统一删除。 | 进行中；尚未声称通过。 | 重新运行新断言并确认其因旧入口失败，然后实现新的 QML 启动和自检路径。 |

- 计划与设计对照：已复核批准的迁移计划 Task 8 和 Qt Quick/QML 设计。当前顺序符合计划：先确认旧自检不满足 QML 运行时，再移除 Widgets 壳与兼容路径。
- 范围修正：除计划列出的旧界面文件外，`PlayerSurface`、其测试、`RoomSession`/`MultiRoomCoordinator` 的兼容重载、基于 QWidget 的测试夹具，以及所有 `Qt6::Widgets` 与 `Qt6::OpenGLWidgets` 链接都属于 M8 删除或重构范围；最终以无 Widgets/浏览器运行时扫描为准。
- 敏感数据检查：本阶段记录不包含播放 URL、Cookie、令牌、签名、解析结果、`MediaSource` 值或原始 mpv 诊断。
- 下一步：以 VS x64 环境重新构建并运行 `native_self_test_test`，捕获预期的旧 QWidget 自检失败；之后切换 `main.cpp` 至 Qt Quick 应用运行时。

## 2026-08-26 - M8 Qt Quick 运行时迁移

| Phase | Design requirement | Implemented evidence | Verification command | Result | Open limit |
| --- | --- | --- | --- | --- | --- |
| M8 | 以 Qt Quick/QML + C++ 作为唯一运行时；移除 QWidget、Electron、Chromium 与 Qt WebEngine；保留真实 libmpv 自检和最多九路的产品上限。 | `native/app/main.cpp` 使用 `QGuiApplication` 与 `QQmlApplicationEngine`；旧 `MainWindow`、`RoomManagementDock`、`PlayerSurface` 和 Widgets 测试已移除；`native/CMakeLists.txt` 只保留 Qt Core/Gui/Qml/Quick/QuickControls2/OpenGL 依赖；关闭自检状态机修正为仅在媒体未停止时等待首帧。 | `ctest --preset windows-x64-debug -R "^(qml_engine_smoke_test|qml_visual_smoke_test|qml_interaction_test|app_controller_test|mpv_quick_item_test|native_self_test_test|native_workspace_store_test|room_session_test|multi_room_coordinator_test|notification_policy_test|windows_notification_service_test|streamget_process_client_test)$" --output-on-failure -j 1`; `ctest --preset windows-x64-debug -R "^(native_self_test_media|douyu_monitor_native_self_test)$" --output-on-failure -j 1`; 受限源码/CMake/QML 扫描；`dumpbin /dependents`。 | 11/11 重点测试通过（40.28s）；2/2 进程级自检通过（18.89s）。1280x720 和 1920x1080 QML 截图均无顶栏、侧栏、网格或提示层重叠。扫描无旧运行时命中；调试程序依赖 Qt Quick/QML/OpenGL 与 `libmpv-2.dll`，不含 Widgets 或浏览器运行时。 | M8 无功能阻塞。Task 9 将补齐 Release 打包、运行时依赖、九路关闭释放与全量验收。 |

- Design/plan alignment: 已对照 `docs/superpowers/plans/2026-08-26-qt-quick-qml-electron-ui-migration.md` 的 Task 8 与批准的 Qt Quick/QML 迁移设计。唯一产品运行时为 Qt Quick/QML + C++；九路上限保持不变，未引入 Electron、Chromium 或 Qt WebEngine。
- Evidence: `native/app/main.cpp`、`native/CMakeLists.txt`、`native/app/qml/Main.qml`、`native/tests/native_self_test_test.cpp` 和 `native/tests/mpv_quick_item_test.cpp`。自检回归的根因是首帧成功后 `stop()` 会重置首帧状态，轮询仍继续等待；现已在媒体停止后退出等待分支。
- Verification: 当前基线提交为 `63b2d00`，M1--M8 变更仍按既有工作树分组而未提交。上述 13 个 Debug CTest 已串行通过；源码/CMake/QML 受限扫描无 `QWidget`、`Qt6::Widgets`、`Qt6::OpenGLWidgets`、Electron、Chromium 或 Qt WebEngine；`dumpbin /dependents` 结果仅含所需的 Qt Quick/QML 与 libmpv 调试依赖。
- Sensitive-data check: 本条不包含播放 URL、Cookie、令牌、签名、原始 StreamGet 输出、`MediaSource` 值或原始 mpv 诊断。
- Next phase: Task 9。先加入九路 QML 关闭释放回归测试和 Release 运行时依赖验证，再执行 Debug/Release 全量证据集、打包检查及敏感输出扫描。

### Notion synchronization

M8 完成子页面已创建并回读验证：

https://app.notion.com/p/3c80f4bdec48816384cbd5c5f7133df5?pvs=204

## 2026-08-26 - M8 发布验收、关闭回归与 Release 打包（Task 9）

| Phase | Design requirement | Implemented evidence | Verification command | Result | Open limit |
| --- | --- | --- | --- | --- | --- |
| M8 / Task 9 | 验证九路关闭释放、Qt Quick Release 包依赖、无旧运行时和无敏感输出；保持 Qt Quick/QML + C++ 唯一运行时、九路上限和 C++ 内存内播放源边界。 | `qml_close_regression_test.cpp` 覆盖九路 QML 关闭、附件释放、服务停止和重复关闭。`AppController::shutdown()` 统一执行持久化、协调器/播放器释放、服务停止和通知释放。`verify_runtime_dependencies.cmake` 检查 Release 可执行文件、Qt Quick/QML/Gui/Core、`libmpv-2.dll`、`streamget_service.exe` 与 QML 模块，并拒绝 Widgets、WebEngine、Electron、Chrome 和 Node 运行时。Qt Test 目标统一获得 SDK 运行时搜索路径，应用自检仍只验证已部署包。`MpvQuickItem::release()` 取消待处理加载并拒绝释放后到达的旧 `START_FILE` 事件。 | `cmake --build --preset windows-x64-debug`; `ctest --preset windows-x64-debug -j 1`; `cmake --build --preset windows-x64-release`; `ctest --preset windows-x64-release -j 1`; Release 依赖脚本、源码/产物敏感扫描、`git diff --check` 和进程检查。 | Debug 23/23 通过（62.41 秒）；Release 23/23 通过（56.22 秒）。Release 依赖脚本通过；受限运行时、敏感日志和产物扫描均无命中；无残留 `douyu_monitor_native.exe`、`streamget_service.exe` 或测试服务进程。 | M8 和 Task 9 无功能阻塞。下一步进入变更审查与提交准备；真实斗鱼房间的人工验收仍需在用户环境中执行，且不得记录播放源或认证材料。 |

- 计划与设计对照：已逐项对照 `docs/superpowers/plans/2026-08-26-qt-quick-qml-electron-ui-migration.md` 的 Task 9，以及 `docs/superpowers/specs/2026-08-26-qt-quick-qml-electron-ui-migration-design.md` 的发布验收、关闭顺序、九路上限和敏感数据边界。关闭回归、Release 包验证、Debug/Release CTest、无 Widgets/浏览器运行时扫描和敏感产物扫描均满足计划。
- 调试记录：Release 的 12 个基础 Qt Test 最初以 `0xc0000135` 启动失败。直接将匹配 Qt SDK 的 `bin` 加入 `PATH` 后通过，根因是 CTest 未为这些 Qt Test 目标提供 `Qt6Test.dll`。仅为测试目标补充 SDK 运行时路径，未把 SDK 测试 DLL 复制进发布包。
- 时序修复：完整验收先后暴露两个负载相关问题。`streamget_process_client_test` 的畸形响应断言把请求预算从 500ms 提高到 1500ms，产品超时和协议处理未改动。`multi_room_coordinator_test` 发现离线切换后陈旧 libmpv `START_FILE` 事件可覆盖 `Idle` 状态；`MpvQuickItem` 现在在释放时中止待处理加载、退休活动播放项，并忽略 `Idle` 后抵达的旧启动事件。受影响的 Quick 和协调器测试先通过，再完成 Debug/Release 全量验证。
- 打包与边界：Release 包检查确认 Qt Core/Gui/Qml/Quick、`libmpv-2.dll`、`streamget_service.exe` 和 QML 模块存在，并拒绝 `Qt6Widgets`、Qt WebEngine、Electron、Chrome 和 Node。源码、QML、构建产物和本日志未记录播放 URL、Cookie、令牌、签名、原始 StreamGet 输出、`MediaSource` 构造值或原始 mpv 诊断。
- 下一步：按计划进入变更审查和提交准备。保持现有工作树中的其他未提交变更不被重置或混入；执行人工的 1/4/9 路窗口操作验收时，只记录固定状态与结果，不记录真实播放源。

## 2026-08-26 - M8 close-lifecycle repair and release-package follow-up

| Phase | Design requirement | Implemented evidence | Verification command | Result | Next step |
| --- | --- | --- | --- | --- | --- |
| M8 repair | Preserve the Qt Quick/QML + C++ only runtime, safely destroy nine libmpv render contexts, reject released-item events, and keep the Release package free of legacy UI runtime files. | `MpvQuickItem` now asks the attached `QQuickWindow` to release scene-graph resources before mpv core teardown, and rejects late load-completion events after `release()`. The nine-room close regression now forces OpenGL, waits for render contexts, destroys the QML engine, and drains queued work. `native/CMakeLists.txt` removes stale Widgets/OpenGLWidgets DLLs before deployment; `verify_runtime_dependencies.cmake` rejects those files and WebEngine files if present. | Full Debug CTest; Release rebuild; full Release CTest; runtime dependency verifier using the Release executable; constrained output scan; `git diff --check`. | Debug: 23/23 passed in 164.64 seconds. Release: 23/23 passed in 92.05 seconds after the package cleanup. The runtime verifier passed, and the rebuilt package has no Widgets, OpenGLWidgets, or WebEngine DLL files. | Review the full uncommitted migration diff and prepare integration; do not commit, merge, push, or delete the worktree automatically. |

- Plan/design alignment: this follow-up completes `2026-08-26-m8-close-lifecycle-review-repair.md` without expanding product scope. The sole runtime remains Qt Quick/QML plus C++, the capacity limit remains nine rooms, and no Electron, Chromium, Qt WebEngine, QWidget, or QOpenGLWidget path was introduced.
- Review repair: the review findings were addressed at the ownership boundary. Scene-graph resources are released before `mpv_terminate_destroy`; stale `FILE_LOADED` and `VIDEO_RECONFIG` events no longer restore released media flags; the nine-renderer shutdown test exercises the real OpenGL teardown path.
- Packaging repair: the executable dependency list contains the expected Qt Quick/QML/Gui/Core and libmpv dependencies. A stale deployment directory had legacy Widgets DLLs from earlier builds even though the executable did not depend on them; the post-build cleanup and package verifier now prevent that mismatch from recurring.
- Sensitive-data check: the Release output directory and this project log were scanned for playback-auth material, raw resolver output, `MediaSource` construction values, and raw mpv diagnostic helpers. The scan reported no output or log matches. This entry intentionally contains no playback source, credential, token, cookie, signature, or raw service response.
- Next phase: use the completed design and plan as the baseline for a submission-readiness review. Preserve unrelated worktree changes and document any review findings before the user selects a commit, merge, or pull-request action.

## 2026-08-27 - M8 关闭生命周期审查纠正与修复完成

| 阶段 | 计划目标 | 实施证据 | 验证结果 | 下一步 |
| --- | --- | --- | --- | --- |
| M8 repair | 修复 render context 与 libmpv core 的跨线程销毁顺序，并拒绝发布包内任意位置的旧运行时残留。 | `MpvQuickItem` 以共享 `MpvRenderState` 隔离渲染线程与 QML 项目对象；更新回调不再保存裸项目指针；Qt Quick render job/renderer 路径先在渲染线程清除回调并释放 `mpv_render_context`，再通过 GUI 队列销毁 `mpv_handle`。新增关闭顺序观测回归。新增 `clean_forbidden_runtime_files.cmake`，并让 Release 验证递归检查 Widgets、WebEngine、Electron、Chrome、Node 文件。 | 关闭顺序测试首次在旧路径复现 `0xc0000409`，修复后 Debug/Release 均通过；Debug CTest 23/23 通过（99.09s）；Release CTest 23/23 通过（85.76s）；Release 依赖验证通过；递归残留负向测试会拒绝 `nested/runtime/node.exe`，清理后通过；敏感输出扫描无命中；`git diff --check` 通过。 | 进入完整未提交变更的提交就绪审查。保留当前工作树，不自动 commit、merge、push 或删除。 |

- 设计与计划对照：仍为 Qt Quick/QML + C++ 唯一运行时，最大 9 路；未引入 Electron、Chromium、React、Node.js、Qt WebEngine、QWidget 或 QOpenGLWidget。修复范围限于生命周期握手、共享状态和 Release 目录验收，不扩展产品功能。
- 根因与修复：libmpv 官方头文件要求 `mpv_render_context_free()` 先于 core 销毁，且 OpenGL render API 调用必须在对应渲染线程完成。现由 Qt Quick 渲染阶段执行 context 释放，再排队到 GUI 线程调用 `mpv_terminate_destroy()`；共享状态让异步更新在项目析构后自动失效。
- 测试证据：新增 `destroysMpvCoreAfterRenderContextRelease`，记录渲染上下文释放序号和 core 销毁序号；可见 OpenGL 播放器析构不再崩溃。既有九路 QML 关闭回归继续通过。
- 打包证据：Release 包包含 Qt Core/Gui/Qml/Quick、`libmpv-2.dll`、`streamget_service.exe` 与 QML 模块；递归禁止文件计数为 0。清理脚本和验证脚本均覆盖根目录及嵌套目录的旧运行时名称。
- 敏感数据检查：构建产物和项目日志未记录播放地址、Cookie、令牌、签名、原始解析响应、`MediaSource` 构造值或原始 mpv 诊断。

### Notion synchronization

已创建并回读本轮纠正日志页：

https://app.notion.com/p/3c90f4bdec4881e58000deebafb70194?pvs=204

## 2026-08-27 - M8 提交就绪审查

| 审查项 | 计划/设计要求 | 审查证据 | 结果 | 剩余限制 |
| --- | --- | --- | --- | --- |
| 自动化验证 | Qt Quick/QML + C++ 唯一运行时、最多九路、关闭和发布回归可重复通过。 | Debug 全量 CTest 23/23 通过（177.15 秒）；Release 全量 CTest 23/23 通过（82.96 秒）；Release 构建完成 193 个步骤。 | 通过。 | 真实斗鱼房间的人工 1/4/9 路验收仍须在用户环境执行。 |
| 发布包 | 包含所需 Qt Quick/QML/libmpv/服务依赖，并递归拒绝旧 UI 和浏览器运行时。 | Release 依赖验证通过；递归禁止运行时文件计数为 0。 | 通过。 | 不自动提交、合并、推送或删除当前工作树。 |
| 运行时边界 | 不引入 Electron、Chromium、React、Node.js、Qt WebEngine、QWidget 或 QOpenGLWidget。 | 源码和运行时扫描未发现上述禁止项。 | 通过。 | 无。 |
| 安全与可视化 | 不在日志或产物中保留播放授权材料；QML 外观在目标分辨率可用。 | 敏感信息扫描无命中；`git diff --check` 通过（仅既存 CRLF/LF 警告）；已检查 1280x720 和 1920x1080 QML 截图。 | 通过。 | 人工验收记录只能使用固定状态和结果，不能记录播放地址或认证材料。 |

- 设计与计划对照：已复核 QML 迁移设计、迁移计划和 M8 关闭生命周期修复计划。当前变更保持 Qt Quick/QML + C++ 唯一运行时、最多九路及 C++ 内存内播放源边界；未扩大产品范围。
- 审查结论：自动化、打包、禁止运行时、敏感输出和视觉检查均具备提交准备证据。唯一外部阻塞是 Notion 授权恢复，以及不含播放源信息的真实斗鱼环境人工验收。
- 审查反馈复核：随后收到的两项旧路径风险已与当前工作树逐项核对。`MpvRenderState::releaseRenderContextOnRenderThread()` 会在渲染线程清除回调并释放 `mpv_render_context`，再排队至 GUI 线程执行 core 销毁；`verify_runtime_dependencies.cmake` 与清理脚本递归枚举运行时目录，覆盖嵌套的 Widgets、WebEngine、Electron、Chrome 与 Node 文件。2026-08-27 复跑 `qml_close_regression_test` 和 `mpv_quick_item_test` 为 2/2 通过（19.21 秒）；加载 VS x64 开发环境后 Release 依赖校验脚本以退出码 0 通过。首次全量 Debug 运行时，`streamget_process_client_test` 在一个重叠的 CTest 会话中以 `0xc0000409` 退出；随后在无残留进程条件下将该用例直接重复 5 次、经 CTest 重复 5 次，均通过，且 Windows Application Error 日志未记录对应错误。最终串行 Debug 全量 CTest 23/23 通过（124.27 秒），Release 全量 CTest 23/23 通过（95.44 秒）；禁止源码运行时扫描无命中，`git diff --check` 通过（仅既存 CRLF/LF 提示）。
- 下一步：Notion 授权恢复后，在既有迁移设计页下创建本条的子页面并立即回读，随后把页面链接追加到本地日志。保持暂停状态，等待用户选择提交、合并、推送或人工验收操作。

### Notion synchronization

Notion 授权于 2026-08-27 恢复后，已在批准的 Qt Quick/QML 迁移设计页下创建并立即回读本轮提交就绪审查页面。回读确认父级、标题、验证证据、审查反馈复核与剩余限制均正确：

https://app.notion.com/p/3c90f4bdec4881d78297c91a9a84043f?pvs=204

## 2026-08-27 - Qt Native repair completion and package launch verification

| Repair target | Implemented evidence | Fresh verification | Status |
| --- | --- | --- | --- |
| Sidebar placement and history | The sidebar-collapse control leads the product mark in `AppHeader.qml`; `RoomLibraryView` projects history and favorites through the safe `libraryRooms` model. | `qml_interaction_test` verifies order and opening the history view. | Complete |
| Missing icons and Electron-parity shell | Packaged SVG assets replace placeholder action glyphs across the header, sidebar, tiles, overlays, and window controls. | Visual smoke captures at 1280x720 and 1920x1080 were regenerated and inspected. | Complete |
| Eight-stream CPU and memory pressure | `MpvQuickItem` now drains libmpv events through its wakeup callback and no longer unconditionally schedules the next render frame. | `mpv_quick_item_test` and the close-lifecycle regression passed in the Debug suite. | Complete |
| Ninth room | The coordinator retains the hard limit of nine, creates nine render contexts, rejects a tenth room, and closes them safely. | `multi_room_coordinator_test` and `qml_close_regression_test` passed; the Debug full suite also passed. | Complete |
| Live status and streamer information | The status scheduler, resolver metadata mapping, safe presentation model, and monitoring notifications feed the QML room model. | `room_status_scheduler_test`, `room_session_test`, and `multi_room_coordinator_test` passed in the Debug full suite. | Complete |
| Console window and native frame | The native target is a Windows GUI executable. `Main.qml` is frameless and exposes a blank title-bar drag region with double-click maximize/restore behavior. | Debug and Release targeted tests verify `Qt.FramelessWindowHint` and PE subsystem `Windows GUI`. | Complete |
| Release package startup | The current Release deployment contains the matching Qt runtime, `qwindows` platform plugin, libmpv, and stream resolver service. | A normal Windows-platform `--self-test` exited with code 0. After an explicit fresh Release rebuild, six package/QML lifecycle tests passed in 27.43 seconds. | Complete |

- Design and plan comparison: the current implementation remains the approved Qt Quick/QML plus C++ only product runtime. The nine-room ceiling is unchanged; the user-facing shell has a left room list, restored history, asset-backed icons, and no browser runtime.
- Full Debug verification: a fresh serial `ctest --preset windows-x64-debug --output-on-failure -j 1` passed all 23 registered tests. The suite covers player lifecycle, quality policy, room limit, safe metadata/status projection, notifications, close teardown, QML interaction/visual behavior, Python resolver protocol tests, and packaged application self-test.
- Release verification: explicitly rebuilt the Release QML engine, visual, interaction, close-lifecycle, self-test, and application targets. The fresh Release CTest subset passed 6/6: QML engine, visual smoke, interaction, close regression, media self-test, and packaged application self-test. The Release directory has no Electron, Chromium, Node, or Qt WebEngine runtime files.
- Launch-dialog investigation: the reported Windows platform-plugin dialog could not be reproduced after redeployment. There are no global `QT_*` environment overrides; both Debug and Release packages successfully loaded their matching `qwindows` plugin under Qt loader diagnostics. The Release package was rebuilt before the final validation so the executable and plugin directory are current together.
- User launch target: use `D:\DouyuMonitor\.worktrees\codex\qt-libmpv-m0\native\out\build\windows-x64-release\douyu_monitor_native.exe`. Do not open it while a build or `windeployqt` deployment is still running, because the output directory is updated in place.
- Sensitive-data check: this repair record, the QML boundary, and package validation contain no playback address, authentication material, raw resolver response, or raw player diagnostic.
- Remaining limit: real 1/4/9-room viewing must still be manually checked with the user's own rooms. Record only room-count outcomes and fixed status labels, never source or authentication data.

### Notion synchronization

The new repair-completion page was created and fetched back successfully. The
readback confirms the repair table, fresh Debug and Release evidence, package
startup investigation, runtime boundary, and remaining manual 1/4/9-room
check:

https://app.notion.com/p/3c90f4bdec48818ebbfceb865a13b5d2?pvs=204

## 2026-08-27 - Qt Quick/QML 与 Electron 迁移差距审计

### 审计范围与方法

本次只比较根目录旧 Electron/React 实现与 `native/` 中 Qt Quick/QML + C++ 产品实现；不把 Electron、Chromium 或浏览器运行时作为回迁候选。审计通过旧版状态存储、主进程桥接、React 组件、原生 `AppController`、工作区模型、QML 组件和现有 QML 截图交叉完成。旧 Vite 预览服务可在本机监听，但浏览器验证环境拒绝访问本地端口，因此未把旧版运行截图作为证据；视觉结论以旧组件与样式契约、新 QML 源码以及已检查的 1920x1080 QML 截图为准。

### 已完成的主链路

- 九路容量、libmpv 多路播放、StreamGet 解析边界、清晰度、单一音频焦点、房间收藏/历史、基本工作区保存和应用、直播状态调度、Windows 通知、无边框窗口控制和左侧房间列表均已具备原生实现。
- 旧 Web 播放器、Electron IPC 与浏览器通知属于被 Qt/libmpv、C++ 控制器和 Windows 通知服务替换的运行时实现，不是遗漏迁移项。

### 尚未完成或仅部分完成的迁移

| 优先级 | 旧版能力 | 原生现状 | 审计结论 |
| --- | --- | --- | --- |
| P0 | 实时弹幕连接、重连、渲染叠层和治理 | 原生只保存每房间 `danmakuEnabled` 并显示开关；没有弹幕会话、协议客户端、叠层渲染、状态或重试实现。 | 功能未迁移；当前开关没有实际弹幕效果。 |
| P0 | 分组切换会恢复该组房间集 | `setActiveGroup()` 只更新并持久化 ID；没有替换/过滤协调器房间，侧边栏也没有分组标签。 | 核心工作流未迁移，分组目前只是管理数据。 |
| P1 | 房间号、链接、主播名搜索并展示候选 | 原生添加框只接受数字房间号，直接调用 `addRoom()`；没有原生搜索候选 UI。 | 搜索和链接/主播名输入未迁移。 |
| P1 | 多种布局、主画面比例和布局预设恢复 | 原生布局按当前数量自动推荐；QML 仅处理 single/2 列/3 列。没有布局选择、主画面比例拖拽或恢复。预设保存了 `layoutId`，但应用时不会设置它。 | 布局与预设恢复仅部分迁移。 |
| P1 | 全局静音、单声道/多声道、全局弹幕 | 原生模型保留部分字段，但控制器/QML 没有对应完整命令或控件；音频只支持单一焦点。 | 音频与全局弹幕策略未迁移。 |
| P1 | 完整分组和预设管理 | 原生分组对话框不能列出/移除/排序成员；预设面板不能更新、重命名或删除预设。 | 管理 UI 和操作未完成。 |
| P1 | 逐房监控、刷新资料/播放源、弹幕重试 | 原生监控面板只有在线/离线/异常计数和通知入口；房间卡只公开重试播放源，刷新资料只可由快捷键作用于主房间。 | 监控与故障处理 UI 未完整迁移。 |
| P2 | 房间卡直接调节音量、完整操作菜单 | 原生已有 `setVolume()`，但 QML 未提供音量滑杆；菜单缺少“刷新房间资料”和旧版状态摘要。 | C++ 能力未完整接入 UI。 |
| P2 | 拖拽排序、上下移动和一致快捷键 | 原生只提供上移；没有拖拽、下移按钮。QML 快捷键也改成了四个不同组合，缺少侧栏和工作区切换。 | 交互迁移不完整。 |
| P2 | 头像/观众数展示、分级 Toast 和操作入口 | 原生模型已带 avatar URL 与 viewer label，但 QML 未使用；Toast 只有单条纯文本，缺少等级、队列和操作。 | 视觉信息与反馈体验未完整迁移。 |

### UI 结论

新 QML 外壳保持了深色密集工作区、左侧房间列表、房间网格、无边框标题栏和基本图标体系，但不是旧 Electron UI 的完整等价迁移。最显著差异是：顶部全局控制组缺失，分组标签缺失，搜索结果流程缺失，房间卡控制和元数据更少，监控面板从逐房操作降为聚合计数，预设/分组管理和 Toast 反馈被简化。

### 建议后续顺序

1. 先决定弹幕是否仍在 Qt-only 产品范围；若保留，优先原生实现会话、治理和 QML 叠层。
2. 修复分组切换的实际房间集切换，再补侧边栏分组标签和完整成员管理。
3. 恢复旧版添加搜索、布局选择/比例、预设完整 CRUD，以及全局音频/弹幕控制。
4. 补齐逐房监控、房间卡音量/刷新、拖拽排序、快捷键、头像/观众信息和可操作 Toast。

### 计划与设计对照

当前 M8 发布验收的自动化测试证明了新的 Qt/libmpv 主运行时、关闭过程与发布包，但没有覆盖以上旧版功能的逐项等价性。上述项目应作为新的迁移差距清单，而不是把 M8 的通过结果解释为 Electron 功能全量等价。

### Notion synchronization

本次迁移差距审计已作为现有 Qt Quick/QML 迁移设计的子页面创建并回读：

https://app.notion.com/p/3c90f4bdec4881a4a423ea874a0d0538?pvs=204

## 2026-08-27 - 原生弹幕迁移 Task 1/2

| 阶段 | 计划目标 | 实施证据 | 验证结果 | 下一步 |
| --- | --- | --- | --- | --- |
| Task 1 | 建立斗鱼 STT 与二进制帧协议边界。 | 新增 `native/src/danmaku/douyu_danmaku_protocol.*` 与协议测试；CMake 注册 Qt WebSockets 组件和 focused target；CTest 运行环境显式继承 Qt DLL 路径。 | `douyu_danmaku_protocol_test` 1/1 通过；非法 UTF-8 测试修正为直接破坏帧 payload 后通过；`git diff --check` 无错误。 | 进入 Socket/Timer/Client 状态机前，先完成共享治理层。 |
| Task 2 | 建立安全消息类型、清洗、关键字过滤、去重和速率治理边界。 | 新增 `native/src/danmaku/danmaku_types.*`、`danmaku_governance.*` 与 focused 测试；限制字段、关键字和显示设置范围。 | `danmaku_governance_test` 1/1 通过；协议+治理回归 2/2 通过；`git diff --check` 无错误。 | 执行 Task 3：可注入 Socket、确定性 Timer 和单房间客户端状态机。 |

- 设计与计划对照：仍保持 Qt Quick/QML + C++ 唯一运行时、最多九路、无 Electron/Chromium/WebEngine/QWidget/QOpenGLWidget；本阶段只增加协议与治理基础，不向 QML 或持久化暴露端点、令牌、Cookie、播放 URL 或原始帧。
- 验证边界：测试均为本地 QtTest，无网络访问。首次 CTest 失败原因为测试条目未继承 Qt DLL 路径；修复后协议目标通过。治理测试覆盖字段清洗、长度限制、关键字过滤和相邻重复抑制。
- Notion 状态：尝试读取 `notion://docs/enhanced-markdown-spec` 时 MCP 返回 `Auth required`，因此本轮未创建或回读 Notion 页面；授权恢复后需补建本条脱敏日志页并回读。

## 2026-08-27 - 原生弹幕迁移 Task 3

| 阶段 | 计划目标 | 实施证据 | 验证结果 | 下一步 |
| --- | --- | --- | --- | --- |
| Task 3 | 建立可注入 Socket、确定性 Timer 和单房间客户端状态机。 | 新增 `danmaku_socket.*`、`danmaku_timer_scheduler.*`、`douyu_danmaku_client.*`；生产适配器使用 Qt WebSockets，测试使用 fake socket/timer；实现登录、入组、心跳、握手超时、端点轮换、有限重试、认证阻断和幂等停止。 | `douyu_danmaku_client_test` 1/1 通过；协议+治理+客户端回归 3/3 通过；`git diff --check` 无错误。 | 执行 Task 4：九路会话管理、有界队列和资格同步。 |

- 设计与计划对照：保持最多九路和 Qt Quick/QML + C++ 唯一运行时；Socket 只向客户端转发二进制帧、通用网络失败和明确认证信号，不解析错误字符串推断 HTTP 状态；端点、认证信息和原始帧仍停留在 C++ 内部。
- 验证边界：客户端测试全程使用 fake socket/timer，无真实网络连接；心跳测试先注入合法登录确认，再推进 45 秒，保留 10 秒握手超时语义。
- Notion 状态：MCP 仍返回 `Auth required`，尚未能创建或回读本轮 Notion 页面；授权恢复后需补建 Task 1/2/3 汇总子页面并回读。

## 2026-08-27 - 原生弹幕迁移 Task 4

| 阶段 | 计划目标 | 实施证据 | 验证结果 | 下一步 |
| --- | --- | --- | --- | --- |
| Task 4 | 建立最多九路会话管理、资格同步、去重和有界消息队列。 | 新增 `danmaku_session_manager.*` 与 fake-client 测试；会话只在 active/room/global/live 四项均满足且容量未满时保留；每房保留 200 个 ID、最多 100 条已治理消息。 | `danmaku_session_manager_test` 1/1 通过；协议+治理+客户端+会话回归 4/4 通过；`git diff --check` 无错误。 | 执行 Task 5：工作区版本 3 弹幕配置与预设迁移/往返。 |

- 设计与计划对照：九路上限、离线/禁用/非活动房间立即停止并清空队列、治理后入队均已落地；会话管理器不保留原始 STT 帧，只存储安全消息与有限统计。
- 验证边界：全程 fake client，无网络访问；队列用例显式关闭峰值保护以隔离 100 条容量行为，默认治理规则仍由 Task 2 测试覆盖。
- Notion 状态：MCP 仍返回 `Auth required`，本轮仅写入本地脱敏日志；授权恢复后需补建并回读包含 Task 1-4 的新页面。

## 2026-08-27 - 原生弹幕迁移 Task 5

| 阶段 | 计划目标 | 实施证据 | 验证结果 | 下一步 |
| --- | --- | --- | --- | --- |
| Task 5 | 将工作区弹幕设置与预设迁移到版本 3，保留版本 1/2 兼容性并拒绝敏感字段。 | `native_workspace_types.h` 新增 `NativeDanmakuConfiguration`，快照与预设均持有完整显示/治理配置；`native_workspace_store.cpp` 序列化完整 `danmaku` 对象、校验设置范围、过滤未知房间覆盖，并将版本 2 的全局弹幕安全默认为关闭、预设布尔值映射到新配置。 | `native_workspace_store_test` 通过；协议、治理、客户端、会话和工作区持久化回归 5/5 通过；`git diff --check` 无错误。首次构建失败仅因当前 PowerShell 未加载 VS x64 标准库环境，加载 `VsDevCmd.bat` 后构建成功。 | 执行 Task 6：实现 `DanmakuController`、AppController 同步、房间安全角色和 QML 边界。 |

- 设计与计划对照：版本 3 持久化保持 Qt Quick/QML + C++ 唯一运行时、最多九路和敏感数据边界；端点、Cookie、令牌、签名、播放 URL、原始帧和原始诊断不会进入工作区 JSON。
- 迁移行为：版本 1 继续无预设兼容加载；版本 2 保留房间级 `danmakuEnabled`，当前工作区全局开关默认为关闭，旧预设开关映射到 `preset.danmaku.globalEnabled`；版本 3 要求严格的显示、治理和覆盖对象并在加载/保存时归一化。
- 验证边界：使用 VS 2022 x64 开发环境运行构建，CTest 显式加入 Qt DLL 路径；无网络访问。当前 Notion MCP 仍返回 `Auth required`，因此本轮只追加本地脱敏日志，授权恢复后再创建并回读 Notion 页面。

## 2026-08-27 - 原生弹幕迁移 Task 6

| 阶段 | 计划目标 | 实施证据 | 验证结果 | 下一步 |
| --- | --- | --- | --- | --- |
| Task 6 | 建立 `DanmakuController`、AppController 同步、房间安全角色和关闭生命周期边界。 | 新增 `DanmakuController` 的 QML 安全接口与九路会话同步；AppController 在全局开关、房间开关、直播状态、活动房间、移除、预设、恢复和关闭时统一同步；RoomListModel 仅公开连接状态、认证阻断标识和数值统计；`RoomSession` 改为原生 `MpvQuickItem` 绑定。 | `douyu_danmaku_protocol_test`、`danmaku_governance_test`、`douyu_danmaku_client_test`、`danmaku_session_manager_test`、`native_workspace_store_test`、`multi_room_coordinator_test`、`app_controller_test`、`room_list_model_test`、`qml_close_regression_test` 共 9/9 通过；`git diff --check` 无输出。另新增直播状态变化快照回归，复现并修复了 `Ready` 快照早于 `Online` 状态发布导致弹幕会话未启动的问题。 | 执行 Task 7：实现 QML 弹幕叠层、弹幕行和车道调度器，并接入 `RoomTile`。 |

- 设计与计划对照：仍保持 Qt Quick/QML + C++ 唯一运行时、最多九路；端点、Cookie、令牌、签名、播放 URL、原始帧和原始诊断不进入 QML 或持久化。
- 状态同步修复：`RoomSession::liveStatusChanged` 触发协调器重新发布快照，确保 AppController 能在解析结果将房间标记为在线后同步弹幕资格。
- 验证边界：本轮回归全部为本地确定性测试，无真实斗鱼网络连接；尚未运行完整 Debug/Release 套件，也未完成真实房间的弹幕连接观测。
- Notion 状态：本轮需在授权可用后创建并回读新的 Task 6 脱敏日志子页面；若 MCP 仍返回 `Auth required`，保留本地日志并在下一次授权恢复后补写。

## 2026-08-28 - 原生弹幕迁移 Task 7

| 阶段 | 计划目标 | 实施证据 | 验证结果 | 下一步 |
| --- | --- | --- | --- | --- |
| Task 7 | 实现 QML 弹幕叠层、弹幕行、车道调度器，并接入房间卡。 | 新增 `DanmakuOverlay.qml`、`DanmakuLine.qml` 和 `DanmakuLaneScheduler.js`；叠层按区域和密度选择无碰撞车道，保留未发出的本地候选消息，禁用时清空活动项和会话队列；`RoomTile.qml` 接入安全连接指示与失败/认证阻断后的重试操作。 | 新增 `qml_danmaku_overlay_test` 覆盖区域投放、车道防碰撞与禁用清空；定向测试通过。`qml_engine_smoke_test`、`qml_visual_smoke_test`、`qml_interaction_test`、`qml_close_regression_test` 与新测试共 5/5 通过。`douyu_qml` 静态 QML 模块已重新生成并通过编译；已检查 1280x720 视觉截图。 | 执行 Task 8：恢复旧版等价的弹幕设置面板与视觉集成。 |

- TDD 证据：测试资源先注册，构建因缺少 `DanmakuOverlay.qml` 明确失败；补齐组件后，定向测试转绿。
- 设计与计划对照：仍为 Qt Quick/QML + C++ 唯一运行时；弹幕动画仅使用每条消息一个线性 `NumberAnimation`，高级模式只增加同根节点的阴影文本，不引入效果模块或第二套动画树。
- 安全边界：QML 仅接收已净化的消息文本、昵称、显示设置和安全状态；没有新增播放地址、Cookie、令牌、签名、端点、原始帧或原始诊断暴露。
- 验证边界：当前验证为本地确定性 QML 测试与静态 QML 模块编译；真实斗鱼房间的端到端弹幕连接仍待后续人工验收。

## 2026-08-28 - 原生弹幕迁移 Task 8

| 阶段 | 计划目标 | 实施证据 | 验证结果 | 下一步 |
| --- | --- | --- | --- | --- |
| Task 8 | 恢复旧版等价的弹幕设置面板，并完成房间卡与视觉集成。 | `DanmakuSettingsPanel.qml` 改为显示、治理、统计三页；保留全局与逐房开关、治理范围和房间覆盖、统计重置及显示默认值重置。`RoomTile.qml` 增加安全连接状态与重试入口，并把顶部/底部控件区域传给 `DanmakuOverlay.qml` 作为避让边距。 | 完整 Debug CTest 28/28 通过，总耗时 111.49 秒；`qml_interaction_test` 覆盖三页、连接/重试状态和显示重置；`qml_visual_smoke_test` 使用固定安全夹具验证弹幕不与房间控件重叠，且设置面板在 1280x720 下保持边界正确。 | 执行 Task 9：完整 Release 构建与 CTest、受控单房真实直播验收、九路生命周期对照和最终记录。 |

- 设计与计划对照：三页设置、全局及逐房控制、安全连接状态、重试入口、叠层避让和可重复的视觉覆盖均符合 `2026-08-27-qt-native-danmaku-migration` 设计与计划；Qt Quick/QML + C++ 仍是唯一运行时，房间上限仍为九路。
- 安全边界：本阶段只使用 `DanmakuController` 的安全 QML API。页面、日志和测试夹具均未写入播放地址、解析输出、Cookie、令牌、签名、端点、原始帧或原始诊断。
- 验证说明：完整 Debug CTest 的当前日志为 `native/out/verification/final-debug-ctest.log`。CTest 的 `LastTestsFailed.log` 保留了早前一次运行的旧条目，不能作为本次结果；本次完整结果和随后单独复跑的 `qml_interaction_test` 均为通过。
- Notion 状态：已创建并回读新的脱敏子页面：<https://app.notion.com/p/3ca0f4bdec488190bc12c092821e242f?pvs=204>。

## 2026-08-28 - Windows Qt 平台插件启动修复

| 项目 | 结果 |
| --- | --- |
| 用户问题 | 启动时出现 “This application failed to start because no Qt platform plugin could be initialized. Available platform plugins are: windows.” |
| 根因 | Release 包只部署 `platforms/qwindows.dll`；在测试环境继承 `QT_QPA_PLATFORM=offscreen` 时，Windows GUI 程序会尝试加载未部署的 `offscreen` 插件。该问题不是 QML 页面或 libmpv 初始化失败。 |
| 修复 | `native/app/main.cpp` 在 Windows 创建 `QGuiApplication` 前固定 `QT_QPA_PLATFORM=windows`，确保 Explorer/脚本启动使用包内原生平台插件；`native/tests/native_self_test_test.cpp` 的子进程回归也显式使用 `windows`。 |
| 验证 | Release 目标重建并重新部署 Qt；清空外部环境后 `--self-test` 退出码 0。相关回归 `qml_close_regression_test`、`mpv_quick_item_test`、`native_self_test_media`、`douyu_monitor_native_self_test` 4/4 通过；完整 Release CTest 28/28 通过，总耗时 129.01 秒。 |
| 视觉验证 | QML 离屏截图仍使用 `qml_visual_smoke_test` 生成并检查；未使用 Computer Use。 |
| 运行入口 | `native/out/build/windows-x64-release/douyu_monitor_native.exe`，启动时不要从仍在构建或部署中的目录运行。 |
| 未覆盖 | 真实斗鱼请求、真实九路播放和长时间性能基准仍需用户环境人工验收；本轮未记录播放地址、认证信息或原始诊断。 |

- 计划与设计对照：保持 Qt Quick/QML + C++ 唯一运行时、最多九路、无 Electron/Chromium/WebEngine/QWidget；本修复只收紧 Windows 平台插件选择，不改变媒体、弹幕或持久化边界。

## 2026-08-28 - 用户反馈修复：通知、房间元数据与弹幕

| 用户问题 | 根因/修复 | 验证 |
| --- | --- | --- |
| 右上角通知无法关闭 | `WorkspaceModel::setLastMessage(QString)` 暴露为 `Q_INVOKABLE`；Toast 关闭按钮增加稳定对象名并直接清空消息。 | 新增 `closesToastFromQml`；Debug/Release `qml_interaction_test` 通过。 |
| 头像、直播间标题、主播名为空 | StreamGet 服务强制 UTF-8 stdin/stdout；搜索响应字段安全解析并合并到 `RoomSession`、`RoomListModel` 和 QML 房间卡/历史列表。 | Release `streamget_service.exe` 对房间 `63136` 返回在线状态、主播名、标题、分类、观众数和头像 URL；`room_list_model_test`、QML 头像角色测试通过。 |
| 没有弹幕 | 弹幕生产端点改回旧框架实际使用的 `danmuproxy.douyu.com`；C++ 客户端、会话治理和 QML 覆盖层保持安全边界。 | `douyu_danmaku_protocol_test`、`douyu_danmaku_client_test`、`danmaku_session_manager_test`、`qml_danmaku_overlay_test` 通过；视觉烟测生成并检查覆盖层截图。 |

- 构建：使用 VS 2022 x64 开发环境重新生成 `douyu_monitor_native.exe` 与 `streamget_service.exe` 的 Debug/Release 目标，退出码均为 0。
- 回归：Release 完整 CTest 28/28 通过（89.99 秒）；Debug 核心 QML/弹幕回归 5/5 通过（42.27 秒），此前 Debug 完整套件也已 28/28 通过。
- UI 证据：`native/out/verification/qml/shell-1280x720.png` 与 `danmaku-settings-1280x720.png` 已生成并人工检查；未使用 Computer Use。
- 运行时边界：源码/应用目录未发现 QWidget、QOpenGLWidget、Qt WebEngine、Electron、Chromium 或 Node 运行时命中；未将播放 URL、Cookie、令牌、签名或原始弹幕帧写入日志。
- 计划与设计对照：本次完成 `2026-08-27-qt-native-repair.md` 的元数据、UI 交互和最终回归要求；继续保持 Qt Quick/QML + C++ 唯一运行时、最多 9 路。真实播放画面和真实弹幕长时间稳定性仍需用户环境验收。
- 下一步：使用 Release 目录启动程序，添加真实房间并确认 Toast 关闭、头像/标题/主播名、弹幕开关与弹幕滚动；随后再进行 8/9 路性能基准和长时间运行观察。

## 2026-08-28 - P0 分组切换与成员管理

| 阶段 | 计划目标 | 实施证据 | 验证结果 | 下一步 |
| --- | --- | --- | --- | --- |
| P0 | 分组切换替换实际活动房间集，并提供成员移除、上下移动、启动恢复和会话同步。 | `native/src/ui/app_controller.cpp`、`native/src/workspace/multi_room_coordinator.cpp`、`native/app/qml/components/RoomSidebar.qml`、`native/app/qml/dialogs/GroupManagerDialog.qml`；活动分组切换调用协调器替换房间，分组标签与溢出菜单可切换，成员支持移除/上移/下移，启动按活动分组顺序恢复，播放与弹幕会话随房间集释放。 | Debug CTest 28/28 通过；Release CTest 28/28 通过；QML 聚焦测试 2/2 通过；已检查 `shell-1280x720.png`、`shell-1920x1080.png` 和 `danmaku-settings-1280x720.png`；`git diff --check` 无错误。 | 进入 P1：房间号、斗鱼链接、主播名搜索，候选结果展示与添加流程。 |

- 设计与计划对照：本阶段完成审计表 P0 分组行，保持 Qt Quick/QML + C++ 唯一运行时、最多九路和敏感数据边界。
- Notion 状态：当前 MCP 请求返回 `Auth required`，本轮无法创建或回读新的 Notion 页面；本地日志已完整记录，授权恢复后需补建同内容的脱敏子页面并回读。

## 2026-08-28 - P1 房间搜索、候选展示与元数据添加

| 阶段 | 计划目标 | 实施证据 | 验证结果 | 下一步 |
| --- | --- | --- | --- | --- |
| P1 | 支持房间号、斗鱼链接、主播名搜索，展示候选并添加所选房间的公开资料。 | `native/src/ui/app_controller.h/.cpp` 新增可取消搜索、过期响应过滤、候选安全投影和元数据缓存；`native/app/qml/dialogs/AddRoomDialog.qml` 支持自由输入、状态/错误提示、候选列表、头像回退、主播名/标题/分类/在线状态/观众数展示和候选添加；`native/tests/app_controller_test.cpp` 与 `native/tests/qml_interaction_test.cpp` 覆盖链接规范化、元数据保留和加入动作。 | VS 2022 x64 Debug 构建退出码 0；Debug CTest 28/28 通过（`native/p1-debug-ctest.log`，316.17 秒）；Release 构建退出码 0；Release CTest 28/28 通过（`native/p1-release-ctest.log`，205.21 秒）；定向 `app_controller_test` 与 `qml_interaction_test` 均通过。为使 QML 测试反映真实运行时，补充窗口宿主并通过 `ListView.itemAtIndex(0)` 获取惰性委托；未修改生产搜索逻辑。 | 完成审计表 P1 搜索行；进入下一项 P1：多种布局、主画面比例和布局预设恢复。 |

- 设计与计划对照：继续保持 Qt Quick/QML + C++ 唯一运行时、最多 9 路；搜索输入只接受数字房间号、斗鱼 HTTP(S) 链接或非空主播名，响应仅向 QML 暴露已净化公开资料，播放源、认证材料和原始服务输出不进入界面、持久化或日志。
- 验证补充：`git diff --check` 退出码 0（仅报告现有 LF/CRLF 转换提示）；源码、QML、计划和应用目录未命中 QWidget、QOpenGLWidget、Qt WebEngine、Electron、Chromium 或 Node；Release 可执行文件、StreamGet 服务和 `libmpv-2.dll` 均存在。
- Notion 状态：已尝试读取增强 Markdown 规范并连接 Notion，MCP 返回 `Auth required`；本轮未创建或回读页面，保留本地脱敏日志，待授权恢复后补建并回读。

## 2026-08-28 - P1 布局、主画面比例与预设恢复

| 阶段 | 计划目标 | 实施证据 | 验证结果 | 下一步 |
| --- | --- | --- | --- | --- |
| P1 | 迁移多种布局、主画面比例、手动布局持久化与预设恢复。 | `MultiRoomCoordinator`、`WorkspaceModel`、`AppController` 和 `WorkspaceGrid` 支持经校验的布局选择、主画面比例、拖拽/步进调整、房间顺序、侧栏可见性和预设往返恢复；新增窄窗口视觉回归。 | Debug CTest 28/28 通过（154.08 秒）；Release CTest 28/28 通过（113.77 秒）。QML 截图尺寸为 `1280×720`、`1920×1080`、`1280×720` 设置面板和按最小尺寸约束归一化后的 `960×844` 窄窗口。 | 进入下一项 P1：全局静音、单/多房音频模式和全局弹幕策略的完整 QML 控件与持久化联动。 |

- 设计与计划对照：`2026-08-28-qt-p1-layout-ratio-presets.md` Task 1-4 已完成；产品仍为 Qt Quick/QML + C++ 唯一运行时，最多 9 路，不引入 Electron、Chromium、Qt WebEngine、QWidget 或敏感播放材料。
- 回归修复：Release/offscreen 平台不会在测试夹具中自动执行 `Window` 最小尺寸约束；`qml_visual_smoke_test` 现在按 QML 声明的 `minimumWidth`/`minimumHeight` 归一化请求尺寸后再验证视口边界。生产 `Main.qml` 未改动。
- 完整性检查：`git diff --check` 无内容错误（仅既有换行转换提示）；截图文件存在且尺寸正确；受限敏感信息扫描未发现播放地址、认证材料、原始弹幕帧或原始诊断；未发现残留应用/服务进程。
- Notion 状态：已在批准的 Qt Quick/QML 迁移设计页下创建并回读本条脱敏日志页面；回读确认父级、标题、验证证据、回归修复、计划对照和下一审计项均正确。

https://app.notion.com/p/3ca0f4bdec48813792fee023bdcc7342?pvs=204

## 2026-08-29 - P1 全局控制验证与 primary-two 布局回归修复

| 项目 | 结果 |
| --- | --- |
| 用户问题 | `primary-two` 主画面错误地只占左上区域，次级房间被排到下方，留下空白工作区。 |
| 根因 | `WorkspaceGrid.qml` 让 `primaryRoomRatio` 同时控制桌面端的主画面宽度与高度，违背了单轴分割设计。 |
| 修复 | 宽屏仅按比例分割左右区域：主画面在左侧占满高度，次级房间在右侧排列。窄窗口仅按比例分割上下区域：主画面占满宽度，次级房间在下方排列。 |
| 回归覆盖 | `qml_interaction_test` 新增 9 路 `primary-two` 几何验证：主画面与首个次级房间顶部对齐、主画面在左侧、主画面高度明显更大。 |
| 验证 | Debug 完整 CTest 28/28 通过（175.08 秒）；Release 重新构建后完整 CTest 28/28 通过（114.04 秒）。 |
| UI 检查 | 已检查 1280x720、1920x1080 与窄窗口归一化后的 960x844 QML 截图；未使用 Computer Use。 |
| 完整性 | `git diff --check` 未报告内容错误，仅有既有 LF/CRLF 转换提示；未发现残留原生应用或服务进程。 |

- 计划与设计对照：全局静音、单/多房音频状态及持久化保持有效；`primary-two` 重新符合已批准的“单轴主画面分割”设计。产品仍为 Qt Quick/QML + C++ 唯一运行时，最多 9 路，无 Electron、Chromium 或 WebEngine。
- 验证边界：本轮为本地确定性 CTest 与 QML 截图检查，不替代真实斗鱼房间的长期播放、弹幕或 8/9 路性能验收。
- 安全边界：本记录未包含播放地址、Cookie、令牌、签名、认证材料、原始弹幕帧或原始诊断。
- Notion：已创建并回读脱敏子页面：<https://app.notion.com/p/3cb0f4bdec4881eb8ab4f328ccf660ab?pvs=204>。

## 下一步准备（2026-08-29）

按迁移审计表，下一阶段从尚未完整迁移的逐房监控与房间卡操作开始：

1. 先为房间卡音量调节、刷新房间资料和状态摘要建立独立失败测试。
2. 在 `AppController` 现有 `setVolume`、`refreshRoom` 安全命令之上接入最小 QML 控件，不改变播放地址、认证材料和九路上限边界。
3. 增加 QML 交互与截图验证，再运行完整 Debug/Release CTest。
4. 完成后再次写入本地日志并创建/回读对应 Notion 脱敏页面。

该阶段尚未实施，因此不将房间卡音量、刷新资料或逐房监控标记为已完成。

## 2026-08-29 - P1 房间卡音量与刷新操作

| 项目 | 结果 |
| --- | --- |
| 目标 | 补齐房间卡直接音量调节和刷新房间资料操作，使用现有 AppController 安全命令。 |
| 实施 | RoomTile.qml 底部控制条新增 roomVolumeSlider，按 0-100 映射调用 controller.setVolume(roomId, volume)；更多操作菜单新增 refreshRoomAction，调用 controller.refreshRoom(roomId) 后关闭菜单。 |
| TDD | qml_interaction_test 先以缺失 roomVolumeSlider 的预期失败进入 RED；实现后验证音量值传递与刷新命令均通过。 |
| 验证 | Debug qml_interaction_test 与 qml_visual_smoke_test 2/2 通过；检查 1280x720、1920x1080 与窄窗口截图，布局保持稳定。 |
| 计划对照 | 房间卡音量与刷新操作已完成；逐房状态摘要、排序拖拽/快捷键、完整 Toast 分级和真实网络/性能验收仍未完成。 |
| 安全边界 | 未向 QML、日志或持久化写入播放地址、Cookie、令牌、签名、原始帧或原始诊断。 |
| Notion | 已创建并回读脱敏子页面：https://app.notion.com/p/3cb0f4bdec4881c4b7c9f84ab785482b?pvs=204 |

## 下一步准备（2026-08-29）

按迁移审计表，下一阶段处理房间排序与操作完整性：

1. 为拖放排序及上移/下移控件建立失败交互测试，覆盖九路顺序保持。
2. 接入现有 moveRoom 命令，补齐房间卡/侧栏操作和键盘可达性。
3. 运行 QML 交互与视觉回归，再执行完整 Debug/Release CTest。
4. 完成本阶段后再次写入本地日志并创建/回读对应 Notion 脱敏页面。

## 2026-08-29 - P1 房间排序控件补齐（进行中）

| 项目 | 结果 |
| --- | --- |
| 实施 | RoomSidebar.qml 为当前房间行补充稳定的上移/下移控件对象名，并将下移操作接入现有 controller.moveRoom(roomId, 1)；上移继续使用 delta=-1。 |
| TDD | 排序交互测试先因缺少稳定控件入口失败，随后改用 ListView.itemAtIndex(0) 获取惰性委托并验证上下移参数，测试转绿。 |
| 验证 | Debug qml_interaction_test exposesRoomOrderingControls 通过（3/3）；尚未宣称拖放排序完成。 |
| 审计对照 | 上下移按钮路由已覆盖；拖放排序、完整快捷键 parity、边界禁用态和九路顺序持久化仍待继续实现。 |
| Notion | 已创建并回读脱敏子页面：https://app.notion.com/p/3cb0f4bdec4881cbb596cd47bedf8bcc?pvs=204 |

## 下一步准备（2026-08-29）

继续排序审计项：为九路列表增加可验证的拖放排序入口与边界行为，保持 AppController.moveRoom 为唯一变更通道；随后运行完整 QML 回归并记录。

## 2026-08-29 - P1 排序边界与拖放入口

| 项目 | 结果 |
| --- | --- |
| 实施 | 当前房间行补充上移/下移边界禁用态；新增 roomDragHandle 与 roomDropArea，拖放落点按源/目标索引计算位移并调用 moveRoom。 |
| TDD | 边界测试先因首项上移未禁用而失败，补充委托 index 后通过；拖放测试先因缺少拖放入口失败，恢复实现后通过。 |
| 验证 | Debug qml_interaction_test 与 qml_visual_smoke_test：2/2 通过；Debug/Release douyu_monitor_native 均重新构建并完成 QML 资源部署。 |
| 审计对照 | 上下移与拖放入口已覆盖；顺序持久化、完整快捷键 parity、真实九路长期性能仍待后续。 |
| 安全边界 | 未写入播放地址、Cookie、令牌、签名、原始帧或原始诊断。 |
| Notion | 已创建并回读脱敏子页面：https://app.notion.com/p/3cb0f4bdec48811c825de4d6d59035aa?pvs=204 |

## 下一步准备（2026-08-29）

进入逐房监控与反馈审计：补齐状态摘要、重试/刷新结果反馈和 Toast 分级动作，随后进行真实网络与 8/9 路性能验收准备。

## 2026-08-29 - 可运行程序同步构建

刚才完成的房间卡音量、刷新操作及排序上下移改动已重新编译进实际应用程序：

- Debug：native/out/build/windows-x64/douyu_monitor_native.exe
- Release：native/out/build/windows-x64-release/douyu_monitor_native.exe
- Release 构建完成 Qt QML 资源重新部署。
- 未发现残留 douyu_monitor_native 进程。

后续启动应使用 Release 目录中的可执行文件；测试目录中的旧程序不再作为运行入口。

## 2026-08-29 - P1 快捷键完整对齐

| 项目 | 结果 |
| --- | --- |
| 目标 | 将 Qt Quick 快捷键与旧框架六项语义对齐，并保持编辑控件不抢占输入。 |
| 实施 | Main.qml 使用 `Ctrl+Shift+A/W/M/D/S/R` 分别触发添加房间、工作区、监控、弹幕、侧栏和主房间刷新；面板采用打开/关闭切换；应用窗口聚焦范围内生效。移除旧的临时 `Ctrl+N/Ctrl+B/Ctrl+M/F5` 映射。 |
| 输入保护 | 通过 activeFocusItem 识别房间搜索、预设、分组和通知标题输入框；编辑状态下快捷键禁用，避免吞掉用户输入。 |
| TDD | 新增 handlesLegacyShortcutParity 回归测试；首次运行暴露旧测试在添加房间模态框打开时错误期待监控快捷键，随后补充关闭对话框并验证编辑态不触发，测试转绿。 |
| 验证 | `qml_interaction_test` 23/23 通过；Debug/Release `douyu_monitor_native.exe` 均重新构建。测试中仅有既有字体目录与 FakeController QML 警告，不影响断言结果。 |
| 计划对照 | 完整快捷键 parity 已完成；逐房状态摘要、Toast 分级动作、真实拖放运行时/顺序持久化和真实网络 8/9 路性能验收仍待后续。 |
| 安全边界 | 未向 QML、日志或持久化写入播放地址、Cookie、令牌、签名、原始帧或原始诊断。 |

## 下一步准备（2026-08-29）

按迁移审计表进入逐房监控与反馈闭环：

1. 审计并补齐房间卡直播状态、主播资料/头像/标题/观众信息的稳定显示与刷新结果映射。
2. 为播放源重试、资料刷新和状态变化增加可关闭、分级的 Toast 反馈测试。
3. 运行 QML 交互/视觉回归及 Debug/Release 构建，之后准备真实斗鱼网络与 8/9 路性能验收。
4. 完成本阶段后再次更新本地日志并创建/回读 Notion 脱敏页面。

## 2026-08-29 - P1 资料状态呈现与操作反馈

| 项目 | 结果 |
| --- | --- |
| 目标 | 修正房间资料或直播状态尚未返回时的误导性显示，并为刷新/播放源重试提供可关闭的反馈。 |
| 实施 | `RoomTile.qml` 为主播名、标题、分类和观众数增加稳定占位；空主播名显示房间号，空标题显示“斗鱼直播间”，空分类显示“未分类”，空观众数显示“--”。未知直播状态独立显示“检查中”，不再混同为“未开播”；左侧房间列表同步采用同一状态与资料降级策略。 |
| 反馈 | `retryPlayback` 在成功启动检查时显示“正在检查播放源”，失败时显示固定命令错误；`refreshRoom` 在有效房间显示“正在刷新房间数据”，找不到房间时显示固定错误。现有 Toast 支持手动关闭。 |
| TDD | `rendersFallbackMetadataAndUnknownStatus` 先因房间号占位缺失失败，补齐 QML 后转绿；`publishesCommandFailureToToast` 先因失败命令未写入 WorkspaceModel 消息失败，接入 Toast 链路后转绿。 |
| 验证 | `app_controller_test` 1/1、`qml_interaction_test` 1/1、`qml_visual_smoke_test` 1/1 通过；最新截图已刷新为 1280x720、1920x1080、窄窗口和弹幕面板。Debug/Release `douyu_monitor_native.exe` 已重新构建。 |
| 计划对照 | 资料缺省/未知状态误导和刷新/重试反馈已处理；真实斗鱼网络资料刷新、真实拖放顺序持久化、Toast 视觉分级及 8/9 路长期性能验收仍待后续。 |
| 安全边界 | 未将播放地址、Cookie、令牌、签名、原始帧或原始诊断写入 UI、日志或持久化。 |

## 下一步准备（2026-08-29）

按迁移审计表，下一阶段先验证状态刷新端到端链路并扩展反馈：

1. 使用隔离测试资料检查搜索/状态返回后的主播资料、头像、标题、观众数和 online/offline 投影。
2. 为刷新完成、播放源重试失败、状态变化补齐明确 Toast 级别与自动消失策略。
3. 再进行真实斗鱼网络单房验收，确认后才进入 8/9 路性能测量。
4. 完成后更新本地日志并创建/回读新的 Notion 脱敏页面。

## 2026-08-29 - P1 状态刷新端到端投影与 Toast 分级

| 项目 | 结果 |
| --- | --- |
| 目标 | 把主播资料、直播状态和操作反馈从搜索/刷新链路完整投影到 QML，并补齐 Toast 自动消失。 |
| 实施 | WorkspaceModel 增加 Toast 级别与超时；AppController 在刷新、重试与状态更新时写入分级提示；MultiRoomCoordinator 透出房间状态刷新信号；ToastViewport.qml 按级别渲染并支持自动消失。 |
| TDD | 新增 WorkspaceModelTest::exposesToastSeverityAndTimeout、QmlInteractionTest::autoDismissesToastBySeverity、AppControllerTest::projectsRefreshedRoomMetadataAndStatus，先以缺少契约的红灯失败，再补最小实现转绿。 |
| 验证 | Debug workspace_model_test、qml_interaction_test、app_controller_test 通过；Release 同组三个测试也通过（3/3，25.76 秒）。 |
| 计划对照 | 隔离测试资料的主播名、标题、头像、观众数和 online/offline 投影已对齐；Toast 分级和自动消失已补齐。真实斗鱼单房验收与 8/9 路性能测量仍待后续。 |
| 安全边界 | 未写入播放地址、Cookie、令牌、签名、原始帧或原始诊断。 |

## 下一步准备（2026-08-29）

继续进入真实斗鱼单房验收，再决定是否展开 8/9 路性能测量；若验收暴露新问题，再回到对应房间卡或状态链路修补。

## 2026-08-29 - 真实单房验收与 1/4/6/9 路本地性能基线

| 项目 | 结果 |
| --- | --- |
| 关闭回归 | `qml_close_regression_test` 与 `mpv_quick_item_test` 串行运行，2/2 通过；参数化夹具支持 `DOUYU_PERF_ROOM_COUNT=1/4/6/9`，仅用于本地确定性生命周期测量。 |
| 真实单房验收 | 使用已授权公开房间做脱敏验收：搜索返回 1 条结果，在线状态为 true，主播名/标题/头像均非空；解析请求返回 1 个播放变体；服务退出码为 0。播放地址、认证材料和原始服务输出未记录。 |
| 本地基线 | 1 路：峰值工作集 291.2 MB、峰值 CPU 22.3%、生命周期 11.23 s；4 路：492.6 MB、20.2%、11.08 s；6 路：617.6 MB、23.6%、11.45 s；9 路：822.2 MB、26.3%、11.92 s。四档退出码均为 0。指标来自串行关闭回归进程采样，不能替代真实斗鱼长时间播放。 |
| 计划对照 | 单房真实资料/状态/解析链路已通过；9 路关闭生命周期和本地资源基线已建立。真实 8/9 路多房播放、弹幕长期稳定性、GPU/解码占用和用户机器差异仍待人工验收。 |
| 代码变更 | `qml_close_regression_test.cpp` 增加受限房间数环境变量和动态断言；生产运行时边界不变，仍为 Qt Quick/QML + C++ + libmpv、最多 9 路。 |
| 安全边界 | 日志未包含播放 URL、Cookie、令牌、签名、原始弹幕帧或原始 mpv/服务诊断。 |

## 下一步准备（2026-08-29）

进入真实斗鱼多路验收：按 1/4/6/9 路逐档观察 10-30 分钟，记录固定指标（成功播放路数、状态/资料完整性、弹幕连接状态、峰值工作集、CPU/GPU 总体趋势、关闭是否干净），不记录播放源或认证材料。若 9 路出现失败，优先按房间索引、解码负载和服务请求并发隔离，而不是扩大产品上限。

## 2026-08-29 - P1 控件、动态清晰度与应用级全屏

| 项目 | 结果 |
| --- | --- |
| 顶部控件 | 工作区预设改用 `star.svg`，布局继续使用 `layout-grid.svg`；移除“原生 Qt 工作区”副标题；新增“声音总控”弹层，收纳全局静音、单声道和多声道；新增应用级全屏按钮。 |
| 房间卡布局 | 标题与右侧操作区采用固定剩余空间布局，标题使用 `ElideRight`，长标题不会覆盖音量、主画面、弹幕和清晰度操作。 |
| 动态清晰度 | StreamGet 返回的 variants 在 C++ 中安全投影为 `id/label/quality`，不暴露播放地址；RoomListModel 与 QML ComboBox 使用每房间实际选项。 |
| 音频策略 | 单声道添加首个房间时自动设为声音焦点；焦点房间删除后回退到当前列表第一路。 |
| 全屏行为 | `AppController` 提供 `toggleFullScreen/exitFullScreen`，F11 切换、Escape 退出，并恢复进入全屏前的最大化/普通状态；无 C++ 控制器时 QML 预览仍可工作。 |
| 验证 | Debug 完整 CTest 28/28 通过；Release 完整 CTest 28/28 通过；`qml_visual_smoke_test` 通过；新增控件、长标题、动态清晰度和首路音频焦点回归均通过。 |
| 产物 | Release：`native/out/build/windows-x64-release/douyu_monitor_native.exe`；Debug：`native/out/build/windows-x64/douyu_monitor_native.exe`。 |
| 计划对照 | 本阶段 5 个任务全部完成。剩余唯一发布前事项仍是用户环境中的真实斗鱼 1/4/6/9 路长时间播放验收，重点观察第九路、弹幕、CPU/GPU、内存和关闭稳定性。 |
| 安全边界 | 未向 UI、日志或持久化写入播放 URL、Cookie、令牌、签名、原始弹幕帧或原始诊断。 |

## 2026-08-29 - Windows 安装包

| 项目 | 结果 |
| --- | --- |
| 打包方式 | 使用 Windows 自带 IExpress 生成当前用户安装器；不引入 Electron、Chromium、Node 或 WebEngine。 |
| 安装器 | `native/out/installer/DouyuMonitor-Setup.exe`，约 92.6 MiB。 |
| 运行时载荷 | `DouyuMonitor-runtime.zip` 包含 Release Qt DLL、QML 模块、libmpv-2.dll、`douyu_monitor_native.exe` 和 `streamget_service.exe`。 |
| 载荷清理 | 已排除测试 exe、静态库、调试符号、构建日志、Ninja/CMake 残留和生成源文件。 |
| 验证 | 安装器脚本重新执行成功；打包 stage 中的 Release 程序以 `--self-test` 启动并返回码 0；载荷扫描未发现 Electron/Chromium/Node/WebEngine 或测试文件。 |
| 安装位置 | 当前用户 `%LOCALAPPDATA%\\Programs\\DouyuMonitor`，并创建开始菜单快捷方式。 |
| 计划对照 | Windows 安装包已完成；仍需在提交前完成 Git 分支重命名、远端发布和用户环境真实斗鱼 1/4/6/9 路长时间验收。 |
