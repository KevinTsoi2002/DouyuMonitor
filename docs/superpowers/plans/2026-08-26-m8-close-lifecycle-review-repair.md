# M8 Close Lifecycle Review Repair Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the reviewed libmpv teardown and stale-event gaps without changing the Qt Quick/QML product boundary.

**Architecture:** Keep the existing item-owned mpv core, but force Qt Quick scene-graph resource release from the item teardown path before terminating that core. The renderer clears its callback and frees `mpv_render_context` during the release handshake. Gate load-completion events on an active playback entry, then exercise the behavior with a direct local-media regression and a nine-renderer QML-engine shutdown regression.

**Tech Stack:** Qt 6.8 Quick/QML, `QQuickFramebufferObject`, libmpv render API, Qt Test, CMake/CTest.

---

### Task 1: Reproduce both review findings in tests

**Files:**
- Modify: `native/tests/mpv_quick_item_test.cpp`
- Modify: `native/tests/qml_close_regression_test.cpp`

- [x] **Step 1: Add a release-after-load regression.**

Create a visible OpenGL `MpvQuickItem`, begin a local-media load, immediately call `release()`, process queued events, and assert `Idle`, `!isMediaLoaded()`, and `!isFirstFrameRendered()`.

- [x] **Step 2: Run the focused item test and observe the stale-event assertion fail.**

Run: `cmake --build --preset windows-x64-debug --target mpv_quick_item_test; ctest --preset windows-x64-debug -R mpv_quick_item_test --output-on-failure -j 1`

Expected: the new release-after-load test fails because late `FILE_LOADED` or `VIDEO_RECONFIG` events can restore a released state.

- [x] **Step 3: Strengthen the nine-room shutdown test.**

Set the graphics API to OpenGL before creating the QML engine; wait for exactly nine `MpvQuickItem` instances and all nine render contexts to be ready. After closing the window, destroy the QML engine, drain GUI events, then assert that no player remains attached and the service is stopped.

- [x] **Step 4: Run the focused close test and observe its old teardown behavior is insufficient.**

Run: `cmake --build --preset windows-x64-debug --target qml_close_regression_test; ctest --preset windows-x64-debug -R qml_close_regression_test --output-on-failure -j 1`

Expected: it builds and exercises the real scene-graph renderer path; before the lifecycle fix it may expose an ordering failure or teardown instability.

### Task 2: Preserve core lifetime until renderer teardown

**Files:**
- Modify: `native/src/ui/mpv_quick_item.h`
- Modify: `native/src/ui/mpv_quick_item.cpp`

- [x] **Step 1: Keep the item-owned mpv core and isolate render-thread state.**

    The item remains the sole logical owner of `mpv_handle`, while a shared `MpvRenderState` carries the handle, render context, safe update callback target, and atomic media flags across the GUI/render-thread boundary.

- [x] **Step 2: Free the render context before terminating the item core.**

    `MpvQuickItem::~MpvQuickItem` retires callbacks and media work, then schedules a Qt Quick render job. The render thread clears the update callback and frees `mpv_render_context`; only after that does a queued GUI-thread action call `mpv_terminate_destroy`. Headless items take the direct GUI-thread core teardown path when no renderer is attached.

- [x] **Step 3: Run the item and shutdown regressions.**

Run: `cmake --build --preset windows-x64-debug --target mpv_quick_item_test qml_close_regression_test; ctest --preset windows-x64-debug -R "mpv_quick_item_test|qml_close_regression_test" --output-on-failure -j 1`

Observed: both focused regressions pass without raw playback information in output.

### Task 3: Reject stale events after release

**Files:**
- Modify: `native/src/ui/mpv_quick_item.cpp`

- [x] **Step 1: Accept load-completion events only for an active playback entry.**

Ignore `MPV_EVENT_FILE_LOADED` and `MPV_EVENT_VIDEO_RECONFIG` unless an active playlist entry is present and playback remains active. Continue to schedule a frame only for accepted events.

- [x] **Step 2: Re-run the release-after-load regression.**

Run: `ctest --preset windows-x64-debug -R mpv_quick_item_test --output-on-failure -j 1`

Expected: the item remains `Idle` with both public media flags false after queued events drain.

### Task 4: Re-run acceptance and record evidence

**Files:**
- Modify: `docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md`

- [x] **Step 1: Run full Debug and Release CTest.**

Run: `cmake --build --preset windows-x64-debug; ctest --preset windows-x64-debug --output-on-failure -j 1; cmake --build --preset windows-x64-release; ctest --preset windows-x64-release --output-on-failure -j 1`

Expected: all Debug and Release tests pass.

- [x] **Step 2: Run runtime and sensitive-output checks.**

Run: `cmake -DAPP_DIR=native/out/windows-x64-release -P native/cmake/verify_runtime_dependencies.cmake; rg -n -i "https?://[^ ]*(token|cookie|signature|wsauth)|mpv_error_string|MediaSource\\(" native/out docs/superpowers/logs --glob '!**/*.png'; git diff --check`

Expected: runtime verification succeeds, sensitive-output scan has no matches, and `git diff --check` exits successfully.

- [x] **Step 3: Append a sanitized local log and create/read back a Notion child page.**

Record the review findings, concrete repair, focused/full verification, design/plan alignment, and the next integration step. Keep the project log date at `2026-08-26` per project convention and omit playback sources, credentials, raw service payloads, and raw mpv diagnostics.
