# Five-Minute Playback Interruption Investigation

Date: 2026-10-07. Source under investigation: commit
`0a86c92a3a236c6d622611b2ae16ff094e5ac8c8`, version 0.2.16.

## Finding

For room 217331, the upstream HTTP-FLV response ends after approximately
300 seconds. This also occurs outside the application and without the Quick
renderer or progress watchdog. libmpv then reports EOF, and the application
enters its delayed source-resolution recovery.

This establishes the immediate cause of the reproduced periodic interruption.
It does not establish the server's policy, signature implementation or the
behavior of every Douyu room.

## Evidence

- The user's local 2026-10-06 application log contains eight recovery entries
  for room 217331 and no progress-watchdog warnings. Consecutive entries within
  the same run are separated by 307.055 to 311.319 seconds.
- The current resolver response advertises `expire=300`. The parameter
  correlates with the observed lifetime; it is not proof of a particular
  authentication policy.
- An independent HTTP reader consumed 126,182,779 bytes from the alternate
  returned CDN, then received EOF at 300.26 seconds. It did not decode, render,
  save video data or invoke application recovery.
- The same shipped libmpv runtime with null video/audio outputs continued
  advancing playback time through 300 seconds. The alternate CDN returned
  `MPV_EVENT_END_FILE` at 303.99 seconds; the default CDN with browser-style
  User-Agent/Referer returned it at 307.26 seconds. Both had reason EOF (0)
  and error 0.
- A simultaneous second connection to the same default URL ended at 0.64
  seconds. This result is confounded by concurrent same-source connections
  and must not be attributed to missing headers.
- Python's default HTTPS probe failed a TLS handshake on the default CDN.
  That is separate from the five-minute EOF: libmpv could play that CDN with
  browser-style headers until the reproduced EOF.

Media buffering before EOF reaches playback, followed by source-refresh delay,
is consistent with the application's slightly longer interruption interval.
An error code of 0 means the EOF event did not report a libmpv error; it does
not reveal why the upstream response ended.

## Application Path

`native/service/douyu_backend.py` returns the platform's FLV URL without
changing its authentication parameters. `MpvQuickItem` treats remote EOF as
`PLAYBACK_FAILED`; `RoomSession` waits at least three seconds before resolving
another source. Playback progress resets the consecutive-retry budget, so each
five-minute interruption can appear as recovery attempt 1.

The previous hotfix restored playback after EOF. It did not prevent upstream
connection rotation or make that rotation seamless.

## Source Rotation Verification

The subsequent paired run lasted approximately 559 seconds and used actual
preserved/current packaged resolver services. A local numeric-result checker
validated the recorded timings and lifecycle events:

| Check | Result |
| --- | --- |
| Preserved V0.2.13 resolver with old playback options, timeout 60 seconds | EOF at 305.32 seconds, reason 0, error 0 |
| Current resolver with timeout 30 seconds and auto-copy | EOF at 307.89 seconds, reason 0, error 0 |
| Fetch a new source while original plays | Completed at 236.06 seconds in 0.98 seconds; address differs; original still active |
| Start fresh source alongside original | Started at 255.07 seconds, loaded in 0.63 seconds; original continued until normal EOF |
| Reopen exact original address after EOF | Loaded, then EOF after 3.65 seconds; not a viable sustained recovery in this run |
| Fresh source after original EOF | Continued advancing through five minute checkpoints, then EOF at its own 304.21 seconds |

V0.2.12, V0.2.13, V0.2.15 and current resolver functions have identical parsed
Python AST hashes. V0.2.15/current service manifests contain the same 113 paths
and hashes. Preserved V0.2.13/V0.2.14 and current libmpv binaries have identical
SHA-256 hashes. The old/current options comparison used null outputs to
isolate transport/demux timing rather than running the complete old GUI.

Both profiles now reproduce periodic EOF. This argues against the changed
30-second network timeout as its cause in this room. It does not prove that
Douyu changed its policy recently, nor explain which previous room, stream,
network or service response allowed longer playback.

The source was prefetched approximately 19 seconds before its connection
opened, yet that new connection lasted approximately 304 seconds. The observed
ending appears tied to a connection's lifetime in this run. Do not interpret
`expire=300` as proven signature-expiration semantics.

## Investigation Follow-Up

Continuous playback requires testing an EOF-specific transport reconnection
with fresh authorization where necessary, retaining the existing decoder and
buffer where possible, or a prepared secondary player with a controlled
handover. Fresh-source prefetch and overlap worked in this run; reusing the
old address did not provide sustained playback. Single-decoder transitions,
FLV timestamp continuity, Qt Quick visual output, audio handover, 16/24-room
peak resource use and repeated rotations were unverified at this investigation
stage. The later native run below verifies rendered output and repeated
rotations; zero-gap audio and peak multi-room performance remain unverified.
Keep retry bounds and cancellation.

Changing `expire`, lengthening the watchdog or switching CDN without testing
would not be an evidence-backed fix. Browser-style headers and the alternate
CDN did not remove the observed five-minute EOF in this run.

## Scope And Privacy

No production code, version number, installed program, release or GitHub
attachment changed during this investigation. Probe scripts and numeric
results are local ignored files under `native/out/verification/`.
Playback addresses, query values other than the non-secret lifetime, tokens,
signatures and video content were not saved in the diagnostic outputs.

## Application Fix And Native Verification

The subsequent application fix retains one libmpv core per room. `RoomSession`
uses a second resolver, not a second decoder, to fetch a replacement after
240 seconds plus a deterministic 0-10 second room offset. It refreshes the
in-memory replacement every 60 seconds if the original connection lasts longer.
Failed prefetch is isolated from active playback and retried at most three
times. An address older than 120 seconds is not used for handover. Quality
changes, stop, cancel, detach, offline state and release cancel prefetch.

`MpvQuickItem` notifies the session before classifying remote EOF/error as a
playback failure. A successful synchronous fresh-source load retires that end
event. Otherwise the existing bounded delayed recovery remains in effect.
The callback also honors intentional stop/release.

The opt-in `RoomSessionTest::sustainsLivePlaybackAcrossTwoSourceRotations`
used the actual packaged resolver, libmpv and a visible Windows Qt/OpenGL
window for room 217331. Its successful run lasted 629.563 seconds:

| Check | Result |
| --- | --- |
| First upstream end and fresh-source handover | Elapsed 307.131 seconds |
| Second upstream end and fresh-source handover | Elapsed 613.866 seconds |
| Session failures / delayed recovery | None |
| Handover output | Lit frame pixels and continued progress verified |
| Audio controls | Volume 37 and muted state preserved |

This proves repeated native single-player handover in this room and test run.
It does not prove zero-gap video/audio, every CDN, every room, or sustained
16/24-room peak decode performance. Server-side EOF still occurs; the fix
removes the mandatory delayed source-resolution wait when a fresh source is
available.

The first test attempt was terminated by QtTest's default 300-second function
timeout and is not success evidence. The completed run used
`QTEST_FUNCTION_TIMEOUT=720000`. To reproduce, set
`DOUYU_LIVE_VERIFY_SERVICE` to the packaged `streamget_service.exe`, use the
Windows Qt backend, and execute only this opt-in test with a text report.

The version remains 0.2.16. This task changes local code and executables only;
no installer, commit or remote release is created.

## Final Local Regression

Both stable16 and beta24 builds pass their native self-test. Each full CTest
suite passes 36 of 37 targets, including player/session lifecycle, QML
interaction, preset/sidebar, close regression, engine smoke and visual smoke.
The sole failing target is the existing navigation/team visual baseline
comparison; the baselines were not replaced.

Separate Windows-native checks pass 71 interaction cases, 7 close/preset
cases, 25 player cases and 29 session cases per profile. The real-network
case is opt-in and skipped in these ordinary suites; its completed 629.563
second run is recorded above.
