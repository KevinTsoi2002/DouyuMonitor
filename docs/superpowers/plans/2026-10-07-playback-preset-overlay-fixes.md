# Playback, Preset And Overlay Fixes Implementation Plan

> **For agentic workers:** Execute inline using systematic debugging, TDD and verification-before-completion. No delegation, commit, version change or publication is authorized.

**Goal:** Refresh preset room presentation immediately, reliably hide inactive player controls, and reuse a freshly prefetched source at upstream EOF.

**Architecture:** Keep the current AppController/model boundary and one libmpv core per room. Update surviving model rows after batched insertion. Distinguish keyboard focus from active control interaction. A separate RemotePlaybackController fetches a replacement while the current source remains active; only natural remote EOF consumes that in-memory source. Existing bounded recovery remains the fallback.

**Tech Stack:** Qt 6.8, QML, C++20, libmpv, StreamGet JSONL service.

## Acceptance And Boundaries

- Applying presets updates existing rows, new rows and history without restarting.
- Mouse inactivity hides controls despite leftover focus. Open menus, quality popups and a pressed volume slider remain usable.
- Prefetch does not publish resolving/error state, stop the current player or change quality/audio.
- Stop, cancel, removal, quality changes and offline state discard prefetched addresses and pending requests.
- Sources and authentication remain in memory only.
- Upstream EOF cannot be prevented locally. Single-player handover may have a short decode/audio gap; zero-gap playback is not an acceptance claim.

## Task 1: Room Presentation

Files: `native/src/ui/room_list_model.cpp`, `native/src/ui/app_controller.cpp`, `native/tests/room_list_model_test.cpp`, `native/tests/app_controller_test.cpp`, `native/tests/qml_interaction_test.cpp`.

- [x] Add failing coverage for prefix changes during append and a secondary-primary-only change.
- [x] Add coverage for opening preset-only rooms in history.
- [x] Remove the append early return and compare secondary-primary state.
- [x] Stamp preset room history once and notify after the transaction.
- [x] Build and run model/controller/sidebar tests.

## Task 2: Overlay Inactivity

Files: `native/app/qml/components/RoomTile.qml`, `native/tests/qml_interaction_test.cpp`.

- [x] Reproduce a focused tile remaining visible after its inactivity timer.
- [x] Cover menu closure and quality-popup interaction; retain the existing pressed-slider guard.
- [x] Hide on inactivity regardless of residual focus, track hover on the whole tile, and restart on interaction completion.
- [x] Start the timer when a new tile is created, even if the pointer never enters.
- [x] Run interaction, engine smoke and visual smoke tests.

## Task 3: Fresh Source At EOF

Files: `native/src/workspace/room_session.*`, `native/src/ui/mpv_quick_item.*`, `native/tests/room_session_test.cpp`, `native/tests/mpv_quick_item_test.cpp`.

- [x] Add failing coverage for prefetch, preserved quality, failure isolation, cancellation and EOF handover.
- [x] Use an independent resolver and single-shot timer, scheduled when a remote source starts at approximately 240 seconds with deterministic room staggering. Playback progress also schedules after player reattachment.
- [x] Keep a successful replacement briefly in memory; retry failed prefetch only within bounds and never invalidate the active source.
- [x] Notify remote EOF/error before marking a failure; consume a valid prefetched source immediately, otherwise retain normal recovery.
- [x] Run session/player/lifecycle tests and actual repeated remote playback checks.

## Final Verification

- [x] Build stable16 and beta24 executables.
- [x] Run both CTest suites sequentially.
- [x] Run Windows QML close/preset tests and Release self-test.
- [x] Inspect diff for private data, unrelated changes and generated files.
- [x] Record evidence and gaps without changing visual baselines.

## Evidence So Far

- Model 10/10; session 29 passed with the opt-in network case skipped; player 25/25.
- Four overlay scenarios passed, including no initial pointer entry.
- Native preset sidebar identities, primary state and library history passed.
- The real Qt/OpenGL run for room 217331 lasted 629.563 seconds and passed two source rotations at elapsed 307.131 and 613.866 seconds. No delayed recovery failure was emitted; rendered pixels, progress and volume/mute were checked after handover.
- The first network run was invalidated by QtTest's default 300-second function timeout. A stdout-only rerun was stopped because this Windows GUI target did not emit an inspectable report; the completed rerun used a file and `QTEST_FUNCTION_TIMEOUT=720000`.
- Stable16 and beta24 final full suites each pass 36/37. The only failures are the pre-existing navigation/team visual baseline differences: guild-navigation 2142 pixels (1.048%) and team-manager 37679 pixels (6.886%). Baselines were not changed.
- Windows-native checks pass for both profiles: interaction 71/71, close/preset 7/7, player 25/25, and room session 29 passed with the opt-in live test skipped.
- Both profile executables pass `--self-test` with the Qt Quick renderer.
- Four overlay tests pass on both Windows and offscreen backends. Tests assert the product's 2200 ms default and then shorten their internal timer to 100 ms; production timing remains unchanged.
- An extra player run launched with `Start-Process -WindowStyle Hidden` failed its first window-exposure assertion. Running through the normal native command pipeline passed 25/25 on both profiles; the full CTest runs also passed this target. No rendering assertions were removed.
- Reports are local ignored files under `native/out/verification/`: `windows-x64-{release,beta24}-ctest-verified.txt`, `windows-x64-{release,beta24}-*-windows-native.txt`, and `windows-x64-{release,beta24}-self-test-native.txt`.
