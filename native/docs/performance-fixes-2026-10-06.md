# Performance Fix Verification: 2026-10-06

## Scope

Fix confirmed avoidable work on the GUI thread without changing room capacity,
persistence schema, transactional team import, background audio or render-context
teardown. The version remains 0.2.15. No installer or GitHub publication was requested.

## Implemented

- High-frequency volume, guild-cache and favorite-metadata saves are combined by
  a single-shot 250 ms timer. Updates do not restart it, preventing indefinite
  postponement. Structural commands, retry and shutdown still save immediately.
  A delayed save failure uses the existing unsaved-workspace warning.
- Guild cache/member and rank notifications are combined within one event-loop
  turn. The immutable bundled roster is parsed once and shared copy-on-write.
- Hiding the guild navigation stops its requests. Active work is requeued before
  cancellation, and late canceled responses are ignored when reopening.
- Danmaku rate windows use exact millisecond buckets and running counts, rather
  than copying and scanning a 60-second timestamp array per incoming message.
  Inclusive 3-second/1-second boundaries, keyword changes and clock rollback
  have regression coverage.
- Hidden danmaku presentation stops launching and clears on-screen objects,
  without clearing the upstream session queue. Continuous message arrivals do
  not restart an already-running launch timer and postpone display indefinitely.
- Playback resolution takes precedence over queued search/status work, with a
  bounded four-request priority burst. The two-in-flight limit and cancellation
  protocol are unchanged.
- libmpv uses `hwdec=auto-copy`; the core option is queried in a regression test.
  This is automatic copy-back decoding with mpv's software fallback, not proof
  that a particular real stream used the GPU.
- Navigation avatars are decoded at 64x64, hover avatars at 84x84, asynchronously.
  The per-snapshot danmaku synchronization informational log was removed.

## Red/Green Evidence

New regressions failed against the previous behavior:

- A 21-update volume burst had already persisted the final volume immediately.
- 58 pairs of guild notifications emitted 116 immediate roster notifications.
- Navigation reopening skipped the canceled metadata request.
- 10,000 messages at one timestamp produced 10,000 timestamp entries.
- Queued background checks ran before new playback resolution.
- The mpv hardware-decoding option was `no`.
- Navigation image `sourceSize` was unset.
- Hidden overlays launched messages; continuous arrivals postponed launching.
- Repeated bundled-roster calls reparsed and returned different backing storage.

These regressions pass after the fixes. The whole controller suite also covers
failed-save retry, shutdown flush, import rollback and cached navigation state.

## Fresh Verification

Both editions were rebuilt from a clean build:

```powershell
cmake --build out/build/windows-x64-release --clean-first --parallel 4
cmake --build out/build/windows-x64-beta24 --clean-first --parallel 4
ctest --preset windows-x64-release --output-on-failure
ctest --preset windows-x64-beta24 --output-on-failure
```

| Check | Result |
| --- | --- |
| Release build | Exit 0 |
| 24-room build | Exit 0 |
| Release CTest | 36/37 passed, 101.91 seconds |
| 24-room CTest | 36/37 passed, 106.29 seconds |
| Focused changed-path suites | 9/9 passed |
| Windows 16-room close regression | Passed |
| Windows 24-room close regression | Passed |
| Python service tests | 35 passed |
| Release self-test with local PPM | Exit 0: load, first frame, stop, release |
| 24-room self-test with local PPM | Exit 0: load, first frame, stop, release |
| QML structural/visual smoke | Passed in both editions |
| Git whitespace check | No whitespace errors |

The one failing suite in each edition is `qml_visual_regression_test`.
The failures remain confined to these existing baselines:

- `guild-navigation-284x720`: 2,142 pixels, 1.048%.
- `team-manager-720x760`: 37,679 pixels, 6.886%.

Baselines were not overwritten. Current 16/24-room captures were visually
inspected in addition to their containment and overlap assertions.

Build validation uncovered stale objects after header-layout changes:
the local cached MSVC dependency-output prefix is incorrectly encoded.
Clean builds eliminate stale objects for these delivered executables;
this does not claim that the local incremental-build dependency cache was repaired.

## Synthetic Performance Evidence

The previous and current probes use the same synthetic 60-second incoming load
and measure 1,000 governance calls at a fixed timestamp. The optimized probe
seeds time buckets and counters rather than per-message timestamp storage.

| Incoming messages/second | Previous total ms | Current total ms |
| --- | --- | --- |
| 100 | 10.3702 | 0.6582 |
| 1,000 | 112.478 | 0.3273 |
| 5,000 | 1,299.63 | 0.2731 |

This measures the algorithm, not application frame time or live-server traffic.
The active input window is four buckets in this fixture instead of up to
301,000 individual timestamps.

Single whole-workspace saves remain synchronous. In the synthetic 1,000-history
fixture, median save cost was 32.01 ms. This fix reduces high-frequency write
count; it does not move serialization/disk I/O to a worker or eliminate each
individual save stall.

## Remaining Validation Boundaries

- Actual GPU decoder selection and long-running 16/24 real-stream CPU/GPU/memory
  behavior have not been measured.
- No QML profiler trace or frame-time percentile evidence was produced.
- Render target-time waiting, synchronous multi-player initialization, large
  rank-snapshot parsing and broader snapshot signal fanout remain potential
  bottlenecks. They were not changed speculatively in this repair.
- Existing visual-baseline approval remains outstanding.
- No installation package, release tag, commit or remote upload was produced.

## Executables

- `D:/DouyuMonitor/native/out/build/windows-x64-release/douyu_monitor_native.exe`
- `D:/DouyuMonitor/native/out/build/windows-x64-beta24/douyu_monitor_native.exe`

Raw test/benchmark outputs are local-only under
`D:/DouyuMonitor/native/out/verification/performance-*`.
