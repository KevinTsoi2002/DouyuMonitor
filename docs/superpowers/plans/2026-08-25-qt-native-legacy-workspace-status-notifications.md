# Qt Native Legacy Workspace, Live Status, and Notifications Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** Replace the M5 right-side room-management dock with a persistent legacy-style Qt room workspace, then add per-room live status refresh, automatic replay, and Windows notifications for live and playback transitions.

**Architecture:** MultiRoomCoordinator remains the only owner of active RoomSession instances and effective-quality policy. NativeWorkspaceStore owns durable, non-sensitive room-library state. RoomStatusScheduler owns timed metadata checks and calls the existing long-lived StreamGet client through callbacks. NotificationPolicy stays pure; WindowsNotificationService is the only Qt platform adapter.

**Tech Stack:** C++20, Qt 6 Core/Gui/Widgets/OpenGLWidgets/Test, QSettings, QSystemTrayIcon, libmpv, CMake/Ninja/MSVC, QtTest, and the existing fake StreamGet child.

---

## Build Environment

Before running any CMake command in this plan, set the SDK roots in the same PowerShell session:

~~~powershell
$env:QT_ROOT = 'D:\Qt\6.8.3\msvc2022_64'
$env:MPV_ROOT = (Resolve-Path 'native\sdk\mpv').Path
cmake -S native --preset windows-x64
~~~

The preset file is native/CMakePresets.json; its configured build tree is
native/out/build/windows-x64. Every build and CTest command below relies on this configured
tree. Do not use the unavailable root-level msvc-x64-debug preset.

## Scope and File Map

Create:

- native/src/workspace/native_workspace_types.h/.cpp: durable room-library, group, history, audio-focus, and active-workspace value types plus safe JSON codecs.
- native/src/workspace/native_workspace_store.h/.cpp: QSettings-backed load/save boundary with a versioned JSON payload.
- native/src/workspace/room_status_scheduler.h/.cpp: independent per-room scheduling, retry backoff, request mapping, and late-response protection.
- native/src/workspace/notification_policy.h/.cpp: pure transition detection, 5-minute dedupe, and six-events-per-minute rate limit.
- native/src/app/room_sidebar.h/.cpp: left-side current/favorites/history views, group tabs, add action, reordering controls, primary/audio/favorite/quality/remove commands, and compact status presentation.
- native/src/app/add_room_dialog.h/.cpp: validated room-ID entry dialog opened by the toolbar or sidebar add icon.
- native/src/app/group_manager_dialog.h/.cpp: create, rename, delete, and assign rooms to groups.
- native/src/app/notification_settings_dialog.h/.cpp: total notification switch and four event switches.
- native/src/app/windows_notification_service.h/.cpp: QSystemTrayIcon-backed delivery adapter and QSettings preference persistence.
- native/tests/native_workspace_store_test.cpp, native/tests/room_status_scheduler_test.cpp, native/tests/notification_policy_test.cpp, native/tests/room_sidebar_test.cpp, native/tests/windows_notification_service_test.cpp.

Modify:

- native/src/workspace/room_workspace_types.h: extend UI-safe room snapshots with immutable display metadata, live status, playback health, favorite/audio flags, and requested quality.
- native/src/workspace/room_session.h/.cpp: keep validated room metadata, map ROOM_OFFLINE to live status, expose playback health, and add a muted-player command.
- native/src/workspace/multi_room_coordinator.h/.cpp: expose library commands, restore active rooms, own the scheduler and policy, publish one ordered snapshot, and emit safe notification events.
- native/src/media/player_surface.h/.cpp: add setMuted(bool) and isMuted() using libmpv's mute property without storing source URLs.
- native/src/app/main_window.h/.cpp: own the sidebar, dialogs, workspace store, tray notifier, and status summary; remove the default visible M5 dock.
- native/src/app/main_window.cpp: build a QToolBar plus QSplitter shell, route sidebar commands to the coordinator, restore before the first show, and save after accepted mutations.
- native/tests/fake_streamget_service.cpp: support deterministic numeric-search responses and scripts for online/offline/status-refresh tests.
- native/tests/room_session_test.cpp, native/tests/multi_room_coordinator_test.cpp, native/tests/main_window_test.cpp, and native/CMakeLists.txt.
- docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md: append phase evidence after each accepted verification gate.

Do not modify legacy Electron/TypeScript files. Do not change StreamGet's production request contract in this milestone. Do not add a network client beyond StreamgetProcessClient, background process, close-to-tray behavior, WebEngine, WebView, Chromium, Node, React, or TypeScript. Do not persist or log a playback URL, Cookie, token, signature, header, traceback, or raw libmpv diagnostic.

## Contracts

native/src/workspace/native_workspace_types.h must define data that can be safely persisted and tested:

~~~cpp
enum class RoomLiveStatus { Unknown, Online, Offline };
enum class RoomPlaybackHealth { Pending, Playing, Error };

struct RoomMetadata {
    QString roomId;
    QString anchorName;
    QString title;
    QString category;
    QString viewerLabel;
    QUrl avatarUrl;
    bool operator==(const RoomMetadata &) const = default;
};

struct NativeRoomRecord {
    QString roomId;
    RoomMetadata metadata;
    StreamQuality requestedQuality = StreamQuality::Auto;
    bool favorite = false;
    qint64 lastOpenedAtMs = 0;
    bool operator==(const NativeRoomRecord &) const = default;
};

struct NativeRoomGroup {
    QString id;
    QString name;
    QStringList roomIds;
    bool operator==(const NativeRoomGroup &) const = default;
};

struct NativeWorkspaceSnapshot {
    int version = 1;
    QVector<NativeRoomRecord> library;
    QVector<NativeRoomGroup> groups;
    QStringList activeRoomIds;
    QString activeGroupId;
    QString primaryRoomId;
    QString audioRoomId;
    bool operator==(const NativeWorkspaceSnapshot &) const = default;
};
~~~

NativeWorkspaceStore stores one JSON object under DouyuMonitor/nativeWorkspaceV1. It rejects malformed records, drops duplicate IDs, limits active rooms and group members to MultiRoomCoordinator::kMaxRooms, and preserves no field outside NativeWorkspaceSnapshot.

RoomSnapshot gains these safe fields:

~~~cpp
RoomMetadata metadata;
RoomLiveStatus liveStatus = RoomLiveStatus::Unknown;
RoomPlaybackHealth playbackHealth = RoomPlaybackHealth::Pending;
bool favorite = false;
bool audioFocused = false;
bool muted = true;
~~~

RoomStatusScheduler has an injected transport and deterministic timing configuration:

~~~cpp
struct RoomRefreshTiming {
    int onlineIntervalMs = 60'000;
    int offlineIntervalMs = 120'000;
    QVector<int> retryDelaysMs{30'000, 60'000, 120'000, 240'000};
};

class RoomStatusScheduler final : public QObject {
    Q_OBJECT
public:
    using StartSearch = std::function<quint64(const QString &roomId)>;
    using CancelRequest = std::function<void(quint64 requestId)>;

    explicit RoomStatusScheduler(StartSearch startSearch, CancelRequest cancelRequest,
                                 RoomRefreshTiming timing = {}, QObject *parent = nullptr);
    void synchronize(const QVector<RoomSnapshot> &rooms);
    void requestNow(const QString &roomId);
    std::optional<QString> takeCompletedRequest(quint64 requestId, bool succeeded);
    void stop();
};
~~~

takeCompletedRequest() returns a room ID only once, clears the room's in-flight state, and schedules its next timer. synchronize() destroys timers and cancels in-flight requests for removed rooms. It never inspects playback URLs or parses service payloads.

NotificationPolicy is a pure class with a controllable time source:

~~~cpp
enum class NotificationEventType { RoomOnline, RoomOffline, PlaybackFailed, PlaybackRecovered };

struct NotificationEvent {
    NotificationEventType type;
    QString roomId;
    QString anchorName;
    QString title;
    QString body;
};

class NotificationPolicy final {
public:
    explicit NotificationPolicy(std::function<qint64()> nowMs);
    QVector<NotificationEvent> update(const RoomSnapshots &snapshots);
    void resetBaseline();
};
~~~

WindowsNotificationService persists this exact preference object in QSettings:

~~~cpp
struct NotificationPreferences {
    bool enabled = true;
    bool roomOnline = true;
    bool roomOffline = true;
    bool playbackFailed = true;
    bool playbackRecovered = true;
};
~~~

RoomSidebar exposes a testable row type instead of requiring tests to inspect layout order:

~~~cpp
class RoomSidebarRow final : public QWidget {
    Q_OBJECT
public:
    QString statusText() const;
    bool hasPlaybackWarning() const;
    QToolButton *primaryButton() const noexcept;
    QToolButton *audioButton() const noexcept;
    QToolButton *favoriteButton() const noexcept;
    QComboBox *qualityCombo() const noexcept;
    QToolButton *removeButton() const noexcept;
};

class RoomSidebar final : public QWidget {
    Q_OBJECT
public:
    RoomSidebarRow *rowForRoom(const QString &roomId) const noexcept;
};
~~~

MultiRoomCoordinator gains refreshRoomStatusNow(const QString &roomId) for the sidebar's
manual refresh action and deterministic integration tests. Its constructor accepts an optional
RoomRefreshTiming argument whose production default is the contract above.

## Phase 1: Native Room Workspace

### Task 1: Add durable workspace types and QSettings persistence

**Files:**
- Create: native/src/workspace/native_workspace_types.h
- Create: native/src/workspace/native_workspace_types.cpp
- Create: native/src/workspace/native_workspace_store.h
- Create: native/src/workspace/native_workspace_store.cpp
- Create: native/tests/native_workspace_store_test.cpp
- Modify: native/CMakeLists.txt

- [x] **Step 1: Write failing persistence tests.**

~~~cpp
void NativeWorkspaceStoreTest::roundTripsSafeWorkspaceState()
{
    QTemporaryDir dir;
    QSettings settings(dir.filePath("workspace.ini"), QSettings::IniFormat);
    NativeWorkspaceStore store(&settings);
    const NativeWorkspaceSnapshot expected = fixtureWorkspace();

    QVERIFY(store.save(expected));
    QCOMPARE(store.load(), expected);
}

void NativeWorkspaceStoreTest::rejectsMalformedAndSensitiveValues()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "test", "workspace");
    settings.setValue("DouyuMonitor/nativeWorkspaceV1",
        R"({"library":[{"roomId":"63136","playbackUrl":"https://secret.invalid"}]})");
    NativeWorkspaceStore store(&settings);
    QVERIFY(store.load().library.isEmpty());
}
~~~

- [x] **Step 2: Run the focused test and confirm it fails because the store is absent.**

Run: cmake --build native/out/build/windows-x64 --target native_workspace_store_test; ctest --test-dir native/out/build/windows-x64 -R native_workspace_store_test --output-on-failure

Expected: build failure that names native_workspace_store_test or missing NativeWorkspaceStore.

- [x] **Step 3: Implement value validation and storage.**

Use QJsonDocument and QJsonObject. Write only accepted room metadata, quality, favorite flag, timestamp, groups, active order, primary ID, and audio ID. Keep RoomMetadata::avatarUrl only when the existing safe HTTP rule accepts it. Normalize duplicate room IDs by keeping the first and drop invalid references from groups and active order.

~~~cpp
bool NativeWorkspaceStore::save(const NativeWorkspaceSnapshot &snapshot)
{
    settings_->setValue(kSettingsKey,
        QJsonDocument(toJson(normalize(snapshot))).toJson(QJsonDocument::Compact));
    settings_->sync();
    return settings_->status() == QSettings::NoError;
}

NativeWorkspaceSnapshot NativeWorkspaceStore::load() const
{
    const auto document = QJsonDocument::fromJson(settings_->value(kSettingsKey).toByteArray());
    return document.isObject() ? fromJson(document.object()) : NativeWorkspaceSnapshot{};
}
~~~

- [x] **Step 4: Register the target and run persistence tests.**

Add native_workspace_store_test to native/CMakeLists.txt, link Qt6::Core and Qt6::Test, then run the command from Step 2.

Expected: 100% tests passed, 0 tests failed out of 1.

- [ ] **Step 5: Commit the persistence boundary.**

~~~powershell
git add native/src/workspace/native_workspace_types.* native/src/workspace/native_workspace_store.* native/tests/native_workspace_store_test.cpp native/CMakeLists.txt
git commit -m "feat: persist native room workspace"
~~~

### Task 2: Make sessions and coordinator expose metadata, live state, favorite, and audio focus

**Files:**
- Modify: native/src/workspace/room_workspace_types.h
- Modify: native/src/workspace/room_session.h
- Modify: native/src/workspace/room_session.cpp
- Modify: native/src/workspace/multi_room_coordinator.h
- Modify: native/src/workspace/multi_room_coordinator.cpp
- Modify: native/src/media/player_surface.h
- Modify: native/src/media/player_surface.cpp
- Modify: native/tests/room_session_test.cpp
- Modify: native/tests/multi_room_coordinator_test.cpp
- Modify: native/tests/player_surface_test.cpp

- [ ] **Step 1: Write failing session/coordinator tests.**

~~~cpp
void RoomSessionTest::mapsOfflineResolveToLiveOfflineWithoutPlaybackFailure()
{
    RoomSession session(&offlineClient, "63136", StreamQuality::Auto, &host);
    QSignalSpy failures(&session, &RoomSession::failed);
    session.resolve();
    QTRY_COMPARE(session.liveStatus(), RoomLiveStatus::Offline);
    QCOMPARE(session.playbackHealth(), RoomPlaybackHealth::Pending);
    QCOMPARE(failures.count(), 0);
}

void MultiRoomCoordinatorTest::appliesSingleAudioFocusAndPublishesIt()
{
    coordinator.addRoom("63136");
    coordinator.addRoom("63137");
    QVERIFY(coordinator.setAudioFocus("63137"));
    const RoomSnapshots snapshots = coordinator.roomSnapshots();
    QVERIFY(!snapshots.at(0).audioFocused);
    QVERIFY(snapshots.at(1).audioFocused);
}
~~~

- [ ] **Step 2: Run focused tests and confirm the missing APIs fail.**

Run: cmake --build native/out/build/windows-x64 --target room_session_test multi_room_coordinator_test player_surface_test; ctest --test-dir native/out/build/windows-x64 -R "room_session_test|multi_room_coordinator_test|player_surface_test" --output-on-failure

Expected: compile failures for liveStatus, playbackHealth, setAudioFocus, or setMuted.

- [ ] **Step 3: Add the smallest coordinated state model.**

RoomSession::applyMetadata(const RoomSearchResult&) copies only protocol-validated fields. RoomSession::onControllerFailed("ROOM_OFFLINE") calls setLiveStatus(Offline), stops the PlayerSurface, sets health to Pending, and does not emit failed. All other error codes set health to Error and emit the fixed code. onControllerSourceReady() sets Online and Playing only after PlayerSurface::loadSource() succeeds.

~~~cpp
bool PlayerSurface::setMuted(bool muted)
{
    if (mpv_ == nullptr) return false;
    if (mpv_set_property_string(mpv_, "mute", muted ? "yes" : "no") < 0) return false;
    muted_ = muted;
    return true;
}

void MultiRoomCoordinator::applyAudioFocus()
{
    for (const QString &roomId : order_) {
        RoomSession *session = sessions_.value(roomId);
        const bool focused = !audioRoomId_.isEmpty() && roomId == audioRoomId_;
        session->surface()->setMuted(!focused);
        session->setAudioFocused(focused);
    }
}
~~~

Extend RoomSnapshot rather than allowing widgets to read sessions directly. Preserve current M4 quality logic and its 9-room limit.

- [ ] **Step 4: Run focused tests and the M4 regression suite.**

Run: ctest --test-dir native/out/build/windows-x64 -R "room_session_test|multi_room_coordinator_test|player_surface_test|quality_policy_test" --output-on-failure

Expected: all selected tests pass.

- [ ] **Step 5: Commit the coordinator state expansion.**

~~~powershell
git add native/src/workspace/room_workspace_types.h native/src/workspace/room_session.* native/src/workspace/multi_room_coordinator.* native/src/media/player_surface.* native/tests/room_session_test.cpp native/tests/multi_room_coordinator_test.cpp native/tests/player_surface_test.cpp
git commit -m "feat: expose native room live state"
~~~

### Task 3: Replace the visible M5 dock with the legacy-style left sidebar and native group controls

**Files:**
- Create: native/src/app/room_sidebar.h
- Create: native/src/app/room_sidebar.cpp
- Create: native/src/app/add_room_dialog.h
- Create: native/src/app/add_room_dialog.cpp
- Create: native/src/app/group_manager_dialog.h
- Create: native/src/app/group_manager_dialog.cpp
- Create: native/tests/room_sidebar_test.cpp
- Modify: native/src/app/main_window.h
- Modify: native/src/app/main_window.cpp
- Modify: native/tests/main_window_test.cpp
- Modify: native/CMakeLists.txt

- [ ] **Step 1: Write failing widget tests for status and actions.**

~~~cpp
void RoomSidebarTest::rendersLiveAndPlaybackStatesWithoutDiagnostics()
{
    RoomSidebar sidebar;
    sidebar.setRooms({onlinePlaying("63136", "主播 A"), offlineRoom("63137", "主播 B"),
                      onlineError("63138", "主播 C")});
    QCOMPARE(sidebar.rowForRoom("63136")->statusText(), "直播中");
    QCOMPARE(sidebar.rowForRoom("63137")->statusText(), "未开播");
    QVERIFY(sidebar.rowForRoom("63138")->hasPlaybackWarning());
    QVERIFY(!sidebar.text().contains("https://"));
}

void RoomSidebarTest::emitsOneIntentForEachRoomAction()
{
    sidebar.setRooms({onlinePlaying("63136", "主播 A")});
    QSignalSpy primary(&sidebar, &RoomSidebar::primaryRequested);
    QSignalSpy audio(&sidebar, &RoomSidebar::audioFocusRequested);
    QSignalSpy favorite(&sidebar, &RoomSidebar::favoriteToggled);
    QSignalSpy quality(&sidebar, &RoomSidebar::requestedQualityChanged);
    QSignalSpy remove(&sidebar, &RoomSidebar::removeRequested);
    RoomSidebarRow *row = sidebar.rowForRoom("63136");
    row->primaryButton()->click();
    row->audioButton()->click();
    row->favoriteButton()->click();
    row->qualityCombo()->setCurrentIndex(row->qualityCombo()->findData(int(StreamQuality::High)));
    row->removeButton()->click();
    QCOMPARE(primary.takeFirst().at(0).toString(), QStringLiteral("63136"));
    QCOMPARE(audio.takeFirst().at(0).toString(), QStringLiteral("63136"));
    QCOMPARE(favorite.takeFirst().at(0).toString(), QStringLiteral("63136"));
    QCOMPARE(quality.takeFirst().at(0).toString(), QStringLiteral("63136"));
    QCOMPARE(remove.takeFirst().at(0).toString(), QStringLiteral("63136"));
}
~~~

- [ ] **Step 2: Run the widget test and confirm it fails.**

Run: cmake --build native/out/build/windows-x64 --target room_sidebar_test; ctest --test-dir native/out/build/windows-x64 -R room_sidebar_test --output-on-failure

Expected: build failure because RoomSidebar does not exist.

- [ ] **Step 3: Implement the sidebar and group dialog.**

Use a fixed-width QWidget inside a horizontal QSplitter, not a floating QDockWidget. The top row contains the title and icon-only add button. AddRoomDialog uses a single QLineEdit with the existing 1-to-20-digit validator, disables confirm until valid input, and reports only fixed validation results. Use QTabBar for current, favorites, and history; group tabs show up to three groups plus a menu button. Each room row has a fixed height, avatar or initial fallback, explicit status text, primary button, favorite button, audio-focus button, requested-quality QComboBox, move-up/down buttons, and remove icon button with tooltips and accessible names.

~~~cpp
signals:
    void addRequested(QString roomId);
    void primaryRequested(QString roomId);
    void audioFocusRequested(QString roomId);
    void favoriteToggled(QString roomId);
    void requestedQualityChanged(QString roomId, StreamQuality quality);
    void moveRequested(QString roomId, int delta);
    void reorderRequested(QString sourceRoomId, QString targetRoomId);
    void removeRequested(QString roomId);
    void groupSwitchRequested(QString groupId);
    void manageGroupsRequested();
~~~

GroupManagerDialog uses QListWidget plus icon-only add/remove controls, validates group names as trimmed non-empty strings of at most 30 Unicode code points, and emits create/rename/delete/assign intents. It does not own persistence or sessions.

Enable drag and drop on the current room rows. A drag carries the source room ID in \`application/x-douyu-room-id\`; a drop emits \`reorderRequested(sourceRoomId, targetRoomId)\` only when both IDs are active and differ. The coordinator applies the move, publishes its ordered snapshot, and the sidebar rebuilds the rows from that snapshot. Move-up and move-down buttons use the same coordinator order mutation for keyboard users.

- [ ] **Step 4: Route sidebar commands through MainWindow and prove the M5 dock is absent.**

MainWindow builds a non-movable toolbar and places RoomSidebar at the left of the grid. The toolbar retains pause and stop, adds an icon-only room-add action that opens AddRoomDialog, and adds an icon-only notification-settings action. Do not call addDockWidget() or create RoomManagementDock in the product executable. Keep RoomManagementDock test target temporarily so existing M5 tests remain valid, but remove it from douyu_monitor_native and main_window_test source lists after sidebar coverage replaces it.

~~~cpp
splitter_ = new QSplitter(Qt::Horizontal, this);
roomSidebar_ = new RoomSidebar(splitter_);
splitter_->addWidget(roomSidebar_);
splitter_->addWidget(gridHost_);
splitter_->setStretchFactor(0, 0);
splitter_->setStretchFactor(1, 1);
setCentralWidget(splitter_);
~~~

- [ ] **Step 5: Run sidebar and MainWindow UI tests.**

Run: ctest --test-dir native/out/build/windows-x64 -R "room_sidebar_test|main_window_test|room_management_dock_test" --output-on-failure

Expected: all tests pass. main_window_test asserts that a RoomSidebar exists and no dock widget titled Room Management exists.

- [ ] **Step 6: Commit the visible workspace shell.**

~~~powershell
git add native/src/app/room_sidebar.* native/src/app/add_room_dialog.* native/src/app/group_manager_dialog.* native/src/app/main_window.* native/tests/room_sidebar_test.cpp native/tests/main_window_test.cpp native/CMakeLists.txt
git commit -m "feat: add native room sidebar workspace"
~~~

### Task 4: Restore persistent groups, favorites, history, order, primary room, audio focus, and requested quality

**Files:**
- Modify: native/src/workspace/native_workspace_store.h
- Modify: native/src/workspace/native_workspace_store.cpp
- Modify: native/src/workspace/multi_room_coordinator.h
- Modify: native/src/workspace/multi_room_coordinator.cpp
- Modify: native/src/app/main_window.h
- Modify: native/src/app/main_window.cpp
- Modify: native/tests/native_workspace_store_test.cpp
- Modify: native/tests/multi_room_coordinator_test.cpp
- Modify: native/tests/main_window_test.cpp

- [ ] **Step 1: Write failing restoration and library-action tests.**

~~~cpp
void MainWindowTest::restoresRoomsAndReplaysWithoutNotifyingHistory()
{
    writeWorkspaceFixture(settingsPath, {"63136", "63137"}, "63137", "63136");
    MainWindow window(fakeServicePath(), settingsPath);
    QCOMPARE(window.roomIds(), QStringList({"63136", "63137"}));
    QCOMPARE(window.primaryRoomId(), QStringLiteral("63137"));
    QCOMPARE(window.audioRoomId(), QStringLiteral("63136"));
    QTRY_VERIFY(window.surfaceForRoom("63136")->playbackState() != PlayerSurface::PlaybackState::Idle);
}

void MultiRoomCoordinatorTest::switchingGroupReplacesActiveSetAndKeepsLibrary()
{
    coordinator.restoreWorkspace(fixtureWithTwoGroups());
    QVERIFY(coordinator.switchGroup("events"));
    QCOMPARE(coordinator.roomIds(), QStringList({"63138", "63139"}));
    QVERIFY(coordinator.libraryContains("63136"));
}
~~~

- [ ] **Step 2: Run restoration tests and confirm the APIs are absent.**

Run: cmake --build native/out/build/windows-x64 --target native_workspace_store_test multi_room_coordinator_test main_window_test; ctest --test-dir native/out/build/windows-x64 -R "native_workspace_store_test|multi_room_coordinator_test|main_window_test" --output-on-failure

Expected: compile failure for restoreWorkspace, switchGroup, primaryRoomId, or audioRoomId accessors.

- [ ] **Step 3: Implement restore and save sequencing.**

Load the workspace in MainWindow before the first show. The coordinator restores active room IDs in stored order, clamps to nine, applies requested quality and primary/audio focus after all sessions exist, then resolves each retained room. A successful sidebar mutation updates the in-memory library first, publishes a new snapshot, then MainWindow persists the normalized snapshot. Failed command results do not alter memory or disk.

History records one timestamp per room, moves the accessed room to the front, and keeps at most 50 entries. Favorite/history views only add a selected library room to the current active group if it is not already active and the room limit permits it. Removing a current room stops and removes its session but leaves its library, favorite, and history records intact.

- [ ] **Step 4: Run restore, sidebar, and coordinator tests.**

Run: ctest --test-dir native/out/build/windows-x64 -R "native_workspace_store_test|room_sidebar_test|multi_room_coordinator_test|main_window_test" --output-on-failure

Expected: all selected tests pass, including restart auto-resolution through the fake child.

- [ ] **Step 5: Write phase-one local evidence and a new Notion page.**

Append a dated **M6 Phase 1 Native Workspace Evidence** section to docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md with the commits, selected test totals, supported persistence fields, scope comparison, and no-live-network limitation. Create a new Notion page titled DouyuMonitor M6 阶段 1 原生工作区验收, write the same concise evidence, fetch it, and correct any mismatch before starting Task 5.

- [ ] **Step 6: Commit the restore phase and evidence.**

~~~powershell
git add native/src/workspace/native_workspace_store.* native/src/workspace/multi_room_coordinator.* native/src/app/main_window.* native/tests/native_workspace_store_test.cpp native/tests/multi_room_coordinator_test.cpp native/tests/main_window_test.cpp docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md
git commit -m "feat: restore native room workspace"
~~~

## Phase 2: Live Status and Automatic Playback

### Task 5: Add deterministic room status scheduling and fake metadata responses

**Files:**
- Create: native/src/workspace/room_status_scheduler.h
- Create: native/src/workspace/room_status_scheduler.cpp
- Create: native/tests/room_status_scheduler_test.cpp
- Modify: native/tests/fake_streamget_service.cpp
- Modify: native/CMakeLists.txt

- [ ] **Step 1: Write failing scheduler tests with millisecond timing.**

~~~cpp
void RoomStatusSchedulerTest::usesOnlineOfflineIntervalsAndNoConcurrentRequests()
{
    QVector<QString> calls;
    RoomRefreshTiming timing{.onlineIntervalMs = 20, .offlineIntervalMs = 40,
                             .retryDelaysMs = {5, 10, 20, 40}};
    RoomStatusScheduler scheduler(
        [&calls](const QString &id) { calls.push_back(id); return calls.size(); },
        [](quint64) {}, timing);
    scheduler.synchronize({onlineSnapshot("63136"), offlineSnapshot("63137")});
    scheduler.requestNow("63136");
    scheduler.requestNow("63136");
    QCOMPARE(calls.count("63136"), 1);
    QCOMPARE(scheduler.takeCompletedRequest(1, true), std::optional<QString>{"63136"});
}

void RoomStatusSchedulerTest::cancelsRemovedRoomsAndRejectsLateResponse()
{
    QVector<quint64> cancelled;
    const RoomRefreshTiming timing{.onlineIntervalMs = 20, .offlineIntervalMs = 40,
                                   .retryDelaysMs = {5, 10, 20, 40}};
    RoomStatusScheduler scheduler([](const QString &) { return quint64{1}; },
                                  [&cancelled](quint64 requestId) { cancelled.push_back(requestId); },
                                  timing);
    scheduler.synchronize({onlineSnapshot("63136")});
    scheduler.requestNow("63136");
    scheduler.synchronize({});
    QCOMPARE(cancelled, QVector<quint64>{1});
    QVERIFY(!scheduler.takeCompletedRequest(1, true).has_value());
}
~~~

- [ ] **Step 2: Run the scheduler test and confirm it fails.**

Run: cmake --build native/out/build/windows-x64 --target room_status_scheduler_test; ctest --test-dir native/out/build/windows-x64 -R room_status_scheduler_test --output-on-failure

Expected: build failure because RoomStatusScheduler does not exist.

- [ ] **Step 3: Implement scheduler ownership and fake search scripting.**

Each scheduler record owns one single-shot QTimer, a failure count, a generation, and one client request ID. Start callbacks receive only room IDs. Use a deterministic hash of the room ID bounded to 10% of the base interval for production jitter; the tests pass zero jitter through the timing constructor. Treat response/failure completion as success/failure only after request ID and generation match.

Extend fake_streamget_service with --search-script online,offline,online and emit one valid results entry for numeric queries. Its result must include fixed synthetic metadata and online; it must never print a source URL in the search response.

- [ ] **Step 4: Run scheduler and protocol/client regression tests.**

Run: ctest --test-dir native/out/build/windows-x64 -R "room_status_scheduler_test|stream_service_protocol_test|streamget_process_client_test" --output-on-failure

Expected: all selected tests pass.

- [ ] **Step 5: Commit scheduler infrastructure.**

~~~powershell
git add native/src/workspace/room_status_scheduler.* native/tests/room_status_scheduler_test.cpp native/tests/fake_streamget_service.cpp native/CMakeLists.txt
git commit -m "feat: schedule native room status refresh"
~~~

### Task 6: Route metadata refresh transitions through the coordinator and sidebar

**Files:**
- Modify: native/src/workspace/multi_room_coordinator.h
- Modify: native/src/workspace/multi_room_coordinator.cpp
- Modify: native/src/workspace/room_session.h
- Modify: native/src/workspace/room_session.cpp
- Modify: native/src/app/room_sidebar.cpp
- Modify: native/tests/multi_room_coordinator_test.cpp
- Modify: native/tests/room_session_test.cpp
- Modify: native/tests/room_sidebar_test.cpp

- [ ] **Step 1: Write failing online/offline transition tests.**

~~~cpp
void MultiRoomCoordinatorTest::replaysWhenMetadataChangesOfflineToOnline()
{
    QWidget host;
    StreamgetProcessClient client(fakeServicePath(),
                                  {"--search-script", "offline,online"});
    const RoomRefreshTiming timing{.onlineIntervalMs = 20, .offlineIntervalMs = 20,
                                   .retryDelaysMs = {5, 10, 20, 40}};
    MultiRoomCoordinator coordinator(&client, &host, timing);
    coordinator.addRoom("63136");
    coordinator.refreshRoomStatusNow("63136");
    QTRY_COMPARE(coordinator.roomSnapshots().at(0).liveStatus, RoomLiveStatus::Offline);
    RoomSession *session = coordinator.sessionForRoom("63136");
    QSignalSpy stateChanges(session, &RoomSession::stateChanged);
    coordinator.refreshRoomStatusNow("63136");
    QTRY_COMPARE(coordinator.roomSnapshots().at(0).liveStatus, RoomLiveStatus::Online);
    QTRY_VERIFY(session->state() == RoomSession::State::Ready);
    QVERIFY(stateChanges.count() >= 2);
}

void MultiRoomCoordinatorTest::stopsWhenMetadataChangesOnlineToOffline()
{
    QWidget host;
    StreamgetProcessClient client(fakeServicePath(), {"--search-script", "online,offline"});
    const RoomRefreshTiming timing{.onlineIntervalMs = 20, .offlineIntervalMs = 20,
                                   .retryDelaysMs = {5, 10, 20, 40}};
    MultiRoomCoordinator coordinator(&client, &host, timing);
    QSignalSpy removed(&coordinator, &MultiRoomCoordinator::roomRemoved);
    coordinator.addRoom("63136");
    coordinator.refreshRoomStatusNow("63136");
    QTRY_COMPARE(coordinator.roomSnapshots().at(0).liveStatus, RoomLiveStatus::Online);
    coordinator.refreshRoomStatusNow("63136");
    QTRY_COMPARE(coordinator.surfaceForRoom("63136")->playbackState(),
                 PlayerSurface::PlaybackState::Idle);
    QCOMPARE(coordinator.roomIds(), QStringList({"63136"}));
    QCOMPARE(removed.count(), 0);
}
~~~

- [ ] **Step 2: Run transition tests and confirm they fail.**

Run: cmake --build native/out/build/windows-x64 --target multi_room_coordinator_test room_session_test room_sidebar_test; ctest --test-dir native/out/build/windows-x64 -R "multi_room_coordinator_test|room_session_test|room_sidebar_test" --output-on-failure

Expected: the new status-transition assertions fail.

- [ ] **Step 3: Implement safe response routing.**

Connect the shared StreamgetProcessClient response and failure signals once in the coordinator. Pass only request IDs owned by RoomStatusScheduler to metadata handling; all other responses continue through RemotePlaybackController. A numeric-search response must contain exactly one matching room entry. Apply metadata and live status only when the room still exists. Online to Offline calls RoomSession::stop(); Offline to Online calls resolve() with the current effective quality. Publish after each accepted state mutation.

Do not call resolve() on unchanged online refreshes. A failed metadata request keeps the previous known status; an initially unknown room stays Unknown.

- [ ] **Step 4: Run phase-two component tests.**

Run: ctest --test-dir native/out/build/windows-x64 -R "room_status_scheduler_test|multi_room_coordinator_test|room_session_test|room_sidebar_test|main_window_test" --output-on-failure

Expected: all selected tests pass.

- [ ] **Step 5: Write phase-two local evidence and a new Notion page.**

Append **M6 Phase 2 Live Status Evidence** with the interval/backoff test cases, online/offline replay assertions, commits, design comparison, and offline-only limitation. Create and reread a new Notion page titled DouyuMonitor M6 阶段 2 直播状态验收; correct any mismatch before Task 7.

- [ ] **Step 6: Commit live status integration and evidence.**

~~~powershell
git add native/src/workspace/multi_room_coordinator.* native/src/workspace/room_session.* native/src/app/room_sidebar.cpp native/tests/multi_room_coordinator_test.cpp native/tests/room_session_test.cpp native/tests/room_sidebar_test.cpp docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md
git commit -m "feat: refresh native room live status"
~~~

## Phase 3: Windows Notifications and Release Evidence

### Task 7: Add pure notification policy and Windows notification settings

**Files:**
- Create: native/src/workspace/notification_policy.h
- Create: native/src/workspace/notification_policy.cpp
- Create: native/src/app/windows_notification_service.h
- Create: native/src/app/windows_notification_service.cpp
- Create: native/src/app/notification_settings_dialog.h
- Create: native/src/app/notification_settings_dialog.cpp
- Create: native/tests/notification_policy_test.cpp
- Create: native/tests/windows_notification_service_test.cpp
- Modify: native/src/app/main_window.h
- Modify: native/src/app/main_window.cpp
- Modify: native/CMakeLists.txt

- [ ] **Step 1: Write failing notification policy tests.**

~~~cpp
void NotificationPolicyTest::usesFirstSnapshotAsBaseline()
{
    NotificationPolicy policy([clock = qint64{1'000}] { return clock; });
    QCOMPARE(policy.update({offlineRoom("63136")}).size(), 0);
    QCOMPARE(policy.update({onlineRoom("63136")}).at(0).type,
             NotificationEventType::RoomOnline);
}

void NotificationPolicyTest::deduplicatesAndLimitsEvents()
{
    qint64 now = 1'000;
    NotificationPolicy policy([&now] { return now; });
    policy.update({playingRoom("63136")});
    QCOMPARE(policy.update({failedRoom("63136", "SERVICE_FAILED")}).size(), 1);
    now += 1'000;
    QCOMPARE(policy.update({playingRoom("63136")}).size(), 1);
    QCOMPARE(policy.update({failedRoom("63136", "SERVICE_FAILED")}).size(), 0);

    policy.resetBaseline();
    policy.update({offlineRoom("1"), offlineRoom("2"), offlineRoom("3"), offlineRoom("4"),
                   offlineRoom("5"), offlineRoom("6"), offlineRoom("7")});
    const QVector<NotificationEvent> events = policy.update(
        {onlineRoom("1"), onlineRoom("2"), onlineRoom("3"), onlineRoom("4"),
         onlineRoom("5"), onlineRoom("6"), onlineRoom("7")});
    QCOMPARE(events.size(), 6);
}

void NotificationPolicyTest::doesNotTreatOfflineAsPlaybackFailure()
{
    NotificationPolicy policy([] { return qint64{1'000}; });
    policy.update({playingRoom("63136")});
    const QVector<NotificationEvent> events = policy.update({offlineRoom("63136")});
    QCOMPARE(events.size(), 1);
    QCOMPARE(events.front().type, NotificationEventType::RoomOffline);
}
~~~

- [ ] **Step 2: Run policy test and confirm it fails.**

Run: cmake --build native/out/build/windows-x64 --target notification_policy_test; ctest --test-dir native/out/build/windows-x64 -R notification_policy_test --output-on-failure

Expected: build failure because NotificationPolicy does not exist.

- [ ] **Step 3: Implement policy, preferences, and fake notification sink.**

Make WindowsNotificationService accept a narrow SystemNotificationSink for tests. The production sink wraps an existing visible QSystemTrayIcon constructed with the application icon. It checks QSystemTrayIcon::isSystemTrayAvailable() and supportsMessages() before calling showMessage. It does not create a retry loop, tray menu, close-to-tray flow, or notification history.

~~~cpp
bool WindowsNotificationService::deliver(const NotificationEvent &event)
{
    if (!preferences_.enabled || !isEnabled(event.type) || sink_ == nullptr || !sink_->available()) {
        return false;
    }
    sink_->show(event.title, event.body);
    return true;
}
~~~

NotificationSettingsDialog uses QCheckBox controls with accessible labels. A total-switch change disables child controls without changing their stored checked values. Persist immediately through QSettings; a write error keeps the in-memory preferences and shows one fixed in-window status message.

- [ ] **Step 4: Connect coordinator notification events to the main window.**

Create the policy before room restoration and call resetBaseline() after restore. Connect MultiRoomCoordinator::roomSnapshotsChanged to NotificationPolicy::update, then route each returned event into WindowsNotificationService::deliver. Keep notification calculation on the Qt GUI thread. For a room absent from the previous snapshot, NotificationPolicy inserts a baseline record without emitting an event. Tests must assert initial restoration sends no event, while later fake metadata transitions do.

- [ ] **Step 5: Run policy, settings, and integration tests.**

Run: ctest --test-dir native/out/build/windows-x64 -R "notification_policy_test|windows_notification_service_test|main_window_test|multi_room_coordinator_test" --output-on-failure

Expected: all selected tests pass; fake sink observes only enabled, deduplicated, rate-limited events.

- [ ] **Step 6: Commit notifications.**

~~~powershell
git add native/src/workspace/notification_policy.* native/src/app/windows_notification_service.* native/src/app/notification_settings_dialog.* native/src/app/main_window.* native/tests/notification_policy_test.cpp native/tests/windows_notification_service_test.cpp native/tests/main_window_test.cpp native/CMakeLists.txt
git commit -m "feat: notify native room status changes"
~~~

### Task 8: Build, test, inspect the native UI, scan safety boundaries, and record final evidence

**Files:**
- Modify: docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md

- [ ] **Step 1: Configure and build the Windows native preset.**

Run: cmake -S native --preset windows-x64; cmake --build native/out/build/windows-x64 --parallel

Expected: zero configuration and compilation errors.

- [ ] **Step 2: Run the full offline suite.**

Run: ctest --test-dir native/out/build/windows-x64 --output-on-failure

Expected: every registered CTest test passes. Record the actual test count, not a planned count.

- [ ] **Step 3: Run the product self-test and inspect the Qt shell.**

Run: native/out/build/windows-x64/douyu_monitor_native.exe --self-test

Expected: exit code 0.

Start the app with the fake child path or packaged StreamGet service, open the native window, and use Playwright/browser automation only if the Qt application exposes a supported UI automation bridge. Otherwise record a manual Windows check: sidebar on the left, no default M5 dock, status rows fit at 1280x720 and 390x844 equivalent window sizes, toolbar controls remain usable, and notification settings open without a modal playback interruption.

- [ ] **Step 4: Scan staged source, tests, logs, and service output for sensitive leakage.**

Run: rg -n "https?://[^ ]*(wsAuth|token=|sign=)|Cookie|cookie=|token=|signature|traceback|mpv.*error" native/src native/tests native/service docs/superpowers/logs

Expected: no playback URL, credential, signature, raw traceback, or raw mpv diagnostic in the new feature's test evidence or logs. A safe host allowlist literal and fixed test fixture URLs may remain only in existing protocol validation code; inspect and explain each match before finalizing.

- [ ] **Step 5: Compare implementation to both approved designs.**

Check every acceptance point in docs/superpowers/specs/2026-08-25-qt-native-live-status-notifications-design.md and the approved legacy shell layout. Confirm: 9-room limit, left sidebar, groups/favorites/history, restart restoration, primary/audio/quality persistence, 60/120-second refresh, 30/60/120/240-second backoff, automatic stop/replay, four notification event types, 5-minute dedupe, 6-per-minute limit, no background monitoring, no browser runtime, and no sensitive logging.

- [ ] **Step 6: Write final local evidence and create/reread a new Notion page.**

Append **M6 Final Native Workspace, Status, and Notifications Evidence** with actual commits, build/test totals, manual UI check result, sensitive-scan result, design/plan comparison, and explicit limits: offline fakes validate behavior; a live Douyu request and a genuine nine-room playback smoke remain unexecuted. Create DouyuMonitor M6 原生工作区、直播状态与通知验收 in Notion, reread it, and correct any discrepancy before reporting completion.

- [ ] **Step 7: Commit verification evidence only when it changed.**

~~~powershell
git add docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md
git commit -m "test: verify native workspace notifications"
~~~

Skip the commit when the evidence file has no change. Do not create an empty commit.

## Plan Self-Review

- **Spec coverage:** Tasks 5 and 6 cover per-room independent status checks, 60/120-second intervals, deterministic jitter, 30/60/120/240-second backoff, cancellation, status display, and automatic stop/replay. Task 7 covers all four event types, running-only behavior, Windows delivery, default-enabled preference persistence, dedupe, rate limiting, and quiet degradation. Tasks 1 through 4 implement the approved legacy-shell prerequisite: left sidebar, library groups/favorites/history, sorting, primary room, audio focus, requested quality, local persistence, and restart replay. Task 8 checks every acceptance condition, logs locally, and creates/rereads a Notion page.
- **Placeholder scan:** The plan contains no unresolved markers, no generic test instructions, and every implementation task names files, APIs, focused tests, exact commands, expected results, and a commit boundary.
- **Type consistency:** RoomLiveStatus, RoomPlaybackHealth, RoomMetadata, RoomSnapshot, RoomStatusScheduler, NotificationPolicy, NotificationEvent, NotificationPreferences, and WindowsNotificationService use the same names and ownership boundaries across all tasks.
- **Scope boundary:** This plan deliberately does not alter the real StreamGet quality resolver. The requested-quality UI continues to route through the existing M4/M5 policy and current resolver boundary; a real StreamGet quality-selection implementation remains a separate service milestone.
