# Qt-only M4 Multi-room Playback And Quality Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a Qt-native workspace that independently manages up to nine libmpv-backed rooms and applies the approved adaptive quality policy.

**Architecture:** Keep one shared `StreamgetProcessClient` with its existing two-request in-flight bound. Add a pure quality-policy module, a `RoomSession` wrapper around one `RemotePlaybackController` and one `PlayerSurface`, and a `MultiRoomCoordinator` that owns room order, primary-room selection, capacity checks, and targeted re-resolution. Extend `MainWindow` with a stable Qt grid while preserving the existing local-media API and libmpv lifecycle.

**Tech Stack:** C++20, Qt 6.8 Core/Widgets/OpenGLWidgets/Test, libmpv, CMake/Ninja/MSVC, existing fake StreamGet child, and QtTest.

---

## Scope and file map

Create:

- `native/src/workspace/quality_policy.h/.cpp`: pure room-count/primary-room quality calculation.
- `native/src/workspace/room_session.h/.cpp`: one room's state, quality fields, controller and player surface.
- `native/src/workspace/multi_room_coordinator.h/.cpp`: room collection, capacity, ordering, primary-room changes and targeted reloads.
- `native/tests/quality_policy_test.cpp`: deterministic policy and grid boundary tests.
- `native/tests/room_session_test.cpp`: session lifecycle and stale-response tests using the fake service.
- `native/tests/multi_room_coordinator_test.cpp`: capacity, quality, ordering and isolation tests.

Modify:

- `native/src/app/main_window.h/.cpp`: replace the single central player surface with a grid host and expose coordinator APIs.
- `native/tests/main_window_test.cpp`: add 1/4/6/9 grid and removal/reorder GUI coverage while retaining local-media tests.
- `native/CMakeLists.txt`: add workspace library and test targets.
- `docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md`: append only after verification checkpoints.

Do not modify Electron, Node, React, TypeScript, or Python resolver behavior. Do not add real Douyu URLs or credentials to tests.

## Public interfaces fixed by this plan

```cpp
struct RoomQualityDecision {
    StreamQuality userQuality = StreamQuality::Auto;
    StreamQuality effectiveQuality = StreamQuality::Auto;
};

RoomQualityDecision resolveRoomQuality(int managedRoomCount,
                                       bool isPrimary,
                                       StreamQuality userQuality);
QString recommendedGridId(int roomCount);
```

```cpp
class MultiRoomCoordinator final : public QObject {
    Q_OBJECT
public:
    explicit MultiRoomCoordinator(StreamgetProcessClient *client,
                                  QWidget *surfaceParent,
                                  QObject *parent = nullptr);
    bool addRoom(const QString &roomId, StreamQuality userQuality = StreamQuality::Auto);
    bool removeRoom(const QString &roomId);
    bool setPrimaryRoom(const QString &roomId);
    int roomCount() const noexcept;
    QString primaryRoomId() const;
    QStringList roomIds() const;
    PlayerSurface *surfaceForRoom(const QString &roomId) const noexcept;
signals:
    void roomAdded(QString roomId);
    void roomRemoved(QString roomId);
    void layoutChanged(QString layoutId);
    void roomStateChanged(QString roomId);
    void qualityChanged(QString roomId, StreamQuality effectiveQuality);
    void failed(QString roomId, QString errorCode);
};
```

## Task 1: Implement the pure quality policy

**Files:**
- Create: `native/src/workspace/quality_policy.h/.cpp`
- Create: `native/tests/quality_policy_test.cpp`
- Modify: `native/CMakeLists.txt`

- [ ] **Step 1: Write failing policy and grid tests**

Add QtTest cases asserting:

```cpp
QCOMPARE(resolveRoomQuality(1, true, StreamQuality::High).effectiveQuality,
         StreamQuality::High);
QCOMPARE(resolveRoomQuality(4, false, StreamQuality::Original).effectiveQuality,
         StreamQuality::Original);
QCOMPARE(resolveRoomQuality(5, true, StreamQuality::Auto).effectiveQuality,
         StreamQuality::Original);
QCOMPARE(resolveRoomQuality(5, false, StreamQuality::Original).effectiveQuality,
         StreamQuality::Standard);
QCOMPARE(resolveRoomQuality(9, false, StreamQuality::High).effectiveQuality,
         StreamQuality::Standard);
QCOMPARE(resolveRoomQuality(5, false, StreamQuality::Original).userQuality,
         StreamQuality::Original);
QCOMPARE(recommendedGridId(0), QStringLiteral("single"));
QCOMPARE(recommendedGridId(1), QStringLiteral("single"));
QCOMPARE(recommendedGridId(4), QStringLiteral("grid-2x2"));
QCOMPARE(recommendedGridId(6), QStringLiteral("grid-3x2"));
QCOMPARE(recommendedGridId(9), QStringLiteral("grid-3x3"));
```

Also assert negative counts clamp to `single`, counts above nine use `grid-3x3`,
and the coordinator—not this helper—owns the tenth-room rejection.

- [ ] **Step 2: Run the focused target and verify it fails**

```powershell
& cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build native/out/build/windows-x64 --target quality_policy_test --parallel 4'
```

Expected: the target or policy symbols do not exist yet.

- [ ] **Step 3: Implement the minimum pure policy**

For managed counts 1–4, return the user's quality. For counts 5–9, return
`Original` for the primary room and `Standard` for every other room; `Standard`
is the existing service quality used for the 720p request. Always preserve
`userQuality` unchanged. Implement `recommendedGridId()` with the 1/2x2/3x2/3x3
boundaries and no widget dependencies.

- [ ] **Step 4: Build and run policy tests**

```powershell
& cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build native/out/build/windows-x64 --target quality_policy_test --parallel 4'
ctest --test-dir native/out/build/windows-x64 --output-on-failure -R quality_policy_test
```

Expected: all policy and grid boundary cases pass.

- [ ] **Step 5: Commit the policy slice**

```powershell
git add native/src/workspace/quality_policy.* native/tests/quality_policy_test.cpp native/CMakeLists.txt
git commit -m "feat: add multi-room quality policy"
```

## Task 2: Create the independent RoomSession lifecycle

**Files:**
- Create: `native/src/workspace/room_session.h/.cpp`
- Create: `native/tests/room_session_test.cpp`
- Modify: `native/CMakeLists.txt`

- [ ] **Step 1: Write failing session tests**

Cover `startsResolvingWithUserAndEffectiveQuality`, `acceptsSourceAndReportsReady`,
`cancelSuppressesLateSource`, `removesSessionStateWithoutLeakingSurface`, and
`mapsControllerErrorsWithoutRawDiagnostics` with `QSignalSpy` and the existing
fake child. Assert room ID, user/effective quality, state transitions, fixed
error codes, and that no signal or description contains a playback URL.

- [ ] **Step 2: Run the focused target and verify it fails**

```powershell
& cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build native/out/build/windows-x64 --target room_session_test --parallel 4'
```

Expected: the target or `RoomSession` API is missing.

- [ ] **Step 3: Implement explicit RoomSession ownership**

Give `RoomSession` a stable room ID, user quality, effective quality, one
`RemotePlaybackController` using the shared client, and one `PlayerSurface`
using the supplied QWidget parent. Add `resolve()`, `cancel()`, `stop()`,
`release()`, read-only state accessors, and signals for source-ready, state,
quality, and fixed failure code. `release()` must stop/release the surface and
invalidate the controller before the session is destroyed.

- [ ] **Step 4: Run session tests and process cleanup checks**

```powershell
ctest --test-dir native/out/build/windows-x64 --output-on-failure -R room_session_test
Get-Process | Where-Object { $_.ProcessName -match 'fake_streamget_service|room_session_test' }
```

Expected: all session tests pass and no matching process remains.

- [ ] **Step 5: Commit the session slice**

```powershell
git add native/src/workspace/room_session.* native/tests/room_session_test.cpp native/CMakeLists.txt
git commit -m "feat: add independent room playback sessions"
```

## Task 3: Build MultiRoomCoordinator capacity and quality orchestration

**Files:**
- Create: `native/src/workspace/multi_room_coordinator.h/.cpp`
- Create: `native/tests/multi_room_coordinator_test.cpp`
- Modify: `native/CMakeLists.txt`

- [ ] **Step 1: Write failing coordinator tests**

Add tests named `acceptsNineRoomsAndRejectsTheTenth`,
`rejectsDuplicateRoomIds`, `appliesUserQualityAtFourRooms`,
`appliesPrimaryOriginalAndOthers720pAtFiveRooms`,
`switchingPrimaryOnlyReloadsAffectedRooms`,
`droppingToFourRoomsRestoresUserQuality`, `removesRoomAndReflowsOrder`, and
`oneRoomFailureDoesNotBlockOtherRooms`. Use `QSignalSpy` on
`qualityChanged`, `layoutChanged`, `roomStateChanged`, and `failed`; assert the
exact affected room IDs for primary changes and quality restoration.

- [ ] **Step 2: Run the focused target and verify it fails**

```powershell
& cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build native/out/build/windows-x64 --target multi_room_coordinator_test --parallel 4'
```

Expected: the target or coordinator symbols are missing.

- [ ] **Step 3: Implement bounded room collection and primary-room rules**

Use `QVector` for stable order and `QHash<QString, RoomSession*>` for lookup.
`addRoom()` rejects empty/invalid IDs, duplicates, and counts above 9. The first
room becomes primary. `removeRoom()` releases the session, selects the first
remaining room as primary when needed, recomputes quality, and emits one layout
change. `setPrimaryRoom()` rejects unknown IDs and recomputes only the old and
new primary sessions.

Treat sessions requested for playback as the managed count while a resolve is
pending. When a response reports offline/error, mark only that session
unavailable and emit its fixed error; do not block or restart other sessions.

- [ ] **Step 4: Implement targeted quality recomputation and scheduling**

Compare each new effective quality with the session's previous value. Call
`RoomSession::resolve()` only for changed sessions; do not reload unaffected
rooms. Rely on `StreamgetProcessClient`'s existing maximum of two in-flight
requests. Keep coordinator signals free of URLs and raw service diagnostics.

- [ ] **Step 5: Run coordinator tests and verify isolation**

```powershell
ctest --test-dir native/out/build/windows-x64 --output-on-failure -R multi_room_coordinator_test
```

Expected: all capacity, policy, primary-switch, restoration, ordering, and
failure-isolation cases pass.

- [ ] **Step 6: Commit the coordinator slice**

```powershell
git add native/src/workspace/multi_room_coordinator.* native/tests/multi_room_coordinator_test.cpp native/CMakeLists.txt
git commit -m "feat: orchestrate up to nine playback rooms"
```

## Task 4: Integrate the Qt multi-room grid into MainWindow

**Files:**
- Modify: `native/src/app/main_window.h/.cpp`
- Modify: `native/tests/main_window_test.cpp`
- Modify: `native/app/main.cpp`
- Modify: `native/CMakeLists.txt`

- [ ] **Step 1: Extend GUI tests before implementation**

Add a test that calls `addRoom()` for 1, then 3, then 5 more rooms and asserts
`roomCount()` values 1, 4, 9 and layout IDs `single`, `grid-2x2`, `grid-3x3`.
Add assertions that the tenth add returns `false`, removal reflows the grid,
primary selection preserves remaining surfaces, and existing local-media
load/pause/stop tests remain unchanged.

- [ ] **Step 2: Run the GUI target and verify the new tests fail**

```powershell
& cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build native/out/build/windows-x64 --target main_window_test --parallel 4'
native/out/build/windows-x64/main_window_test.exe
```

Expected: compilation fails because the multi-room MainWindow API is absent.

- [ ] **Step 3: Add a QWidget grid host and coordinator ownership**

Create a central QWidget with a `QGridLayout`, instantiate one shared
`StreamgetProcessClient` using `QCoreApplication::applicationDirPath()` joined
with `streamget_service.exe`, and construct `MultiRoomCoordinator` with the
grid host as the surface parent. Keep a constructor overload accepting an
explicit service path for tests. Expose `addRoom`, `removeRoom`,
`setPrimaryRoom`, `roomCount`, `layoutId`, `roomIds`, and `surfaceForRoom` as
testable methods.

- [ ] **Step 4: Implement deterministic grid reflow**

On `roomAdded`, `roomRemoved`, or `layoutChanged`, clear only layout item
positions and re-add existing `PlayerSurface` widgets by room order. Do not
delete widgets during reflow. Use `recommendedGridId()` so 7–9 rooms always
occupy a 3x3 grid and each slot has stable minimum dimensions.

- [ ] **Step 5: Preserve single-room compatibility**

Make `playerSurface()` return the primary room's surface when a room exists and
retain a lazily-created compatibility surface for existing local-media tests
when the workspace is empty. `loadLocalMedia()` continues to validate a local
descriptor before loading and never sends a local path to StreamGet.

- [ ] **Step 6: Run GUI and native self-test verification**

```powershell
ctest --test-dir native/out/build/windows-x64 --output-on-failure -R "main_window_test|native_self_test|native_self_test_media"
```

Expected: old local lifecycle tests and the new 1/4/6/9 GUI cases pass.

- [ ] **Step 7: Commit the Qt integration slice**

```powershell
git add native/src/app/main_window.* native/app/main.cpp native/tests/main_window_test.cpp native/CMakeLists.txt
git commit -m "feat: integrate the Qt multi-room grid"
```

## Task 5: Add full-build acceptance, hygiene evidence, and Notion sync

**Files:**
- Modify: `docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md`

- [ ] **Step 1: Configure and build from the MSVC x64 environment**

```powershell
$env:QT_ROOT = 'D:\Qt\6.8.3\msvc2022_64'
$env:MPV_ROOT = (Resolve-Path .\native\sdk\mpv).Path
& cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --preset windows-x64 -S native && cmake --build --preset windows-x64-debug --parallel 4'
```

Expected: exit code `0` and the new workspace targets are present.

- [ ] **Step 2: Run all native and Python tests**

```powershell
ctest --test-dir native/out/build/windows-x64 --output-on-failure
native/.venv/Scripts/python.exe -m unittest discover -s native/service/tests -v
```

Expected: all CTest targets pass and the Python service suite remains at least
`19/19`.

- [ ] **Step 3: Scan captured output for sensitive material**

Capture build/test stdout and stderr, then scan for `playbackUrl`, `wsAuth`,
`Cookie`, `token`, `signature`, `Traceback`, and raw exception text. Fixture
URLs may exist in source code, but captured output and evidence files must not
contain them.

- [ ] **Step 4: Run repository hygiene and process checks**

```powershell
git diff --check
Get-Process | Where-Object { $_.ProcessName -match 'streamget_service|fake_streamget_service|douyu_monitor_native|multi_room_coordinator_test|room_session_test' }
```

Expected: no whitespace errors, no matching processes, and no unrelated files
added by the M4 implementation.

- [ ] **Step 5: Append measured evidence to the local log**

Record exact build, CTest, Python, GUI, sensitive-output scan, and process-cleanup
results. State any live 9-room smoke-test limitation explicitly; do not include
raw URLs or service diagnostics.

- [ ] **Step 6: Create and reread the Notion M4 implementation log**

Create a page titled `DouyuMonitor M4 实施进度与验收` containing commit IDs,
test counts, current completed task, remaining task, and a link to this plan.
Fetch the page again and verify that its summary matches the spec and plan.

- [ ] **Step 7: Commit verification evidence**

```powershell
git add docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md
git commit -m "test: verify Qt-only M4 multi-room playback"
```

## Plan self-review

- **Spec coverage:** Tasks 1–2 cover policy, layouts, session state, cancellation and release. Task 3 covers nine-room capacity, primary-room changes, quality restoration, scheduling and isolation. Task 4 covers the Qt grid, stable reflow, compatibility controls and GUI behavior. Task 5 covers full build/test, security scanning, process cleanup, local evidence and Notion synchronization.
- **Placeholder scan:** No `TBD`, `TODO`, or unspecified implementation step is required; commands and expected outcomes are named.
- **Type consistency:** `StreamQuality::Standard` is the 720p request used by the policy. `RoomQualityDecision`, `RoomSession`, and `MultiRoomCoordinator` signatures are consistent across tasks. Existing `RemotePlaybackController` and `PlayerSurface` APIs are reused rather than duplicated.
- **Scope check:** No Electron, browser runtime, proxy, live network, danmaku, or persistence work is included. The plan is one independently testable Qt workspace milestone.
