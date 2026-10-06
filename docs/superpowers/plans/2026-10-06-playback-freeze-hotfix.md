# Playback Freeze Hotfix Implementation Plan

> Execute inline using systematic-debugging, TDD and verification-before-completion. Preserve version 0.2.16 and replace the stable/beta installer attachments only after verification.

**Goal:** Recover remote streams that end or stop progressing without requiring a manual playback-source refresh.

**Architecture:** MpvQuickItem monitors remote playback progress asynchronously and reports fixed-code failures. RoomSession owns a bounded, delayed source-refresh recovery, preserving quality/audio and cancellation. Do not change persisted data or expose media addresses.

**Tech Stack:** Qt 6.8.3, C++20, libmpv, MSVC/Ninja, Inno Setup, GitHub CLI.

## Evidence And Constraints

- Current END_FILE handling silently ends a live source on EOF without informing RoomSession.
- The player has no progress watchdog and RoomSession has no automatic recovery.
- Existing logs do not identify the specific reported freeze; do not claim a CDN or hardware-decoder cause without evidence.
- SDK client.h documents async property replies and EOF/STOP/REDIRECT reasons.
- Ignore local media, intentional pause, suspended rendering, retired sources and canceled sessions.
- Use monotonic elapsed time, a 30-second stall threshold and one async progress query in flight per player.
- Re-resolve after 3/6/12/24/48 seconds, with at most five consecutive recovery attempts; reset after observed playback progress or a manual resolve.

## Tasks

- [x] Add failing remote-EOF and watchdog-lifecycle regressions in `native/tests/mpv_quick_item_test.cpp`.
- [x] Add failing remote recovery, duplicate-failure and stop/offline cancellation regressions in `native/tests/room_session_test.cpp`.
- [x] Build and run the new tests against existing behavior; retain red evidence.
- [x] Implement async progress sampling, remote EOF failure and network timeout in `native/src/ui/mpv_quick_item.*`.
- [x] Add deterministic progress-deadline, stale-reply, pause/suspension and local/intentional-end tests.
- [x] Implement bounded recovery in `native/src/workspace/room_session.*`.
- [x] Run focused playback/session/coordinator/close tests, then clean-build both editions and run sequential full CTest (36/37 each; existing visual-baseline differences retained).
- [x] Generate both installers, validate staged media/service/payloads, preserve previous same-version installers and calculate fresh hashes.
- [ ] Commit/publish the hotfix source; document its exact commit without moving old release tags.
- [ ] Replace existing installer/checksum/manifest/verification attachments and update release notes. Verify remote sizes and digests.

## Verification

Use both `windows-x64-release` and `windows-x64-beta24`.
Run `mpv_quick_item_test`, `room_session_test`,
`multi_room_coordinator_test`, `remote_playback_controller_test`,
`qml_close_regression_test`, full CTest, Python service tests,
installer-script regression and stage `--self-test --media`.
Preserve old visual baselines and report any remaining baseline failures.
Live multi-hour target-environment acceptance remains separate from deterministic recovery tests.

Publication status and the exact source commit are recorded in the replacement
release attachment `verification.md`; the two publication items above describe
the remaining work at this plan's source-commit checkpoint.
