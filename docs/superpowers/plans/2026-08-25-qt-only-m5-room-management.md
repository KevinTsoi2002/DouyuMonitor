# Qt-only M5 Room Management Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** Add a Qt-native dock that lets users add, remove, choose the primary room, and set requested quality for up to nine libmpv-backed rooms.

**Architecture:** MultiRoomCoordinator remains the only source of room order, primary state, session state, requested quality, and effective quality. It publishes an ordered safe snapshot plus command results. RoomManagementDock renders that snapshot and emits UI intent only. MainWindow routes each intent to the coordinator, then refreshes the dock and playback grid from one snapshot-change signal.

**Tech Stack:** C++20, Qt 6 Core/Widgets/OpenGLWidgets/Test, libmpv, CMake/Ninja/MSVC, QtTest, and the existing fake StreamGet child.

---

## Scope and file map

Create:

- native/src/workspace/room_workspace_types.h: UI-safe snapshot and stable command-result types.
- native/src/app/room_management_dock.h/.cpp: room-ID input, room-list rows, fixed feedback, and intent signals.
- native/tests/room_management_dock_test.cpp: isolated offline QtTest coverage for the dock.

Modify:

- native/src/workspace/room_session.h/.cpp: allow the coordinator to update a saved requested quality.
- native/src/workspace/multi_room_coordinator.h/.cpp: return command results and publish ordered snapshots.
- native/src/app/main_window.h/.cpp: own the right-side QDockWidget and wire dock commands.
- native/tests/room_session_test.cpp, native/tests/multi_room_coordinator_test.cpp, and native/tests/main_window_test.cpp: extend existing behavior coverage.
- native/CMakeLists.txt: build the dock test and compile the dock into native app targets.
- docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md: append M5 evidence after fresh verification.

Do not modify Electron, Chromium, Qt WebEngine, WebView, Node, React, TypeScript, the Python resolver, or URL validation. Do not write real playback URLs, cookies, tokens, signatures, request headers, raw service output, tracebacks, or mpv diagnostics to tests or evidence.

## Contracts

Create native/src/workspace/room_workspace_types.h:

~~~cpp
#pragma once

#include <QMetaType>
#include <QString>
#include <QVector>

#include "service/stream_service_protocol.h"
#include "workspace/room_session.h"

enum class RoomCommandResult {
    Accepted,
    InvalidRoomId,
    DuplicateRoomId,
    RoomLimitReached,
    RoomNotFound,
    AlreadyPrimary,
    Unchanged,
    Unavailable,
};

struct RoomSnapshot {
    QString roomId;
    bool isPrimary = false;
    RoomSession::State state = RoomSession::State::Idle;
    StreamQuality requestedQuality = StreamQuality::Auto;
    StreamQuality effectiveQuality = StreamQuality::Auto;
};

using RoomSnapshots = QVector<RoomSnapshot>;

Q_DECLARE_METATYPE(RoomCommandResult)
Q_DECLARE_METATYPE(RoomSnapshot)
Q_DECLARE_METATYPE(RoomSnapshots)
~~~

Add these MultiRoomCoordinator methods. The current bool methods remain as compatibility wrappers.

~~~cpp
RoomCommandResult addRoomDetailed(const QString &roomId,
                                  StreamQuality requestedQuality = StreamQuality::Auto);
RoomCommandResult removeRoomDetailed(const QString &roomId);
RoomCommandResult setPrimaryRoomDetailed(const QString &roomId);
RoomCommandResult setRequestedQuality(const QString &roomId,
                                      StreamQuality requestedQuality);
RoomSnapshots roomSnapshots() const;

signals:
    void roomSnapshotsChanged(RoomSnapshots snapshots);
~~~

Create the dock interface:

~~~cpp
class RoomManagementDock final : public QWidget {
    Q_OBJECT
public:
    explicit RoomManagementDock(QWidget *parent = nullptr);

    QLineEdit *roomIdInput() const noexcept;
    QPushButton *addButton() const noexcept;
    QWidget *rowForRoom(const QString &roomId) const noexcept;
    QString feedbackText() const;

public slots:
    void setRooms(const RoomSnapshots &snapshots);
    void setCommandResult(RoomCommandResult result);

signals:
    void addRequested(QString roomId);
    void removeRequested(QString roomId);
    void primaryRequested(QString roomId);
    void requestedQualityChanged(QString roomId, StreamQuality quality);
};
~~~

## Task 1: Add requested-quality mutation, result codes, and snapshots

**Files:**
- Create: native/src/workspace/room_workspace_types.h
- Modify: native/src/workspace/room_session.h
- Modify: native/src/workspace/room_session.cpp
- Modify: native/src/workspace/multi_room_coordinator.h
- Modify: native/src/workspace/multi_room_coordinator.cpp
- Modify: native/tests/room_session_test.cpp
- Modify: native/tests/multi_room_coordinator_test.cpp

- [ ] **Step 1: Add a failing RoomSession test**

Add updatesRequestedQualityWithoutChangingEffectiveQuality:

~~~cpp
void RoomSessionTest::updatesRequestedQualityWithoutChangingEffectiveQuality()
{
    StreamgetProcessClient client(fakeServicePath());
    QWidget host;
    RoomSession session(&client, QStringLiteral("63136"), StreamQuality::High, &host);

    QVERIFY(session.setRequestedQuality(StreamQuality::Super));
    QCOMPARE(session.userQuality(), StreamQuality::Super);
    QCOMPARE(session.effectiveQuality(), StreamQuality::High);
    QVERIFY(!session.setRequestedQuality(StreamQuality::Super));
    client.shutdown();
}
~~~

- [ ] **Step 2: Verify the new test fails**

~~~powershell
Push-Location native
& cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build out/build/windows-x64 --target room_session_test --parallel 4'
Pop-Location
~~~

Expected: compile failure because setRequestedQuality is absent.

- [ ] **Step 3: Implement the smallest RoomSession API**

Declare and implement this method without resolving or changing effective quality:

~~~cpp
bool RoomSession::setRequestedQuality(StreamQuality quality)
{
    if (userQuality_ == quality) return false;
    userQuality_ = quality;
    return true;
}
~~~

- [ ] **Step 4: Confirm the focused test passes**

~~~powershell
Push-Location native
ctest --test-dir out/build/windows-x64 --output-on-failure -R room_session_test
Pop-Location
~~~

Expected: room_session_test passes with no real network traffic.

- [ ] **Step 5: Add failing coordinator tests**

Add three slots to native/tests/multi_room_coordinator_test.cpp:

~~~cpp
void returnsSpecificResultsForManagementCommands();
void publishesOrderedSnapshotsWithPolicyOverrides();
void restoresChangedRequestedQualityAfterDroppingToFourRooms();
~~~

The result test must assert:

~~~cpp
QCOMPARE(coordinator.addRoomDetailed(QStringLiteral("x")),
         RoomCommandResult::InvalidRoomId);
QCOMPARE(coordinator.addRoomDetailed(QStringLiteral("63136")),
         RoomCommandResult::Accepted);
QCOMPARE(coordinator.addRoomDetailed(QStringLiteral("63136")),
         RoomCommandResult::DuplicateRoomId);
QCOMPARE(coordinator.removeRoomDetailed(QStringLiteral("63137")),
         RoomCommandResult::RoomNotFound);
QCOMPARE(coordinator.setPrimaryRoomDetailed(QStringLiteral("63136")),
         RoomCommandResult::AlreadyPrimary);
QCOMPARE(coordinator.setRequestedQuality(QStringLiteral("63136"), StreamQuality::Auto),
         RoomCommandResult::Unchanged);
~~~

Attach QSignalSpy to roomSnapshotsChanged, add five High rooms, and inspect the final snapshot:

~~~cpp
const RoomSnapshots snapshots = snapshotSpy.last().at(0).value<RoomSnapshots>();
QCOMPARE(snapshots.size(), 5);
QCOMPARE(snapshots.at(0).roomId, roomId(0));
QVERIFY(snapshots.at(0).isPrimary);
QCOMPARE(snapshots.at(0).effectiveQuality, StreamQuality::Original);
QCOMPARE(snapshots.at(1).requestedQuality, StreamQuality::High);
QCOMPARE(snapshots.at(1).effectiveQuality, StreamQuality::Standard);
~~~

For the restoration test, change roomId(1) to Super at five rooms, assert effective Standard, remove the fifth room, then assert requested and effective Super.

- [ ] **Step 6: Verify the coordinator tests fail**

~~~powershell
Push-Location native
& cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build out/build/windows-x64 --target multi_room_coordinator_test --parallel 4'
Pop-Location
~~~

Expected: missing result-code and snapshot types or methods.

- [ ] **Step 7: Implement snapshots and detailed commands**

Include room_workspace_types.h from multi_room_coordinator.h and register the vector type:

~~~cpp
qRegisterMetaType<RoomSnapshots>("RoomSnapshots");
~~~

Add a private publisher:

~~~cpp
void MultiRoomCoordinator::publishSnapshots()
{
    RoomSnapshots snapshots;
    snapshots.reserve(order_.size());
    for (const QString &roomId : order_) {
        const RoomSession *session = sessions_.value(roomId, nullptr);
        if (session == nullptr) continue;
        snapshots.push_back({roomId,
                             roomId == primaryRoomId_,
                             session->state(),
                             session->userQuality(),
                             session->effectiveQuality()});
    }
    emit roomSnapshotsChanged(std::move(snapshots));
}
~~~

addRoomDetailed returns InvalidRoomId, DuplicateRoomId, RoomLimitReached, or Unavailable before allocating; on success it preserves current order, primary, layout, quality, resolve, and old signals, then publishes and returns Accepted. The old wrapper is:

~~~cpp
return addRoomDetailed(roomId, userQuality) == RoomCommandResult::Accepted;
~~~

Use the same wrapper pattern for removal and primary selection. Return RoomNotFound for unknown rooms and AlreadyPrimary for the current primary. Implement quality command:

~~~cpp
RoomSession *session = sessionForRoom(roomId);
if (session == nullptr) return RoomCommandResult::RoomNotFound;
if (!session->setRequestedQuality(requestedQuality)) return RoomCommandResult::Unchanged;
recomputeQuality();
publishSnapshots();
return RoomCommandResult::Accepted;
~~~

Publish after successful add, remove, primary switch, requested-quality change, and forwarded RoomSession state changes. Disconnect a removed session from the coordinator before release so removal emits only the final post-removal snapshot.

- [ ] **Step 8: Run focused M5 workspace tests**

~~~powershell
Push-Location native
ctest --test-dir out/build/windows-x64 --output-on-failure -R "room_session_test|multi_room_coordinator_test"
Pop-Location
~~~

Expected: quality retention, result codes, ordering, policy override, and restoration pass.

- [ ] **Step 9: Commit the slice**

~~~powershell
git add native/src/workspace/room_workspace_types.h native/src/workspace/room_session.* native/src/workspace/multi_room_coordinator.* native/tests/room_session_test.cpp native/tests/multi_room_coordinator_test.cpp
git commit -m "feat: expose room workspace snapshots"
~~~

## Task 2: Build the native management dock

**Files:**
- Create: native/src/app/room_management_dock.h
- Create: native/src/app/room_management_dock.cpp
- Create: native/tests/room_management_dock_test.cpp
- Modify: native/CMakeLists.txt

- [ ] **Step 1: Write dock tests and its CMake target**

Add:

~~~cmake
add_executable(room_management_dock_test
    tests/room_management_dock_test.cpp
    src/app/room_management_dock.cpp
)
target_link_libraries(room_management_dock_test PRIVATE Qt6::Widgets Qt6::Test)
target_include_directories(room_management_dock_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_test(NAME room_management_dock_test COMMAND room_management_dock_test)
~~~

Create RoomManagementDockTest with:

~~~cpp
void validatesRoomIdAndCapacityBeforeAdd();
void emitsAddRemovePrimaryAndQualityIntents();
void rendersSnapshotsWithoutEchoingProgrammaticChanges();
void displaysOnlyFixedCommandFeedback();
~~~

Use object names roomIdInput, addRoomButton, roomFeedback, roomRow-<id>, primaryButton, qualityCombo, effectiveQualityLabel, policyLabel, and removeButton.

The validation test must prove empty or nonnumeric input disables add, 63136 enables it, and nine snapshots disable it. The signal test must prove all four signals. The add portion is:

~~~cpp
QSignalSpy adds(&dock, &RoomManagementDock::addRequested);
dock.roomIdInput()->setText(QStringLiteral("63136"));
dock.addButton()->click();
QCOMPARE(adds.count(), 1);
QCOMPARE(adds.at(0).at(0).toString(), QStringLiteral("63136"));
~~~

Use this snapshot to verify quality display, then call setRooms again under a requestedQualityChanged spy and assert zero emitted signals:

~~~cpp
const RoomSnapshots snapshots = {{QStringLiteral("63136"), false,
                                  RoomSession::State::Ready,
                                  StreamQuality::High,
                                  StreamQuality::Standard}};
dock.setRooms(snapshots);
QCOMPARE(dock.rowForRoom(QStringLiteral("63136"))
             ->findChild<QLabel *>(QStringLiteral("effectiveQualityLabel"))->text(),
         QStringLiteral("Effective: Standard"));
QCOMPARE(dock.rowForRoom(QStringLiteral("63136"))
             ->findChild<QLabel *>(QStringLiteral("policyLabel"))->text(),
         QStringLiteral("Policy active"));
~~~

setCommandResult(InvalidRoomId) and setCommandResult(DuplicateRoomId) must produce only the fixed feedback messages.

- [ ] **Step 2: Configure and verify target failure**

~~~powershell
Push-Location native
& cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --preset windows-x64 && cmake --build --preset windows-x64-debug --target room_management_dock_test --parallel 4'
Pop-Location
~~~

Expected: the dock source or header is missing.

- [ ] **Step 3: Implement the dock and fixed-height rows**

Build a QVBoxLayout with input/feedback and a QScrollArea for the rows. Apply a QRegularExpressionValidator using ^[0-9]{1,20}$ and calculate add enablement:

~~~cpp
const bool canAdd = roomIdInput_->hasAcceptableInput()
    && rows_.size() < MultiRoomCoordinator::kMaxRooms;
addButton_->setEnabled(canAdd);
~~~

Disable add before emitting addRequested(roomIdInput_->text()). setCommandResult must re-run validation, clear and focus input only for Accepted, and use only this mapping:

~~~cpp
InvalidRoomId    -> QStringLiteral("Invalid room ID")
DuplicateRoomId  -> QStringLiteral("Room already exists")
RoomLimitReached -> QStringLiteral("Maximum of 9 rooms reached")
RoomNotFound     -> QStringLiteral("Room is no longer managed")
AlreadyPrimary   -> QStringLiteral("Room is already primary")
Unavailable      -> QStringLiteral("Room management is unavailable")
Accepted         -> QString()
Unchanged        -> QString()
~~~

Each row disables its primary, quality, and remove controls immediately before it
emits an intent. setCommandResult() re-enables controls on retained rows after
the synchronous coordinator command returns; a successfully removed row is
discarded by the snapshot refresh. This prevents repeated clicks from queuing
conflicting UI commands.

Create a private RoomManagementRow inside the .cpp. Set fixed height 44. Give it room ID, state, primary QToolButton, QComboBox, effective label, policy label, and remove QToolButton. Use QStyle::SP_DialogYesButton for primary and QStyle::SP_DialogCloseButton for remove, with tooltips and accessible names.

Store enums in the combo:

~~~cpp
combo->addItem(QStringLiteral("Auto"), static_cast<int>(StreamQuality::Auto));
combo->addItem(QStringLiteral("Original"), static_cast<int>(StreamQuality::Original));
combo->addItem(QStringLiteral("Super"), static_cast<int>(StreamQuality::Super));
combo->addItem(QStringLiteral("High"), static_cast<int>(StreamQuality::High));
combo->addItem(QStringLiteral("Standard"), static_cast<int>(StreamQuality::Standard));
~~~

Block signals around setChecked and setCurrentIndex during each snapshot refresh. Show Policy active only when requested and effective differ. Reuse rows by room ID, delete only absent rows, and place retained rows in snapshot order. The dock must not calculate primary or effective quality.

- [ ] **Step 4: Run dock tests**

~~~powershell
Push-Location native
ctest --test-dir out/build/windows-x64 --output-on-failure -R room_management_dock_test
Pop-Location
~~~

Expected: validation, capacity, all intent signals, signal-blocked refresh, and fixed feedback pass.

- [ ] **Step 5: Commit the slice**

~~~powershell
git add native/src/app/room_management_dock.* native/tests/room_management_dock_test.cpp native/CMakeLists.txt
git commit -m "feat: add Qt room management dock"
~~~

## Task 3: Bind the dock to MainWindow and the grid

**Files:**
- Modify: native/src/app/main_window.h
- Modify: native/src/app/main_window.cpp
- Modify: native/tests/main_window_test.cpp
- Modify: native/CMakeLists.txt

- [ ] **Step 1: Write failing MainWindow integration tests**

Add:

~~~cpp
void managesRoomsThroughDock();
void reflectsQualityPolicyInDockAfterRoomCountChanges();
~~~

The first test creates MainWindow window(fakeServicePath()), adds 63136 and 63137 through roomManagementDock(), sets 63137 primary with its primaryButton, asserts:

~~~cpp
QCOMPARE(window.playerSurface(), window.surfaceForRoom(QStringLiteral("63137")));
~~~

Then remove 63136 via its row and assert its surface is absent while 63137's pointer remains unchanged.

The policy test adds five rooms through the existing bool compatibility API, selects Super for row 63137, and asserts:

~~~cpp
QCOMPARE(row->findChild<QLabel *>(QStringLiteral("policyLabel"))->text(),
         QStringLiteral("Policy active"));
QCOMPARE(row->findChild<QLabel *>(QStringLiteral("effectiveQualityLabel"))->text(),
         QStringLiteral("Effective: Standard"));
~~~

Remove room 63140 and assert Effective: Super and an empty policy label. Keep all existing local-media, pause, stop, grid, and surface-preservation tests.

- [ ] **Step 2: Verify MainWindow test failure**

~~~powershell
Push-Location native
& cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build out/build/windows-x64 --target main_window_test --parallel 4'
Pop-Location
~~~

Expected: MainWindow::roomManagementDock() and dock wiring are absent.

- [ ] **Step 3: Create the dock container and snapshot refresh**

In main_window.h add:

~~~cpp
RoomManagementDock *roomManagementDock() const noexcept;

private:
    void synchronizeWorkspace();
    QDockWidget *roomDockHost_ = nullptr;
    RoomManagementDock *roomManagementDock_ = nullptr;
~~~

After coordinator construction in main_window.cpp:

~~~cpp
roomDockHost_ = new QDockWidget(QStringLiteral("Room Management"), this);
roomDockHost_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
roomManagementDock_ = new RoomManagementDock(roomDockHost_);
roomDockHost_->setWidget(roomManagementDock_);
addDockWidget(Qt::RightDockWidgetArea, roomDockHost_);
~~~

Replace MainWindow's three coordinator-to-grid connections with:

~~~cpp
connect(coordinator_, &MultiRoomCoordinator::roomSnapshotsChanged,
        this, [this](const RoomSnapshots &) { synchronizeWorkspace(); });
~~~

synchronizeWorkspace() calls rebuildGrid() and then roomManagementDock_->setRooms(coordinator_->roomSnapshots()). Call it once after setup.

- [ ] **Step 4: Route dock signals through detailed coordinator commands**

Use:

~~~cpp
const auto showResult = [this](RoomCommandResult result) {
    if (roomManagementDock_ != nullptr) roomManagementDock_->setCommandResult(result);
};

connect(roomManagementDock_, &RoomManagementDock::addRequested, this,
        [this, showResult](const QString &roomId) {
            showResult(coordinator_->addRoomDetailed(roomId));
        });
connect(roomManagementDock_, &RoomManagementDock::removeRequested, this,
        [this, showResult](const QString &roomId) {
            showResult(coordinator_->removeRoomDetailed(roomId));
        });
connect(roomManagementDock_, &RoomManagementDock::primaryRequested, this,
        [this, showResult](const QString &roomId) {
            showResult(coordinator_->setPrimaryRoomDetailed(roomId));
        });
connect(roomManagementDock_, &RoomManagementDock::requestedQualityChanged, this,
        [this, showResult](const QString &roomId, StreamQuality quality) {
            showResult(coordinator_->setRequestedQuality(roomId, quality));
        });
~~~

MainWindow must not calculate capacity, duplicates, primary status, or quality policy. Its existing bool command APIs stay intact. rebuildGrid() moves only layout items and reuses PlayerSurface instances.

- [ ] **Step 5: Add dock source to application targets**

Add src/app/room_management_dock.cpp to main_window_test and douyu_monitor_native in native/CMakeLists.txt. Do not add browser dependencies.

- [ ] **Step 6: Run M5 focused UI and regression tests**

~~~powershell
Push-Location native
ctest --test-dir out/build/windows-x64 --output-on-failure -R "room_management_dock_test|main_window_test|multi_room_coordinator_test|room_session_test"
Pop-Location
~~~

Expected: dock actions, policy display, local-media controls, grid layout, capacity, and lifecycle coverage pass.

- [ ] **Step 7: Commit the slice**

~~~powershell
git add native/src/app/main_window.* native/tests/main_window_test.cpp native/CMakeLists.txt
git commit -m "feat: manage rooms from the Qt main window"
~~~

## Task 4: Acceptance and durable records

**Files:**
- Modify: docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md

- [ ] **Step 1: Configure and build from MSVC x64**

~~~powershell
$env:QT_ROOT = (Resolve-Path 'D:\Qt\6.8.3\msvc2022_64').Path
$env:MPV_ROOT = (Resolve-Path '.\native\sdk\mpv').Path
Push-Location native
& cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --preset windows-x64 && cmake --build --preset windows-x64-debug --parallel 4'
Pop-Location
~~~

Expected: exit code 0, including room_management_dock_test, main_window_test, and douyu_monitor_native.

- [ ] **Step 2: Run all native and Python tests**

~~~powershell
Push-Location native
ctest --preset windows-x64-debug
.\.venv\Scripts\python.exe -m unittest discover -s service/tests -v
Pop-Location
~~~

Expected: all CTest entries pass and Python tests remain 19/19 or report an explicit increase.

- [ ] **Step 3: Scan captured acceptance output**

Capture configure, build, CTest, and Python output into a temporary file outside the worktree, then run:

~~~powershell
$forbidden = 'playbackUrl|wsAuth|Cookie|token|signature|Traceback|mpv.*(debug|error)'
$matches = Select-String -LiteralPath $acceptanceLog -Pattern $forbidden -CaseSensitive:$false
if ($matches) { throw "Sensitive material found in acceptance output." }
~~~

Expected: no match. Do not commit the temporary file.

- [ ] **Step 4: Verify hygiene and cleanup**

~~~powershell
git diff --check
$processes = Get-Process -ErrorAction SilentlyContinue |
    Where-Object { $_.ProcessName -match 'streamget_service|fake_streamget_service|douyu_monitor_native|room_management_dock_test|main_window_test|multi_room_coordinator_test|room_session_test' }
if ($processes) { throw "M5 verification left a managed process running." }
~~~

Expected: no whitespace error or matching process. Leave unrelated worktree changes unstaged.

- [ ] **Step 5: Compare final code with design and plan**

Record this comparison before declaring M5 complete:

~~~text
1. The right-side Qt dock accepts numeric room IDs and rejects a tenth room.
2. Each row removes a room, selects the primary room, and saves requested quality.
3. The coordinator alone calculates effective quality and publishes snapshots.
4. Five-to-nine room overrides display without overwriting requested quality.
5. Returning to four rooms restores each saved request.
6. No browser runtime, live Douyu request, credential, or raw diagnostic enters test evidence.
~~~

Expected: each line maps to implementation and a fresh test assertion. If a line fails, return to the smallest affected task and add a failing test before the fix.

- [ ] **Step 6: Append verified M5 local evidence**

Append a dated M5 Qt-only Room Management Evidence section to docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md. Include three implementation commits, exact totals, scan and cleanup results, design/plan comparison, and offline/no-credentials limitation. Do not copy raw output.

- [ ] **Step 7: Create and reread a new Notion M5 acceptance page**

Create DouyuMonitor M5 实施进度与验收 after local logging. Include the same scope, commits, counts, comparison, and offline limitation. Fetch it after creation and correct any mismatch before proceeding.

- [ ] **Step 8: Commit evidence**

~~~powershell
git add docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md
git commit -m "test: verify Qt-only M5 room management"
~~~

## Plan self-review

- **Spec coverage:** Task 1 establishes one snapshot source, fixed command results, requested-quality retention, and quality restoration. Task 2 delivers the management controls, validation, display, fixed feedback, tooltips, accessible actions, and no echo on programmatic refresh. Task 3 synchronizes dock and grid through the coordinator. Task 4 covers build, tests, safety scan, cleanup, design/plan comparison, local evidence, and a new reread Notion page.
- **Unresolved-marker scan:** The plan defines every named type, command, error result, test target, validation action, and verification command.
- **Type consistency:** RoomCommandResult, RoomSnapshot, RoomSnapshots, setRequestedQuality, and roomSnapshotsChanged use one spelling and ownership model throughout. The dock reads snapshots; only the coordinator decides effective quality.
- **Scope check:** Direct room-ID management is the sole M5 feature. Search, metadata, persistence, playback retry, live network smoke testing, browser runtimes, and Electron compatibility stay outside this milestone.
