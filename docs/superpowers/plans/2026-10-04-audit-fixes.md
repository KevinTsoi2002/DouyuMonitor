# Audit Fixes Implementation Plan

> Execute inline using executing-plans and test-driven-development. The user approved the design in conversation. No agent delegation, commits, installer generation or publication.

**Goal:** Repair the nine confirmed audit findings while retaining existing functionality and settings.

**Architecture:** Keep current AppController, resolver, client and JSONL boundaries. Add only explicit unknown-state projection, retryable persistence state and validated event mapping where necessary.

**Tech Stack:** Qt 6.8.3, C++20, QML, Python asyncio, CMake/Ninja/MSVC.

## Task 1: Reliability and security
- [x] Add failing post-handshake-close/deduplicated-retry tests in `native/tests/douyu_danmaku_client_test.cpp`; implement in `native/src/danmaku/douyu_danmaku_client.cpp`.
- [x] Add credential-format and previous-handler tests in `native/tests/application_logger_test.cpp`; redact in `native/src/app/application_logger.cpp`.
- [x] Add blocked synchronous cancellation regression in `native/service/tests/test_service.py`; retain occupancy in `native/service/streamget_service.py`.
- [x] Run the three relevant targets and Python suite; verify shutdown remains bounded.

## Task 2: Identity and display
- [x] Add ambiguous-plus-valid/order-independent tests in `native/tests/maozi_team_import_test.cpp`; protect candidates in planner.
- [x] Correct leader-first assertions and no-team cases in `native/tests/qml_interaction_test.cpp`; fix `GuildNavigationPanel.qml`.
- [x] Add exact hydration/broad merge/unknown-state tests in service backend/protocol and C++ protocol/controller tests; update AddRoomDialog presentation.
- [x] Run importer, protocol, backend and QML tests.

## Task 3: Freshness, persistence and event mapping
- [x] Add periodic lightweight status refresh with visible-only scheduling through AppController and resolver; test TTL and status-only calls.
- [x] Add controller unsaved state/retry command and persistent QML warning; exercise failed save followed by successful retry using temporary settings.
- [x] Add validated versioned local role/team mapping, existing defaults, Settings editor and reload application. Tests cover role changes, invalid mapping, identity aliases and persistence.
- [x] Fix Python-test interpreter selection so enabled tests cannot silently disappear; preserve explicitly selected interpreter.

## Verification
- [x] Clean rebuild both profiles if public headers change, using `D:\VSBuildTools\VC\Auxiliary\Build\vcvars64.bat`, Qt `D:\Qt\6.8.3\msvc2022_64`, MPV `native/sdk/mpv`.
- [x] Run `ctest --test-dir native/out/build/windows-x64-release --output-on-failure -j 1 --timeout 120` and same for beta24. Each profile reports 36/37 passed; the visual baseline target remains failing.
- [x] Run `python -m unittest discover -s native/service/tests -v` (35/35), installer script regression, both `--self-test` (exit 0).
- [x] Inspect screenshots, visual mismatch diffs, Git diff and `git diff --check`; report remaining native-DPI/live-install limits.
- [ ] Obtain approval for the intentional navigation/team-manager visual changes before refreshing the two old image baselines and rerunning visual regression. Keep this acceptance gate open.

## Verification Status

The nine code fixes and their behavioral regressions are implemented. This is not an all-green release acceptance: `qml_visual_regression_test` still differs from the old navigation and team-manager baselines. The inspected differences are 1.048% and 6.886%, respectively. No baseline replacement, version change, installer, commit or publication was performed.

Local evidence and remaining limits are recorded in `native/out/verification/audit-fixes-2026-10-04.md`. Remote tests use local fixtures; real Douyu playback, live Maozi authorization, multi-monitor DPI, installed-app behavior and sustained 16/24-stream performance remain outside this verification.
