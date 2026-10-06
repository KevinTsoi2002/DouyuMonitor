# Performance Fixes Implementation Plan

> **For agentic workers:** Execute inline using systematic-debugging, TDD and verification-before-completion. No delegation, commits or remote mutations.

**Goal:** Remove confirmed avoidable UI stalls and rebuild both room-capacity editions.

**Architecture:** Keep existing domain boundaries and persistence format. Coalesce high-frequency workspace writes and roster notifications, pause hidden navigation work without losing pending tasks, and use exact rolling counters for danmaku. Preserve transactional team import, save failures, shutdown flushing and background audio.

**Tech Stack:** Qt 6.8.3, C++20, QML, libmpv, MSVC/Ninja.

## 1. Workspace And Navigation

- [x] Add controller regressions: a burst of volume updates must leave stored volume unchanged immediately, save latest volume within 1 s, and flush on shutdown; a guild cache/member notification pair must produce one roster notification.
- [x] Build/run the new tests and verify failures against the old behavior.
- [x] Add a single-shot 250 ms timer for volume, guild-cache and favorite-metadata saves. Start only if inactive to bound continuous traffic. Keep structural commands and retry immediate; immediate persistence cancels pending timer, and shutdown performs a final save.
- [x] Coalesce resolver/rank notifications with a zero-delay timer. Keep command-side synchronous notifications unchanged.
- [x] Add a resolver regression: stop cancels active metadata work, reopening retries that task and continues remaining work. Pause the resolver in `setNavigationVisible(false)`, retaining queues and active task kind.
- [x] Run `app_controller_test`, `guild_room_resolver_test`, persistence and QML interaction tests.

## 2. Danmaku

- [x] Add regressions for exact 3 s/1 s cutoff boundaries, peak limits, backward clock movement and bounded storage for repeated messages at one timestamp.
- [x] Run new tests and verify the old per-message timestamp storage fails the bounded-storage regression.
- [x] Replace timestamp-per-message arrays and full scans with time buckets and running counts. Prune from queue ends, preserving inclusive boundaries. Cache validated keyword settings until settings change.
- [x] Run governance/session/controller tests and repeat the synthetic throughput benchmark.
- [x] Reproduce hidden-page animation consumption and continuous-arrival timer starvation, then gate hidden presentation and avoid restarting an already-running launch timer. Preserve upstream queue/session behavior.

## 3. Request And Playback Pressure

- [x] Add a service test with two occupied slots: playback resolution must run before queued background status work, without changing two-worker capacity or cancellation.
- [x] Run it red, then prioritize queued `resolve` requests while retaining FIFO within each priority and a bounded high-priority burst.
- [x] Verify libmpv hardware-decoding configuration against official documentation; add a regression querying the player core's option before choosing a compatible automatic/fallback setting.
- [x] Bound avatar decode size; verify QML source size and async behavior through interaction tests.
- [x] Do not change render timing or player destruction without a reproducible test demonstrating the need and preserving frame scheduling.
- [x] Cache the immutable bundled roster; regression verifies shared storage and detached caller mutations. Remove the hot-path per-snapshot informational log.

## 4. Delivery

- [x] Build both `windows-x64-release` and `windows-x64-beta24`.
- [x] Run full CTest for both, QML close regression with the Windows backend, Python service tests and executable self-tests.
- [x] Inspect diff and capture benchmark/test results in `native/docs/performance-fixes-2026-10-06.md`. Preserve old visual baselines and report their pre-existing mismatches.
- [x] Supply executable paths in the verification report. No version bump, installer packaging or GitHub publication in this task.
