# Qt-only M3 Remote Playback Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a deterministic single-room remote playback lifecycle from the Qt StreamGet service to libmpv while preserving local-media behavior and sensitive-data boundaries.

**Architecture:** Extend `MediaSource` with a validated in-memory `RemoteStream` kind. Add `PlayerSurface::loadSource()` as the typed libmpv entry point. Add `RemotePlaybackController` to own one resolve generation, cancel stale requests, convert validated responses into sources, and emit only typed sources or fixed error codes. Use the existing fake child for offline controller tests and keep M3 free of visible room UI.

**Tech Stack:** C++20, Qt 6.8 Core/Test/Widgets/OpenGLWidgets, libmpv, CMake/Ninja/MSVC, existing `StreamgetProcessClient`, QtTest, and deterministic fake child processes.

---

## Scope and file map

Create:

- `native/src/media/remote_playback_controller.h`
- `native/src/media/remote_playback_controller.cpp`
- `native/tests/remote_playback_controller_test.cpp`

Modify:

- `native/src/media/media_source.h/.cpp`
- `native/src/media/player_surface.h/.cpp`
- `native/tests/media_source_test.cpp`
- `native/tests/player_surface_test.cpp`
- `native/tests/fake_streamget_service.cpp`
- `native/tests/streamget_process_client_test.cpp` only if the fake-child option needs a focused regression
- `native/CMakeLists.txt`
- `docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md`

Do not modify Electron, Node, React, TypeScript, Python service logic, or visible MainWindow controls. Do not connect a live Douyu URL.

### Task 1: Add the validated remote `MediaSource` kind

**Files:** `native/src/media/media_source.h/.cpp`, `native/tests/media_source_test.cpp`

- [ ] **Step 1: Write failing remote-source tests**

Add QtTest cases that construct a `StreamVariant` with an allowed URL and assert:

```cpp
void acceptsAllowedRemoteVariant();
void rejectsUnsafeRemoteVariants();
void keepsRemoteDescriptionFreeOfUrlData();
```

`acceptsAllowedRemoteVariant()` uses room `63136`, id `flv-auto`, quality `Auto`, container `flv`, and `https://live.douyucdn.cn/live/test.flv?wsAuth=redacted`; it asserts `Kind::RemoteStream`, room/variant/quality/container accessors, and exact stable description `remote-stream`.

`rejectsUnsafeRemoteVariants()` covers `ftp://`, `https://user:secret@live.douyucdn.cn/x.flv`, `https://example.invalid/x.flv`, an invalid room ID, an empty variant ID, an empty container, and an empty URL.

`keepsRemoteDescriptionFreeOfUrlData()` asserts the description contains none of `?`, `#`, `@`, `token`, or `wsAuth`.

- [ ] **Step 2: Run the focused test and verify red**

Run from the repository root:

```powershell
& cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build native/out/build/windows-x64 --target media_source_test --parallel 4'
native/out/build/windows-x64/media_source_test.exe acceptsAllowedRemoteVariant
```

Expected: compilation fails because `RemoteStream`, `fromRemoteVariant()`, and the remote accessors do not exist.

- [ ] **Step 3: Implement the minimum remote source model**

Extend `MediaSource` with `Kind::RemoteStream`, include `QUrl` and `service/stream_service_protocol.h`, and add:

```cpp
static std::optional<MediaSource> fromRemoteVariant(
    const QString &roomId, const StreamVariant &variant);
QUrl remoteUrl() const;
QString roomId() const;
QString variantId() const;
StreamQuality quality() const noexcept;
QString container() const;
```

Store local and remote data in explicit members. Validate room IDs with `^[0-9]{1,20}$`, require non-empty variant id/container, require HTTP(S), reject URL username/password, and require host suffix `.douyucdn.cn`, `.douyucdn2.cn`, or `.edgesrv.com`. Return `std::nullopt` for every invalid case. Keep local descriptor behavior unchanged and return `remote-stream` from `stableDescription()` for remote values.

- [ ] **Step 4: Run the focused tests and existing media-source tests**

Run:

```powershell
native/out/build/windows-x64/media_source_test.exe
ctest --test-dir native/out/build/windows-x64 --output-on-failure -R media_source_test
```

Expected: all media-source tests pass and no URL appears in test output.

- [ ] **Step 5: Commit the source-model slice**

```powershell
git add native/src/media/media_source.h native/src/media/media_source.cpp native/tests/media_source_test.cpp
git commit -m "feat: add validated remote media sources"
```

### Task 2: Add typed `PlayerSurface::loadSource()`

**Files:** `native/src/media/player_surface.h/.cpp`, `native/tests/player_surface_test.cpp`

- [ ] **Step 1: Write failing dispatch and rejection tests**

Add:

```cpp
void rejectsInvalidSourceWithoutChangingIdleState();
void acceptsValidatedRemoteSourceForLoading();
```

The first test uses `std::nullopt` from an unsafe remote variant and asserts the surface remains `Idle` or enters a fixed `Error` without a URL in `mediaError()`. The second creates a valid remote `MediaSource`, calls `loadSource(source)`, asserts `true`, `PlaybackState::Loading`, and then calls `stop()` so the test never waits on real network playback. It must not assert a remote first frame.

- [ ] **Step 2: Run the new tests and verify red**

Run:

```powershell
native/out/build/windows-x64/player_surface_test.exe rejectsInvalidSourceWithoutChangingIdleState
```

Expected: compilation fails because `loadSource()` does not exist.

- [ ] **Step 3: Implement typed source dispatch**

Add `bool loadSource(const MediaSource &source)` and make `loadLocalMedia(path)` construct a local source then delegate. In `loadSource()`:

1. Reject an uninitialized libmpv handle with fixed `libmpv is not initialized`.
2. For `LocalFile`, require `QFileInfo::isFile()` and pass its absolute path to `loadfile`.
3. For `RemoteStream`, use the already validated `remoteUrl().toEncoded()` only for the in-memory `mpv_command_async()` call.
4. Reset `mediaLoaded_`, `videoConfigured_`, and `firstFrameRendered_`; set `Loading`; clear `mediaError_`.
5. Use one load command request id for both local and remote loads, and keep stop/release cleanup unchanged.

`loadLocalMedia()` must preserve its existing return values and error behavior through the wrapper.

- [ ] **Step 4: Run PlayerSurface and all existing local lifecycle tests**

Run:

```powershell
native/out/build/windows-x64/player_surface_test.exe
ctest --test-dir native/out/build/windows-x64 --output-on-failure -R "media_source_test|player_surface_test"
```

Expected: all local load, first-frame, stop, repeated-stop, release/reload, and new source-dispatch tests pass.

- [ ] **Step 5: Commit the PlayerSurface slice**

```powershell
git add native/src/media/player_surface.h native/src/media/player_surface.cpp native/tests/player_surface_test.cpp
git commit -m "feat: load typed remote media sources in PlayerSurface"
```

### Task 3: Build `RemotePlaybackController` with fake-service coverage

**Files:** `native/src/media/remote_playback_controller.h/.cpp`, `native/tests/remote_playback_controller_test.cpp`, `native/tests/fake_streamget_service.cpp`

- [ ] **Step 1: Extend the fake child with deterministic response modes**

Add command-line options without network access:

```text
--offline-after N       resolve request N returns isLive=false and variants=[]
--error-code CODE       resolve request N returns ok=false with fixed CODE
```

Keep current `--delay-ms`, `--malformed`, `--crash-after`, `--crash-once-file`, and `--ignore-cancel` behavior unchanged. Error responses contain only `code` and `retryable`; no diagnostic text.

- [ ] **Step 2: Write failing controller tests**

Create `remote_playback_controller_test.cpp` with a `fakeServicePath()` helper and these QtTest cases:

```cpp
void resolvesOneTypedSource();
void mapsOfflineAndServiceErrorsToFixedCodes();
void cancelSuppressesLateSource();
void newerGenerationSuppressesOlderResponse();
void timeoutAndRestartRemainRetryable();
void releaseReturnsToIdleAndInvalidatesWork();
```

Each test creates a `StreamgetProcessClient` and `RemotePlaybackController`, spies on `sourceReady`, `failed`, and `stateChanged`, and waits with `QTRY_VERIFY_WITH_TIMEOUT`. `resolvesOneTypedSource()` asserts room `63136`, variant `flv-auto`, `RemoteStream`, and `remote-stream`; it never prints the URL. Offline mode expects `failed("ROOM_OFFLINE")`; error mode expects the fixed requested code. Cancel and generation tests wait longer than the fake delay and assert zero `sourceReady` emissions. Release asserts `Idle` and a larger generation.

- [ ] **Step 3: Run the focused controller target and verify red**

Run:

```powershell
& cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build native/out/build/windows-x64 --target remote_playback_controller_test --parallel 4'
ctest --test-dir native/out/build/windows-x64 --output-on-failure -R remote_playback_controller_test
```

Expected: CMake reports the target does not exist, proving the new tests are red before implementation.

- [ ] **Step 4: Implement the controller state machine**

Implement the exact public API from the spec. Connect to `StreamgetProcessClient::responseReceived` and `requestFailed` using queued/default same-thread delivery. Track `activeRequestId_`, `generation_`, `roomId_`, and `state_`.

Rules:

- `resolve()` increments generation, cancels any previous request, stores room/quality, enters `Resolving`, emits `stateChanged`, and calls `client_->resolve()`.
- Accept only a matching request ID while `Resolving`; reject non-OK responses, offline responses, empty variants, or invalid `MediaSource::fromRemoteVariant()` results with fixed codes.
- On success, emit `sourceReady(source)`, enter `Ready`, and clear the active request.
- On `requestFailed`, accept only the matching request and emit the provided fixed code, enter `Error`, and clear the active request.
- `cancel()`, `stop()`, and `release()` increment generation, call `client_->cancel(activeRequestId_)` if needed, clear active state, and suppress all later responses. `release()` enters `Idle`; `cancel()` and `stop()` enter `Idle` after invalidation.
- The destructor calls `release()` but does not own or shut down the shared client.

Declare `Q_DECLARE_METATYPE(MediaSource)` and register it in the test init so `QSignalSpy` can transport the typed source.

- [ ] **Step 5: Add CMake target and run focused controller tests**

Add `src/media/remote_playback_controller.cpp` to a small `remote_playback` static library linked to `stream_service` and Qt6::Core. Add `remote_playback_controller_test`, link Qt6::Test and `remote_playback`, depend on `fake_streamget_service`, and define `FAKE_STREAMGET_SERVICE_PATH` using `$<TARGET_FILE:fake_streamget_service>`.

Run:

```powershell
& cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build native/out/build/windows-x64 --target remote_playback_controller_test --parallel 4'
ctest --test-dir native/out/build/windows-x64 --output-on-failure -R remote_playback_controller_test
```

Expected: all six controller tests pass and no fake child remains.

- [ ] **Step 6: Commit the controller slice**

```powershell
git add native/src/media/remote_playback_controller.* native/tests/remote_playback_controller_test.cpp native/tests/fake_streamget_service.cpp native/CMakeLists.txt
git commit -m "feat: add single-room remote playback controller"
```

### Task 4: Integrate M3 targets and preserve the native product boundary

**Files:** `native/CMakeLists.txt`, optionally `native/README.md`

- [ ] **Step 1: Add a compile-only API integration test**

Add one controller test that connects `sourceReady` to a lambda calling `PlayerSurface::loadSource()` only after render context readiness is established. The test uses the fake child URL and stops immediately after `loadSource()` returns; it does not wait for a remote first frame or contact Douyu.

- [ ] **Step 2: Ensure existing targets keep their source lists**

Keep all M1 media and self-test targets unchanged except for adding `src/media/media_source.cpp` where the new `MediaSource` API is required. Do not add the controller to `MainWindow` or `app/main.cpp` in M3.

- [ ] **Step 3: Run a clean configure/build and inspect target inventory**

Run:

```powershell
$env:QT_ROOT = 'D:\Qt\6.8.3\msvc2022_64'
$env:MPV_ROOT = (Resolve-Path .\native\sdk\mpv).Path
& cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --preset windows-x64 -S native && cmake --build --preset windows-x64-debug --parallel 4'
ctest --test-dir native/out/build/windows-x64 -N
```

Expected: existing M1 targets plus `stream_service_protocol_test`, `streamget_process_client_test`, `streamget_service_python_tests`, and `remote_playback_controller_test`; no Electron or Node target appears.

### Task 5: M3 acceptance, audit, and evidence

**Files:** `docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md` only after verification

- [ ] **Step 1: Run all tests from a fresh MSVC build**

Run:

```powershell
& cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build native/out/build/windows-x64 --parallel 4'
native/.venv/Scripts/python.exe -m unittest discover -s native/service/tests -v
ctest --test-dir native/out/build/windows-x64 --output-on-failure
```

Expected: Python `19/19` or greater after any added tests, all existing M1/M2 tests, and the controller target pass.

- [ ] **Step 2: Run focused M3 verification**

```powershell
ctest --test-dir native/out/build/windows-x64 --output-on-failure -R "media_source_test|player_surface_test|remote_playback_controller_test"
```

Expected: all three focused groups pass, with no service/fake-child/client process left behind.

- [ ] **Step 3: Scan captured output for sensitive material**

Capture only test stdout/stderr and scan for `playbackUrl`, `wsAuth`, `Cookie`, `token`, `signature`, `Traceback`, and raw exception text. The scan must be clean. URLs may exist in in-memory test fixtures and protocol payloads but not in logs/files/stderr/crash artifacts.

- [ ] **Step 4: Run repository hygiene checks**

```powershell
git diff --check
Get-Process | Where-Object { $_.ProcessName -match 'streamget_service|fake_streamget_service|douyu_monitor_native|streamget_process_client_test|remote_playback_controller_test' }
git status --short
```

Expected: no whitespace errors, no matching processes, and only known unrelated worktree changes remain outside the M3 commits.

- [ ] **Step 5: Record and sync evidence**

Append measured build/test/scan/process-cleanup evidence to
`docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md`, create a new
Notion M3 completion page, reread it, compare every acceptance item against the
spec, and mark M4 as the next phase only if every check is green.

- [ ] **Step 6: Commit scoped verification evidence**

```powershell
git add docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md
git commit -m "test: verify Qt-only M3 remote playback"
```

## Plan self-review

- **Spec coverage:** Tasks 1-2 cover remote source construction and typed
  PlayerSurface loading. Task 3 covers controller state, cancellation,
  generation, errors, restart, and fake-service modes. Tasks 4-5 cover CMake,
  local regressions, security scanning, and evidence.
- **Placeholder scan:** No `TBD`, `TODO`, or unspecified implementation step is
  required; every test, file, command, and expected result is named.
- **Type consistency:** `StreamVariant` comes from the M2 protocol header;
  `MediaSource::fromRemoteVariant()` returns `std::optional<MediaSource>`;
  `RemotePlaybackController::sourceReady` carries `MediaSource`; controller
  request IDs use `quint64` and fixed error codes remain `QString` values.
- **Scope check:** No UI, search, multi-room, Electron, or live-network work is
  included. M3 remains independently testable through fake child processes and
  local libmpv command acceptance.

