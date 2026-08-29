# Qt Quick/QML Electron UI Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the shipped QWidget/OpenGL-window UI with a Qt Quick/QML + C++ desktop application that reproduces the Electron UI and renders up to nine libmpv streams without a browser runtime.

**Architecture:** `QGuiApplication` and `QQmlApplicationEngine` load `app/qml/Main.qml`. `AppController` owns the existing services and QML-facing presentation models; QML sends only intents to this controller. A `MpvQuickItem` derived from `QQuickFramebufferObject` owns one libmpv handle and OpenGL render context per visible tile; `RoomSession` attaches to it through C++ and retains media sources only in memory.

**Tech Stack:** C++20, CMake/Ninja, Qt 6 Core/Gui/Qml/Quick/QuickControls2/OpenGL/Test/QuickTest, QML, libmpv render API, Windows notifications, QSettings, StreamGet sidecar.

---

## Fixed decisions and file map

The migration is intentionally not a hybrid. Remove `Qt6::Widgets` and
`Qt6::OpenGLWidgets` from every shipped native target. `QSystemTrayIcon` is a
Widgets API, so replace the current notification sink with a small Win32
`Shell_NotifyIconW` sink before removing the Widgets module. Do not retain a
hidden QWidget solely for notifications.

`QQuickFramebufferObject` requires an OpenGL scene-graph backend. Call
`QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL)` before loading
QML. Its renderer creates, renders, and frees the libmpv render context only
while Qt has the render context current. mpv update callbacks queue
`MpvQuickItem::update()` on the GUI thread; they do not touch QML model data.

No QML property, signal, test failure, application log, or Notion log may
include `MediaSource`, a playback URL, cookie, token, signature, raw
StreamGet response, or raw mpv error text. Map all playback failures to fixed
local codes and Chinese labels at the controller boundary.

| Path | Responsibility |
| --- | --- |
| `native/app/main.cpp` | Configure OpenGL scene graph, register QML type, create controller and engine, retain self-test options. |
| `native/src/ui/app_controller.*` | Own services, persistence, notifications, safe commands, native window actions, and phase-safe shutdown. |
| `native/src/ui/room_list_model.*` | Incrementally expose ordered safe `RoomSnapshot` presentation data to QML. |
| `native/src/ui/workspace_model.*` | Expose global UI state and call controller commands without service pointers in QML. |
| `native/src/ui/monitoring_model.*` | Expose non-sensitive counts, notification preferences, and fixed health labels. |
| `native/src/ui/mpv_quick_item.*` | QQuick framebuffer renderer and the libmpv playback facade used by `RoomSession`. |
| `native/src/workspace/room_session.*` | Keep resolve/lifecycle policy; attach/detach an `MpvQuickItem` instead of constructing a QWidget. |
| `native/src/workspace/multi_room_coordinator.*` | Keep capacity/quality/order policy; provide C++ player attachment, no `QWidget *surfaceParent`. |
| `native/src/workspace/native_workspace_types.*` | Persist groups and compact workspace presets without source material. |
| `native/app/qml/` | Electron-parity shell, components, panels, dialogs, and visual states. |
| `native/tests/*` | Model, controller, renderer, QML loading, screenshot, and existing business-policy regression coverage. |
| `native/CMakeLists.txt` | Qt Quick dependencies, QML module, tests, runtime deployment, and package scan. |

### Task 1: Establish the Qt Quick build foundation

**Files:**
- Modify: `native/CMakeLists.txt`
- Create: `native/app/qml/Main.qml`
- Create: `native/tests/qml_engine_smoke_test.cpp`

- [ ] **Step 1: Write the failing QML engine smoke test.**

```cpp
void QmlEngineSmokeTest::loadsMainQmlWithoutWarnings()
{
    QQmlApplicationEngine engine;
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));

    QVERIFY2(!engine.rootObjects().isEmpty(), "Main.qml did not create a root object");
    QCOMPARE(warnings.count(), 0);
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    QCOMPARE(window->height(), 720);
}
```

- [ ] **Step 2: Configure and run only the new test to prove the target is absent.**

Run: `cmake --preset windows-x64; cmake --build --preset windows-x64-debug --target qml_engine_smoke_test`

Expected: configuration or build fails because `qml_engine_smoke_test` and the `DouyuMonitor` QML module do not exist.

- [ ] **Step 3: Add the minimal Quick application and module.**

In `native/CMakeLists.txt`, extend the existing GUI module discovery with the
following component list, preserving the legacy Widgets targets until Task 8:

```cmake
find_package(Qt6 CONFIG REQUIRED COMPONENTS
    Core Gui Widgets OpenGL OpenGLWidgets Qml Quick QuickControls2 Test QuickTest)

qt_add_library(douyu_qml STATIC)
qt_add_qml_module(douyu_qml
    URI DouyuMonitor
    VERSION 1.0
    QML_FILES
        app/qml/Main.qml
)
target_link_libraries(douyu_qml PRIVATE Qt6::Qml Qt6::Quick Qt6::QuickControls2)

add_executable(qml_engine_smoke_test tests/qml_engine_smoke_test.cpp)
target_link_libraries(qml_engine_smoke_test PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Test)
qt_add_resources(qml_engine_smoke_test qml_engine_test_resources
    PREFIX "/qml"
    FILES app/qml/Main.qml
)
add_test(NAME qml_engine_smoke_test COMMAND qml_engine_smoke_test)
```

Create `Main.qml` with a non-resizable 1280 by 720 application window and an
object name used by the smoke test:

```qml
import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: root
    objectName: "douyuQuickWindow"
    width: 1280
    height: 720
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    color: "#16191f"
    title: "斗鱼多房间监控"
}
```

Keep `native/app/main.cpp` on the legacy entry point during M1-M7. The actual
`QGuiApplication` / `QQmlApplicationEngine` application switch is Task 8,
when the old QWidget UI and its self-test are removed in the same change.

- [ ] **Step 4: Run the foundation test and debug build.**

Run: `cmake --build --preset windows-x64-debug; ctest --preset windows-x64-debug -R qml_engine_smoke_test`

Expected: the test passes and `douyu_monitor_native.exe` is rebuilt without
changing its legacy runtime linkage; `qml_engine_smoke_test` itself links only
Qt Core/Gui/Qml/Quick/Test.

- [ ] **Step 5: Commit the foundation.**

```powershell
git add native/CMakeLists.txt native/app/main.cpp native/app/qml/Main.qml native/tests/qml_engine_smoke_test.cpp
git commit -m "feat: bootstrap Qt Quick application shell"
```

### Execution sequencing repair: establish the renderer and player contract before the controller

Task 4's controller command tests require a coordinator that can manage rooms
without a QWidget surface parent. Task 6 requires the controller's player
attachment invokables. Therefore the original Task 3 -> Task 4 -> Task 5 ->
Task 6 ordering contains a dependency cycle and is replaced by this approved
sequence: Task 3, Task 5, Task 2, Task 4, Task 6, Task 7, Task 8, Task 9.

Task 5 first creates the real `MpvQuickItem`; Task 2 then introduces the QML
player attachment path used by `AppController`. To keep the legacy entry point
and its regression tests operational until Task 8, Task 2 retains an explicit
legacy `QWidget`/`PlayerSurface` constructor overload as a temporary source
compatibility path. The new no-widget constructor is the only path used by the
QML controller. This is not a hybrid product UI or a renderer facade: it uses
the real `MpvQuickItem` renderer, and Task 8 removes the legacy overload and
all remaining QWidget UI code in one change.

Do not introduce a hidden QWidget into `AppController`, a QWidget host for
QML, or a fake player abstraction. Task 2 red tests start only after the real
`MpvQuickItem` type exists; Task 4 then includes the player attach/detach
invokables, and Task 6 supplies their QML lifecycle calls.

### Task 2: Decouple room policy from QWidget ownership

**Files:**
- Modify: `native/src/workspace/room_session.h`
- Modify: `native/src/workspace/room_session.cpp`
- Modify: `native/src/workspace/multi_room_coordinator.h`
- Modify: `native/src/workspace/multi_room_coordinator.cpp`
- Modify: `native/src/workspace/room_workspace_types.h`
- Modify: `native/tests/room_session_test.cpp`
- Modify: `native/tests/multi_room_coordinator_test.cpp`

- [ ] **Step 1: Write failing attachment and deferred-source tests.**

Add this test to `RoomSessionTest`, using a fake `MpvQuickItem` test double or
an item without a rendered window:

```cpp
void RoomSessionTest::defersResolvedSourceUntilPlayerIsAttached()
{
    StreamgetProcessClient client(fakeServicePath());
    RoomSession session(&client, "63136", StreamQuality::Auto, nullptr);
    QVERIFY(session.resolve() > 0);
    QTRY_COMPARE(session.state(), RoomSession::State::Resolving);
    QTRY_VERIFY(session.hasPendingSourceForTest());

    MpvQuickItem player;
    QVERIFY(session.attachPlayer(&player));
    QTRY_COMPARE(session.state(), RoomSession::State::Ready);
    QVERIFY(!session.hasPendingSourceForTest());
    client.shutdown();
}
```

Add this coordinator test:

```cpp
void MultiRoomCoordinatorTest::addsNineRoomsWithoutAWidgetParent()
{
    StreamgetProcessClient client(fakeServicePath());
    MultiRoomCoordinator coordinator(&client);
    for (int i = 0; i < MultiRoomCoordinator::kMaxRooms; ++i)
        QCOMPARE(coordinator.addRoomDetailed(QString::number(63136 + i)), RoomCommandResult::Accepted);
    QCOMPARE(coordinator.addRoomDetailed("999999"), RoomCommandResult::RoomLimitReached);
    client.shutdown();
}

void MultiRoomCoordinatorTest::movesAndRetriesOnlyTheRequestedRoom()
{
    StreamgetProcessClient client(fakeServicePath());
    MultiRoomCoordinator coordinator(&client);
    QVERIFY(coordinator.addRoom("63136"));
    QVERIFY(coordinator.addRoom("63137"));
    QCOMPARE(coordinator.moveRoomDetailed("63137", -1), RoomCommandResult::Accepted);
    QCOMPARE(coordinator.roomIds(), QStringList({"63137", "63136"}));
    QCOMPARE(coordinator.retryRoomDetailed("63137"), RoomCommandResult::Accepted);
    client.shutdown();
}
```

- [ ] **Step 2: Run the focused tests to confirm the old QWidget constructor prevents the migration.**

Run: `cmake --build --preset windows-x64-debug --target room_session_test multi_room_coordinator_test; ctest --preset windows-x64-debug -R "room_session_test|multi_room_coordinator_test"`

Expected: compile failure because the new constructor, attachment methods, and
test-only pending-source accessor have not been introduced.

- [ ] **Step 3: Replace the parent-widget contract with a QPointer attachment contract.**

Use the following public interface in `room_session.h`; the test accessor is
guarded by `#ifdef DOUYU_TESTING` and never linked into the shipped target.

```cpp
class MpvQuickItem;

RoomSession(StreamgetProcessClient *client, QString roomId,
            StreamQuality userQuality, QObject *parent = nullptr);
bool attachPlayer(MpvQuickItem *player);
void detachPlayer(MpvQuickItem *player);
MpvQuickItem *player() const noexcept;
```

Store `QPointer<MpvQuickItem> player_` and `std::optional<MediaSource>
pendingSource_`. In `onControllerSourceReady`, assign `pendingSource_` and
call a private `startPendingSource()` only when `player_` is non-null. That
method calls `player_->loadSource(*pendingSource_)`, clears the optional on
success, and maps a failure to `PLAYER_FAILED`; it never exposes the source.
`attachPlayer` connects `playbackFailed`, applies the current mute state, and
starts any pending source. `detachPlayer` only clears the matching pointer.

Remove the `QWidget *surfaceParent` parameter and member from
`MultiRoomCoordinator`; create sessions with `new RoomSession(client_, roomId,
requestedQuality, this)`. Add these forwarding methods:

```cpp
bool attachPlayer(const QString &roomId, MpvQuickItem *player);
void detachPlayer(const QString &roomId, MpvQuickItem *player);
MpvQuickItem *playerForRoom(const QString &roomId) const noexcept;
RoomCommandResult moveRoomDetailed(const QString &roomId, int delta);
RoomCommandResult retryRoomDetailed(const QString &roomId);
RoomCommandResult setVolume(const QString &roomId, int volume);
```

`applyAudioFocus()` invokes `session->player()->setMuted(!focused)` only when
an item is attached. `roomSnapshots()` derives `muted` from the stored audio
focus when no item exists, rather than reading a widget. Add `int volume = 100`
to `RoomSnapshot` and store the same validated range (0 through 100) in
`RoomSession`; attachment applies it through `MpvQuickItem::setVolume`.

- [ ] **Step 4: Run the focused tests and existing business-policy suite.**

Run: `cmake --build --preset windows-x64-debug --target room_session_test multi_room_coordinator_test; ctest --preset windows-x64-debug -R "room_session_test|multi_room_coordinator_test|quality_policy_test|room_status_scheduler_test"`

Expected: all selected tests pass; ninth room is accepted, tenth room is
rejected, and no constructor accepts a QWidget parent.

- [ ] **Step 5: Commit the decoupling.**

```powershell
git add native/src/workspace/room_session.* native/src/workspace/multi_room_coordinator.* native/tests/room_session_test.cpp native/tests/multi_room_coordinator_test.cpp
git commit -m "refactor: detach room sessions from QWidget players"
```

### Task 3: Implement the safe QML presentation models

**Files:**
- Create: `native/src/ui/room_list_model.h`
- Create: `native/src/ui/room_list_model.cpp`
- Create: `native/src/ui/workspace_model.h`
- Create: `native/src/ui/workspace_model.cpp`
- Create: `native/src/ui/monitoring_model.h`
- Create: `native/src/ui/monitoring_model.cpp`
- Create: `native/tests/room_list_model_test.cpp`
- Create: `native/tests/workspace_model_test.cpp`

- [ ] **Step 1: Write failing model tests for roles and incremental updates.**

```cpp
void RoomListModelTest::updatesOneChangedRoomWithoutModelReset()
{
    RoomListModel model;
    QSignalSpy resets(&model, &QAbstractItemModel::modelReset);
    QSignalSpy changes(&model, &QAbstractItemModel::dataChanged);
    model.applySnapshots({makeSnapshot("63136", true), makeSnapshot("63137", false)});
    changes.clear();

    auto updated = makeSnapshot("63137", false);
    updated.metadata.anchorName = "主播 B";
    model.applySnapshots({makeSnapshot("63136", true), updated});

    QCOMPARE(resets.count(), 0);
    QCOMPARE(changes.count(), 1);
    QCOMPARE(model.data(model.index(1, 0), RoomListModel::AnchorNameRole).toString(), "主播 B");
}
```

```cpp
void WorkspaceModelTest::mapsRoomLimitToFixedChineseFeedback()
{
    WorkspaceModel model(nullptr);
    QCOMPARE(model.commandMessage(RoomCommandResult::RoomLimitReached), QStringLiteral("最多添加 9 个房间"));
    QVERIFY(!model.commandMessage(RoomCommandResult::RoomLimitReached).contains("://"));
}
```

- [ ] **Step 2: Run tests before creating the models.**

Run: `cmake --build --preset windows-x64-debug --target room_list_model_test workspace_model_test`

Expected: build fails because the model classes and targets do not exist.

- [ ] **Step 3: Create the models with only safe QML roles.**

Define the role enum exactly once in `RoomListModel`:

```cpp
enum Role : int {
    RoomIdRole = Qt::UserRole + 1, AnchorNameRole, TitleRole, CategoryRole,
    ViewerLabelRole, AvatarUrlRole, LiveStateRole, PlaybackStateRole,
    PrimaryRole, FavoriteRole, AudioFocusedRole, RequestedQualityRole,
    EffectiveQualityRole, MutedRole, VolumeRole, DanmakuEnabledRole, GroupIdRole
};
```

`applySnapshots(const RoomSnapshots &snapshots)` must compare old and new
room ids, emit `beginInsertRows`/`beginRemoveRows` for membership changes,
emit `dataChanged` only for changed rows, and use `beginResetModel` only when
the order cannot be transformed with those operations. `roleNames()` returns
lower camel-case QML keys such as `roomId`, `anchorName`, and `playbackState`.
Convert enum values to the fixed QML strings `idle`, `loading`, `playing`,
`paused`, `offline`, `reconnecting`, and `error`; do not return diagnostics.

Define `RoomPresentationSettings { int volume = 100; bool danmakuEnabled =
false; QString groupId; }` in `room_list_model.h` and add
`applyPresentationSettings(const QHash<QString, RoomPresentationSettings> &)`.
`AppController` builds this map from safe `NativeRoomRecord` scalar fields and
group membership, then calls it after a workspace mutation. The method emits
`dataChanged` only for rows whose volume, danmaku, or group value changed; it
does not modify the coordinator snapshot contract.

`WorkspaceModel` exposes `sidebarVisible`, `layoutId`, `primaryRoomId`,
`globalMuted`, `danmakuEnabled`, `maxRooms`, `groups`, `presets`, and `lastMessage`, with signals
only when a value changes. `MonitoringModel` exposes `onlineCount`,
`offlineCount`, `errorCount`, `notificationStatus`, and `recoveryStatus`.
All public mutations delegate to `AppController` in Task 4; until then inject
only the command-result mapper needed by its unit test.

Expose the C++ accessors used by controller tests and one `setWorkspaceData`
method used by the controller after every durable mutation:

```cpp
const QVector<NativeRoomGroup> &groups() const noexcept;
const QVector<NativeWorkspacePreset> &presets() const noexcept;
void setWorkspaceData(QVector<NativeRoomGroup> groups,
                      QVector<NativeWorkspacePreset> presets,
                      QString activeGroupId);
```

- [ ] **Step 4: Run the two model tests.**

Run: `cmake --build --preset windows-x64-debug --target room_list_model_test workspace_model_test; ctest --preset windows-x64-debug -R "room_list_model_test|workspace_model_test"`

Expected: both tests pass, and a model reset is not emitted for a same-size
metadata update.

- [ ] **Step 5: Commit the models and tests.**

```powershell
git add native/src/ui/room_list_model.* native/src/ui/workspace_model.* native/src/ui/monitoring_model.* native/tests/room_list_model_test.cpp native/tests/workspace_model_test.cpp native/CMakeLists.txt
git commit -m "feat: add safe QML presentation models"
```

### Task 4: Build the controller, persistence, and native window bridge

**Files:**
- Create: `native/src/ui/app_controller.h`
- Create: `native/src/ui/app_controller.cpp`
- Create: `native/tests/app_controller_test.cpp`
- Modify: `native/app/main.cpp`
- Modify: `native/src/app/windows_notification_service.*`
- Modify: `native/src/workspace/native_workspace_types.h`
- Modify: `native/src/workspace/native_workspace_store.cpp`
- Modify: `native/tests/native_workspace_store_test.cpp`
- Modify: `native/CMakeLists.txt`

- [ ] **Step 1: Write failing controller tests for commands and safe error projection.**

```cpp
void AppControllerTest::addsRoomsThroughModelAndRejectsTheTenth()
{
    AppController controller(fakeServicePath(), &settings_, &sink_);
    for (int i = 0; i < 9; ++i)
        QCOMPARE(controller.addRoom(QString::number(63136 + i)), QString());
    QCOMPARE(controller.addRoom("999999"), QStringLiteral("最多添加 9 个房间"));
    QCOMPARE(controller.rooms()->rowCount(), 9);
}

void AppControllerTest::doesNotExposeSensitivePlaybackMaterial()
{
    AppController controller(fakeServicePath(), &settings_, &sink_);
    const QString message = controller.fixedPlaybackMessage("UNSAFE_STREAM_URL");
    QCOMPARE(message, QStringLiteral("播放地址不可用"));
    QVERIFY(!message.contains("://"));
    QVERIFY(!message.contains("token", Qt::CaseInsensitive));
}

void AppControllerTest::persistsGroupPresetAndRoomPresentationSettings()
{
    AppController controller(fakeServicePath(), &settings_, &sink_);
    QCOMPARE(controller.addRoom("63136"), QString());
    const QString groupId = controller.createGroup(QStringLiteral("赛事"));
    QVERIFY(!groupId.isEmpty());
    QCOMPARE(controller.assignRoomToGroup("63136", groupId), QString());
    QCOMPARE(controller.setVolume("63136", 35), QString());
    controller.toggleDanmaku("63136");
    const QString presetId = controller.saveWorkspacePreset(QStringLiteral("比赛日"));
    QVERIFY(!presetId.isEmpty());

    AppController restored(fakeServicePath(), &settings_, &sink_);
    QCOMPARE(restored.workspace()->groups().size(), 1);
    QCOMPARE(restored.workspace()->presets().size(), 1);
    QCOMPARE(restored.rooms()->data(restored.rooms()->index(0, 0), RoomListModel::VolumeRole).toInt(), 35);
    QVERIFY(restored.rooms()->data(restored.rooms()->index(0, 0), RoomListModel::DanmakuEnabledRole).toBool());
}
```

- [ ] **Step 2: Build and run the test before implementation.**

Run: `cmake --build --preset windows-x64-debug --target app_controller_test; ctest --preset windows-x64-debug -R app_controller_test`

Expected: build fails because `AppController` is absent.

- [ ] **Step 3: Implement the only QML-to-service boundary.**

Create `AppController` as a `QObject` that owns `StreamgetProcessClient`,
`MultiRoomCoordinator`, `NativeWorkspaceStore`, `NotificationPolicy`,
`WindowsNotificationService`, and all three models. Give it exactly these
QML-safe invokables:

```cpp
Q_INVOKABLE QString addRoom(const QString &roomId);
Q_INVOKABLE QString removeRoom(const QString &roomId);
Q_INVOKABLE QString setPrimaryRoom(const QString &roomId);
Q_INVOKABLE QString setAudioRoom(const QString &roomId);
Q_INVOKABLE QString setQuality(const QString &roomId, int quality);
Q_INVOKABLE QString setFavorite(const QString &roomId, bool favorite);
Q_INVOKABLE QString moveRoom(const QString &roomId, int delta);
Q_INVOKABLE QString retryPlayback(const QString &roomId);
Q_INVOKABLE QString setVolume(const QString &roomId, int volume);
Q_INVOKABLE void toggleDanmaku(const QString &roomId);
Q_INVOKABLE QString createGroup(const QString &name);
Q_INVOKABLE QString renameGroup(const QString &groupId, const QString &name);
Q_INVOKABLE QString deleteGroup(const QString &groupId);
Q_INVOKABLE QString assignRoomToGroup(const QString &roomId, const QString &groupId);
Q_INVOKABLE void setActiveGroup(const QString &groupId);
Q_INVOKABLE QString saveWorkspacePreset(const QString &name);
Q_INVOKABLE QString applyWorkspacePreset(const QString &presetId);
Q_INVOKABLE void refreshRoom(const QString &roomId);
Q_INVOKABLE void attachPlayer(const QString &roomId, MpvQuickItem *item);
Q_INVOKABLE void detachPlayer(const QString &roomId, MpvQuickItem *item);
Q_INVOKABLE void minimizeWindow();
Q_INVOKABLE void toggleMaximizedWindow();
Q_INVOKABLE void closeWindow();
```

Connect `MultiRoomCoordinator::roomSnapshotsChanged` to
`RoomListModel::applySnapshots`, then update workspace and monitoring
summaries. Restore `NativeWorkspaceSnapshot` before the engine exposes the
window; persist only durable ids, metadata, requested quality, favorites,
groups, primary room, and audio room. Reuse notification policy baseline logic
to avoid startup notifications. Implement window actions through a stored
`QPointer<QWindow>` set by `setMainWindow(QWindow *)`; no QML code invokes
platform APIs directly.

Add `NativeWorkspacePreset { QString id; QString name; QString layoutId;
QString activeGroupId; QString primaryRoomId; QString audioRoomId; bool
sidebarVisible; bool danmakuEnabled; }` and a `QVector<NativeWorkspacePreset>
presets` field to `NativeWorkspaceSnapshot`. Extend the store's existing
versioned schema with a `presets` JSON array and preserve backward loading of
version 1 snapshots as an empty preset vector. Extend `NativeRoomRecord` with
`int volume = 100` and `bool danmakuEnabled = false`, validate volume within
0 through 100 during load, and default invalid persisted values to 100.
AppController validates group
names as 1 through 30 trimmed characters, generates a `QUuid` identifier for
new groups and presets, removes a deleted group from room assignments, and
persists only after an accepted mutation. Store per-room volume and danmaku as
safe scalar fields in the corresponding room record; never add a media field.

Move notification delivery behind a non-Widgets sink before removing the
Widgets module. Implement `Win32NotificationSink` inside
`windows_notification_service.cpp`: create a hidden `HWND` with a private
window class, register one `NOTIFYICONDATAW` icon through `Shell_NotifyIconW`,
and submit fixed title/body strings using `NIF_INFO`. Its destructor removes
the icon and destroys the window. Preserve `NotificationPreferences`,
`WindowsNotificationService::deliver`, and `statusText`, add `Shell32` to the
Windows link libraries, and use no `QSystemTrayIcon` include or object.

- [ ] **Step 4: Run controller, persistence, notification, and coordinator tests.**

Run: `cmake --build --preset windows-x64-debug --target app_controller_test native_workspace_store_test notification_policy_test windows_notification_service_test multi_room_coordinator_test; ctest --preset windows-x64-debug -R "app_controller_test|native_workspace_store_test|notification_policy_test|windows_notification_service_test|multi_room_coordinator_test"`

Expected: all selected tests pass; room limit, persistence, baseline
suppression, and fixed feedback behavior are retained without Qt Widgets.

- [ ] **Step 5: Commit the controller boundary.**

```powershell
git add native/src/ui/app_controller.* native/src/app/windows_notification_service.* native/tests/app_controller_test.cpp native/app/main.cpp native/CMakeLists.txt
git commit -m "feat: bridge workspace services to QML"
```

### Task 5: Add the libmpv Qt Quick renderer before visual migration

**Files:**
- Create: `native/src/ui/mpv_quick_item.h`
- Create: `native/src/ui/mpv_quick_item.cpp`
- Create: `native/tests/mpv_quick_item_test.cpp`
- Modify: `native/CMakeLists.txt`

Keep `native/src/media/player_surface.*` and its existing tests through Task 8.
The old renderer remains only for the legacy compatibility constructor defined
by the sequencing repair; Task 8 removes it with the QWidget application shell.

- [ ] **Step 1: Port the renderer tests from the widget surface to a QQuickWindow.**

```cpp
void MpvQuickItemTest::rendersOneLocalFrameAndReleasesCleanly()
{
    QQuickWindow window;
    auto *item = new MpvQuickItem(window.contentItem());
    item->setWidth(320); item->setHeight(240);
    window.resize(320, 240); window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QVERIFY(item->loadLocalMedia(makePpmFixture()));
    QTRY_VERIFY_WITH_TIMEOUT(item->isFirstFrameRendered(), 10000);
    item->release();
    QCOMPARE(item->playbackState(), MpvQuickItem::PlaybackState::Idle);
    QVERIFY(!item->isMediaLoaded());
    QVERIFY(!item->isFirstFrameRendered());
}

void MpvQuickItemTest::doesNotPlaceRemoteAddressInFailureText()
{
    MpvQuickItem item;
    QVERIFY(!item.safeErrorLabel().contains("://"));
    QVERIFY(!item.safeErrorLabel().contains("token", Qt::CaseInsensitive));
}
```

- [ ] **Step 2: Run the test before adding the item.**

Run: `cmake --build --preset windows-x64-debug --target mpv_quick_item_test`

Expected: build fails because `MpvQuickItem` is absent.

- [ ] **Step 3: Implement `MpvQuickItem` as the libmpv owner.**

`MpvQuickItem` derives from `QQuickFramebufferObject` and owns `mpv_handle`,
the event timer, source lifecycle state, mute/pause state, and safe error
code. Its nested `Renderer` owns the `mpv_render_context` and implements:

```cpp
QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override;
void synchronize(QQuickFramebufferObject *item) override;
void render() override;
```

`render()` initializes the context once using
`MPV_RENDER_API_TYPE_OPENGL` and `MPV_RENDER_PARAM_OPENGL_INIT_PARAMS`, then
passes `framebufferObject()->handle()` through `MPV_RENDER_PARAM_OPENGL_FBO`
and `MPV_RENDER_PARAM_FLIP_Y` to `mpv_render_context_render`. The update
callback queues `MpvQuickItem::requestFrame` via `QMetaObject::invokeMethod`
with `Qt::QueuedConnection`. The renderer destructor clears that callback and
calls `mpv_render_context_free` while its render context is current.

The item destructor stops its event timer, disables queued updates with an
atomic `acceptUpdates_` flag, calls `release()`, and only then destroys the
mpv handle. `safeErrorLabel()` returns fixed Chinese labels from a closed
error-code table, never `mpv_error_string`. Preserve request-id retirement so
a failing retired load cannot turn a newer load into an error.

- [ ] **Step 4: Run renderer and session regression tests.**

Run: `cmake --build --preset windows-x64-debug --target mpv_quick_item_test room_session_test; ctest --preset windows-x64-debug -R "mpv_quick_item_test|room_session_test"`

Expected: local frame, stop, release, and stale-load tests pass under a
visible `QQuickWindow`; Qt event processing leaves no raw playback address in
the assertion output.

- [ ] **Step 5: Commit the renderer port.**

```powershell
git add native/src/ui/mpv_quick_item.* native/tests/mpv_quick_item_test.cpp native/CMakeLists.txt
git commit -m "feat: add Qt Quick libmpv renderer"
```

### Task 6: Implement the Electron-parity QML shell and grid

**Files:**
- Modify: `native/app/qml/Main.qml`
- Create: `native/app/qml/components/AppHeader.qml`
- Create: `native/app/qml/components/WindowControls.qml`
- Create: `native/app/qml/components/RoomSidebar.qml`
- Create: `native/app/qml/components/WorkspaceGrid.qml`
- Create: `native/app/qml/components/RoomTile.qml`
- Create: `native/app/qml/components/ToastViewport.qml`
- Create: `native/tests/qml_visual_smoke_test.cpp`

- [ ] **Step 1: Add a failing visual-layout test at both acceptance viewports.**

```cpp
void QmlVisualSmokeTest::hasElectronReferenceGeometryAt1280x720()
{
    auto window = loadWindow(QSize(1280, 720));
    QCOMPARE(window->findChild<QObject *>("appHeader")->property("height").toInt(), 44);
    QCOMPARE(window->findChild<QObject *>("roomSidebar")->property("width").toInt(), 268);
    const QImage image = window->grabWindow();
    QVERIFY(!image.isNull());
    QVERIFY(image.pixelColor(300, 60) != QColor("#000000"));
}
```

Add `hasNoTextOverlapAt1920x1080()` by locating the header, sidebar, grid, and
toast rectangles and asserting that only intended parent/child containment
intersections occur.

- [ ] **Step 2: Run the visual test before adding the components.**

Run: `cmake --build --preset windows-x64-debug --target qml_visual_smoke_test; ctest --preset windows-x64-debug -R qml_visual_smoke_test`

Expected: build or test fails because named QML objects do not exist.

- [ ] **Step 3: Implement the main shell with fixed visual tokens.**

Define these values at the `ApplicationWindow` root and use them throughout
the six components:

```qml
readonly property color canvasColor: "#16191f"
readonly property color surfaceColor: "#1f242c"
readonly property color borderColor: "#343b45"
readonly property color accentColor: "#ff7a18"
readonly property int headerHeight: 44
readonly property int sidebarWidth: sidebarVisible ? 268 : 0
```

`AppHeader` has exact height 44, a left title/drag region, controls for
danmaku, settings, monitoring, workspace, mute, audio, layout, and a
`WindowControls` component. `WindowControls` calls controller invokables for
minimize, maximize, and close. Use icon buttons with accessible names and
tooltips, including disabled state; do not turn icon affordances into text
buttons.

`RoomSidebar` is on the left and uses `ListView { model: appController.rooms }`.
It retains tabs, filters, search, quick add, group view, favorite, move,
primary, audio, quality, and remove intents. `WorkspaceGrid` uses a stable
column count derived from `layoutId` (`single`, `grid-2x2`, `grid-3x3`) and
never reconstructs its model. A `RoomTile` includes a `MpvQuickItem` with
`Component.onCompleted: appController.attachPlayer(roomId, player)` and
`Component.onDestruction: appController.detachPlayer(roomId, player)`. Overlay
controls hide after an activity timer only when the tile is not focused and
its menu is closed.

Use `ToastViewport` bound to `WorkspaceModel.lastMessage` for success,
warning, and error messages. QML labels retain the Electron Chinese text.

- [ ] **Step 4: Run visual layout test and capture deterministic evidence.**

Run: `cmake --build --preset windows-x64-debug --target qml_visual_smoke_test; ctest --preset windows-x64-debug -R qml_visual_smoke_test`

Expected: the test passes at 1280 by 720 and 1920 by 1080, with a 44-pixel
header, a 268-pixel expanded sidebar, and non-empty grid pixels. Save only
local, scrubbed screenshots under `native/out/verification/qml/` for the
current run; do not commit them.

- [ ] **Step 5: Commit the shell and grid.**

```powershell
git add native/app/qml/Main.qml native/app/qml/components native/tests/qml_visual_smoke_test.cpp native/CMakeLists.txt
git commit -m "feat: migrate shell sidebar and room grid to QML"
```

### Task 7: Migrate overlays, dialogs, shortcuts, and monitoring

**Files:**
- Create: `native/app/qml/panels/DanmakuSettingsPanel.qml`
- Create: `native/app/qml/panels/MonitoringStatusPanel.qml`
- Create: `native/app/qml/panels/WorkspacePresetsPanel.qml`
- Create: `native/app/qml/dialogs/AddRoomDialog.qml`
- Create: `native/app/qml/dialogs/GroupManagerDialog.qml`
- Create: `native/app/qml/dialogs/NotificationSettingsDialog.qml`
- Modify: `native/app/qml/Main.qml`
- Modify: `native/src/ui/app_controller.*`
- Modify: `native/src/ui/workspace_model.*`
- Modify: `native/src/ui/monitoring_model.*`
- Create: `native/tests/qml_interaction_test.cpp`

- [ ] **Step 1: Write the failing QML interaction test.**

```cpp
void QmlInteractionTest::opensAddRoomDialogAndRejectsInvalidRoomId()
{
    auto window = loadWindow(QSize(1280, 720));
    auto *addButton = window->findChild<QObject *>("quickAddButton");
    QMetaObject::invokeMethod(addButton, "clicked");
    auto *dialog = window->findChild<QObject *>("addRoomDialog");
    QTRY_VERIFY(dialog->property("visible").toBool());
    QVERIFY(!dialog->property("canSubmit").toBool());
}
```

Add tests for opening monitoring drawer, danmaku popup, workspace presets,
notification settings, sidebar toggle, and shortcuts. The shortcut test sends
`Ctrl+N`, `Ctrl+B`, `Ctrl+M`, and `F5`, then asserts the relevant safe QML
property or controller-spy call changed.

- [ ] **Step 2: Run the interaction test before implementing overlays.**

Run: `cmake --build --preset windows-x64-debug --target qml_interaction_test; ctest --preset windows-x64-debug -R qml_interaction_test`

Expected: tests fail because dialogs, panels, and named actions are absent.

- [ ] **Step 3: Add QML overlays and controller commands.**

Use `Popup` for danmaku/settings/presets, a right-side `Drawer` for
monitoring, and modal `Dialog` components for add room, groups, and
notifications. Give each interactive element the object names used in tests.
Route all accepted input through the controller; validate room ids with
`/^[0-9]{1,20}$/` before enabling the QML submit action and validate group
names as non-whitespace length 1 through 30. Keep the controller as the final
validator.

Implement the existing functionality rather than only painting it: room menus
call primary, audio, favorite, quality, volume, danmaku, move, retry, and
remove actions; group dialog calls create, rename, delete, assign, and active
group actions; workspace preset panel calls save and apply. Persist compact
workspace preset names and selected layout through `NativeWorkspaceStore`,
apply notification preferences through `WindowsNotificationService`, and
expose only model summary counts in the monitoring drawer. Main QML owns
`Shortcut` objects for the four retained operations, never raw service calls.

- [ ] **Step 4: Run interaction, notification, and workspace persistence tests.**

Run: `cmake --build --preset windows-x64-debug --target qml_interaction_test app_controller_test native_workspace_store_test windows_notification_service_test; ctest --preset windows-x64-debug -R "qml_interaction_test|app_controller_test|native_workspace_store_test|windows_notification_service_test"`

Expected: each panel/dialog opens, invalid room ids stay disabled, shortcuts
reach the controller boundary, and notification preferences remain durable.

- [ ] **Step 5: Commit interaction parity.**

```powershell
git add native/app/qml/panels native/app/qml/dialogs native/app/qml/Main.qml native/src/ui/app_controller.* native/src/ui/workspace_model.* native/src/ui/monitoring_model.* native/tests/qml_interaction_test.cpp
git commit -m "feat: migrate QML dialogs panels and shortcuts"
```

### Task 8: Remove the remaining QWidget UI and update self-test behavior

**Files:**
- Delete: `native/src/app/main_window.h`
- Delete: `native/src/app/main_window.cpp`
- Delete: `native/src/app/room_sidebar.h`
- Delete: `native/src/app/room_sidebar.cpp`
- Delete: `native/src/app/add_room_dialog.h`
- Delete: `native/src/app/add_room_dialog.cpp`
- Delete: `native/src/app/group_manager_dialog.h`
- Delete: `native/src/app/group_manager_dialog.cpp`
- Delete: `native/src/app/notification_settings_dialog.h`
- Delete: `native/src/app/notification_settings_dialog.cpp`
- Delete: `native/tests/main_window_test.cpp`
- Delete: `native/tests/room_sidebar_test.cpp`
- Modify: `native/app/main.cpp`
- Modify: `native/tests/native_self_test_test.cpp`
- Modify: `native/CMakeLists.txt`

- [ ] **Step 1: Write a failing self-test process assertion for the QML runtime.**

```cpp
void NativeSelfTestTest::quickSelfTestReportsRendererReadiness()
{
    const auto result = runNative({"--self-test"});
    QCOMPARE(result.exitCode, 0);
    QVERIFY(result.stdoutText.contains("native self-test passed: Qt Quick renderer"));
    QVERIFY(!result.stdoutText.contains("QWidget"));
}
```

- [ ] **Step 2: Run the test and confirm the old output is rejected.**

Run: `cmake --build --preset windows-x64-debug --target native_self_test_test; ctest --preset windows-x64-debug -R native_self_test_test`

Expected: failure because the executable still creates `MainWindow` or prints
the old renderer self-test result.

- [ ] **Step 3: Remove old UI compilation and make the self-test use QML.**

`--self-test` must load the QML engine, create one temporary `MpvQuickItem`
as a direct child of the root `QQuickWindow::contentItem()`, set its size to
`1 x 1`, and wait for its render context to be ready. It must print the fixed
success text, call `release()`, delete the temporary item on the GUI thread,
and exit. With `--media <path>`, the same temporary item loads the local test
fixture, waits for the first frame, stops, releases, and exits. Product room
commands continue to resolve rooms only; no local-media command is exposed to
QML.

Delete all QWidget source/test targets and source entries. Remove every
`Qt6::Widgets` and `Qt6::OpenGLWidgets` link from `CMakeLists.txt`, including
unit tests. Rename no source file merely to conceal it: removed files must be
tracked deletions. Keep business tests, but switch their application macro to
`QTEST_GUILESS_MAIN` where they do not create a `QQuickWindow` and
`QTEST_MAIN` where they do.

- [ ] **Step 4: Prove the project has no QWidget or browser runtime linkage.**

Run: `rg -n "QWidget|QMainWindow|QOpenGLWidget|Qt6::Widgets|OpenGLWidgets|Qt WebEngine|Electron|Chromium|node_modules" native --glob '!out/**'`

Expected: no matches in shipped source or CMake. Reference-only Electron
files outside `native/` are allowed and are not part of this scan.

Run: `cmake --build --preset windows-x64-debug; ctest --preset windows-x64-debug -R "native_self_test_test|douyu_monitor_native_self_test"`

Expected: build and both self-tests pass under the QML runtime.

- [ ] **Step 5: Commit the pure-QML runtime.**

```powershell
git add native/CMakeLists.txt native/app/main.cpp native/tests/native_self_test_test.cpp
git rm native/src/app/main_window.* native/src/app/room_sidebar.* native/src/app/add_room_dialog.* native/src/app/group_manager_dialog.* native/src/app/notification_settings_dialog.* native/tests/main_window_test.cpp native/tests/room_sidebar_test.cpp
git commit -m "refactor: remove shipped QWidget user interface"
```

### Task 9: Run release acceptance, packaging, and sensitive-output checks

**Files:**
- Create: `native/cmake/verify_runtime_dependencies.cmake`
- Create: `native/tests/qml_close_regression_test.cpp`
- Modify: `native/CMakeLists.txt`
- Modify: `docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md`

- [ ] **Step 1: Add a failing close regression test.**

```cpp
void QmlCloseRegressionTest::closesNineAttachedPlayersWithoutLingeringCallbacks()
{
    auto window = loadWindowWithNineFakeRooms();
    QVERIFY(QTest::qWaitForWindowExposed(window.get()));
    window->close();
    QTRY_VERIFY(!window->isVisible());
    QTRY_COMPARE_WITH_TIMEOUT(controller_->attachedPlayerCountForTest(), 0, 5000);
    QVERIFY(!controller_->serviceProcessRunningForTest());
}
```

- [ ] **Step 2: Run it before teardown tracking is complete.**

Run: `cmake --build --preset windows-x64-debug --target qml_close_regression_test; ctest --preset windows-x64-debug -R qml_close_regression_test`

Expected: compile failure until the test-only shutdown observation points are
implemented; after they exist, it may expose stale callback ordering.

- [ ] **Step 3: Implement ordered shutdown and dependency verification.**

`AppController::~AppController()` must persist the workspace, ask coordinator
to release sessions, clear QML attachments, stop the StreamGet client, then
release notifications and settings. The item renderer performs callback clear,
render-context free, and `mpv_terminate_destroy` as Task 5 specified. Do not
call `deleteLater()` after `QGuiApplication` begins destruction.

`verify_runtime_dependencies.cmake` runs `dumpbin /DEPENDENTS` against the
release executable and fails on `Qt6Widgets`, `Qt6WebEngine`, `electron.exe`,
`chrome.dll`, or `node.exe`. It requires `Qt6Quick`, `Qt6Qml`, Qt Core/Gui,
and `libmpv-2.dll` in the package directory. Add it after `windeployqt` in a
post-build command.

- [ ] **Step 4: Run the full Debug and Release evidence set.**

Run: `cmake --preset windows-x64; cmake --build --preset windows-x64-debug; ctest --preset windows-x64-debug`

Expected: every Debug CTest passes, including model, renderer, visual,
interaction, close, existing service, workspace, quality, and notification
tests.

Run: `cmake --preset windows-x64-release; cmake --build --preset windows-x64-release; ctest --preset windows-x64-release`

Expected: every Release CTest passes and the package includes the matching
release Qt plugins, QML modules, `libmpv-2.dll`, and `streamget_service.exe`.

Run: `rg -n -i "https?://[^ ]*(token|cookie|signature|wsauth)|mpv_error_string|MediaSource\(" native/out docs/superpowers/logs --glob '!**/*.png'`

Expected: no output. If a test fixture contains a deliberately redacted URL,
replace it with a value-free assertion before release.

- [ ] **Step 5: Commit acceptance plumbing and local evidence log.**

```powershell
git add native/cmake/verify_runtime_dependencies.cmake native/tests/qml_close_regression_test.cpp native/CMakeLists.txt docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md
git commit -m "test: add QML release acceptance checks"
```

### Task 10: Required phase tracking and Notion readback

**Files:**
- Modify: `docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md`
- Create: one Notion child page per completed M1 through M5 phase under `https://app.notion.com/p/3c80f4bdec488139bd2fd87dd66d63ed?pvs=204`

- [ ] **Step 1: After each M1, M2, M3, M4, and M5 completion, compare its evidence with this plan and the approved design.**

Use a table with exactly these columns: `Phase`, `Design requirement`,
`Implemented evidence`, `Verification command`, `Result`, `Open limit`.
Every claim must point to a changed file and a fresh command result. Record
`No open limit` only when the phase's tests pass; otherwise name the blocker
without credentials or sensitive media details.

- [ ] **Step 2: Append the same sanitized entry to the local progress log.**

Use this fixed entry shape:

```markdown
## 2026-08-26 - M1 Qt Quick foundation

- Design/plan alignment: M1 requirements checked against the approved QML migration design and this implementation plan.
- Evidence: `native/app/main.cpp`, `native/app/qml/Main.qml`, and `native/tests/qml_engine_smoke_test.cpp`.
- Verification: `ctest --preset windows-x64-debug -R qml_engine_smoke_test` passed.
- Sensitive-data check: no playback source material was recorded.
- Next phase: M2 C++ to QML state bridge.
```

- [ ] **Step 3: Create a Notion child log page and read it back.**

The page must contain the same five bullet categories, the current commit id,
and the plan/design links. Fetch the newly created page immediately and verify
that the expected phase heading and the `Sensitive-data check` bullet are
present before reporting synchronization.

- [ ] **Step 4: Record the Notion page URL in the local entry.**

Expected: the local log names the exact child page; the fetched Notion page
matches the local entry and contains no playback source material.

- [ ] **Step 5: Commit only the phase-log update that belongs to the completed phase.**

```powershell
git add docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md
git commit -m "docs: record M<phase> QML migration evidence"
```

## Final implementation review

Before declaring M5 complete, perform these checks in order:

1. Compare every row in the approved design's `Acceptance Criteria` with one
   completed task above and one fresh test, screenshot, or package result.
2. Run `rg -n -e ('TO' + 'DO') -e ('TB' + 'D') -e ('implement' + ' later') -e ('fill in' + ' details') docs/superpowers/plans/2026-08-26-qt-quick-qml-electron-ui-migration.md` and require no output.
3. Run `git diff --check` and inspect `git status --short`; do not stage or
   revert pre-existing unrelated worktree changes.
4. Run the full Debug and Release commands from Task 9 after the last source
   change, then perform a manual 1280 by 720 and 1920 by 1080 visual review of
   the native QML window.
5. Create and fetch the M5 Notion log page before claiming that the design,
   plan, code, local log, and Notion record are synchronized.
