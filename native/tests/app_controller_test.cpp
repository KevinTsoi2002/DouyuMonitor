#include <QSettings>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest/QtTest>

#include "app/windows_notification_service.h"
#include "danmaku/douyu_danmaku_client.h"
#include "ui/app_controller.h"
#include "ui/room_list_model.h"
#include "ui/workspace_model.h"

#ifndef FAKE_STREAMGET_SERVICE_PATH
#define FAKE_STREAMGET_SERVICE_PATH "fake_streamget_service"
#endif

namespace {

QString fakeServicePath()
{
    return QString::fromLocal8Bit(FAKE_STREAMGET_SERVICE_PATH);
}

class FakeNotificationSink final : public SystemNotificationSink {
public:
    bool available() const override { return true; }
    void show(const QString &, const QString &) override { ++shownCount; }

    int shownCount = 0;
};

struct FakeDanmakuFactoryState {
    int starts = 0;
    int stops = 0;
    QVector<QString> rooms;
};

class FakeDanmakuClient final : public DanmakuClient {
public:
    FakeDanmakuClient(QString roomId, FakeDanmakuFactoryState *state, QObject *parent)
        : DanmakuClient(parent)
        , roomId_(std::move(roomId))
        , state_(state)
    {
    }

    void start() override
    {
        if (started_) return;
        started_ = true;
        ++state_->starts;
        emit statusChanged({roomId_, DanmakuConnectionState::Connected});
    }

    void stop() override
    {
        if (!started_ || stopped_) return;
        stopped_ = true;
        ++state_->stops;
    }

    void retry() override
    {
        stopped_ = false;
        start();
    }

private:
    QString roomId_;
    FakeDanmakuFactoryState *state_ = nullptr;
    bool started_ = false;
    bool stopped_ = false;
};

} // namespace

class AppControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void addsRoomsThroughModelAndRejectsTheTenth();
    void recordsOpenedRoomsForTheLibraryHistory();
    void doesNotExposeSensitivePlaybackMaterial();
    void restoresSavedMetadataIntoRoomModel();
    void persistsGroupPresetAndRoomPresentationSettings();
    void persistsNotificationPreferenceAndUpdatesMonitoringState();
    void persistsNotificationEventPreferences();
    void synchronizesDanmakuFromGlobalRoomLiveAndActiveState();
    void stopsDanmakuBeforeServiceShutdown();
    void activatesGroupAndReplacesActiveRoomSet();
    void managesGroupMembersAndOrder();
    void allowsRoomMembershipInMultipleGroupsAndEnforcesCapacity();
    void assignsLibraryRoomToActiveGroupAfterSwitch();
    void restoresActiveGroupMembershipInOrder();
    void searchesRoomCandidatesAndAddsMetadata();
    void projectsRefreshedRoomMetadataAndStatus();
    void publishesCommandFailureToToast();
    void restoresLayoutAndRatioFromWorkspacePreset();
    void persistsGlobalAudioPolicy();
    void preservesFavoriteWhenReaddingLibraryRoom();
    void ordersFavoritesByManualOrderAndMovesThem();
    void appliesWorkspacePresetToAllRoomsRepeatedly();
    void restoresPresetRoomsMissingFromLibrary();
    void defersRequestedRoomRemovalUntilEventLoop();
};

void AppControllerTest::addsRoomsThroughModelAndRejectsTheTenth()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    for (int index = 0; index < 9; ++index) {
        QCOMPARE(controller.addRoom(QString::number(63136 + index)), QString());
    }

    QCOMPARE(controller.addRoom(QStringLiteral("999999")), QStringLiteral("最多添加 9 个房间"));
    QCOMPARE(controller.rooms()->rowCount(), 9);
}

void AppControllerTest::defersRequestedRoomRemovalUntilEventLoop()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
    QVERIFY(QMetaObject::invokeMethod(&controller, "requestRemoveRoom",
                                      Q_ARG(QString, QStringLiteral("63136"))));
    QCOMPARE(controller.rooms()->rowCount(), 1);
    QTRY_COMPARE(controller.rooms()->rowCount(), 0);
}

void AppControllerTest::recordsOpenedRoomsForTheLibraryHistory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
    const QVariantList rooms = controller.libraryRooms();
    QCOMPARE(rooms.size(), 1);
    const QVariantMap entry = rooms.first().toMap();
    QCOMPARE(entry.value(QStringLiteral("roomId")).toString(), QStringLiteral("63136"));
    QVERIFY(entry.value(QStringLiteral("active")).toBool());
    QVERIFY(entry.value(QStringLiteral("lastOpenedAtMs")).toLongLong() > 0);
}

void AppControllerTest::doesNotExposeSensitivePlaybackMaterial()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    const QString message = controller.fixedPlaybackMessage(QStringLiteral("UNSAFE_STREAM_URL"));
    QCOMPARE(message, QStringLiteral("播放地址不可用"));
    QVERIFY(!message.contains(QStringLiteral("://")));
    QVERIFY(!message.contains(QStringLiteral("token"), Qt::CaseInsensitive));
}

void AppControllerTest::restoresSavedMetadataIntoRoomModel()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    settings.setValue(
        QStringLiteral("DouyuMonitor/nativeWorkspaceV1"),
        QByteArray(R"JSON({"version":2,"library":[{"roomId":"63136","metadata":{"roomId":"63136","anchorName":"已保存主播","title":"已保存标题","category":"游戏","viewerLabel":"1.2万"},"requestedQuality":"auto","favorite":false,"lastOpenedAtMs":0,"volume":100,"danmakuEnabled":true}],"groups":[],"activeRoomIds":["63136"],"activeGroupId":"","primaryRoomId":"63136","audioRoomId":"","presets":[]})JSON"));
    FakeNotificationSink sink;

    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.rooms()->rowCount(), 1);
    const QModelIndex room = controller.rooms()->index(0, 0);
    QCOMPARE(controller.rooms()->data(room, RoomListModel::AnchorNameRole).toString(),
             QStringLiteral("已保存主播"));
    QCOMPARE(controller.rooms()->data(room, RoomListModel::TitleRole).toString(),
             QStringLiteral("已保存标题"));
}

void AppControllerTest::persistsGroupPresetAndRoomPresentationSettings()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString settingsPath = directory.filePath(QStringLiteral("workspace.ini"));
    FakeNotificationSink sink;

    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        AppController controller(fakeServicePath(), &settings, &sink);
        QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
        QVERIFY(controller.rooms()
                    ->data(controller.rooms()->index(0, 0), RoomListModel::DanmakuEnabledRole)
                    .toBool());
        const QString groupId = controller.createGroup(QStringLiteral("赛事"));
        QVERIFY(!groupId.isEmpty());
        QCOMPARE(controller.assignRoomToGroup(QStringLiteral("63136"), groupId), QString());
        QCOMPARE(controller.setVolume(QStringLiteral("63136"), 35), QString());
        controller.toggleDanmaku(QStringLiteral("63136"));
        const QString presetId = controller.saveWorkspacePreset(QStringLiteral("比赛日"));
        QVERIFY(!presetId.isEmpty());
    }

    QSettings restoredSettings(settingsPath, QSettings::IniFormat);
    AppController restored(fakeServicePath(), &restoredSettings, &sink);
    QCOMPARE(restored.workspace()->groups().size(), 1);
    QCOMPARE(restored.workspace()->presets().size(), 1);
    QCOMPARE(restored.rooms()->data(restored.rooms()->index(0, 0), RoomListModel::VolumeRole).toInt(),
             35);
    QVERIFY(!restored.rooms()
                 ->data(restored.rooms()->index(0, 0), RoomListModel::DanmakuEnabledRole)
                 .toBool());
}

void AppControllerTest::restoresLayoutAndRatioFromWorkspacePreset()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
    QCOMPARE(controller.addRoom(QStringLiteral("63137")), QString());
    bool changed = false;
    QVERIFY(QMetaObject::invokeMethod(&controller,
                                      "setLayout",
                                      Q_RETURN_ARG(bool, changed),
                                      Q_ARG(QString, QStringLiteral("primary"))));
    QVERIFY(changed);
    QVERIFY(QMetaObject::invokeMethod(&controller,
                                      "setPrimaryRoomRatio",
                                      Q_RETURN_ARG(bool, changed),
                                      Q_ARG(double, 0.67)));
    QVERIFY(changed);
    const QString presetId = controller.saveWorkspacePreset(QStringLiteral("主画面"));
    QVERIFY(!presetId.isEmpty());

    QVERIFY(QMetaObject::invokeMethod(&controller,
                                      "setLayout",
                                      Q_RETURN_ARG(bool, changed),
                                      Q_ARG(QString, QStringLiteral("grid-3x3"))));
    QVERIFY(changed);
    QVERIFY(controller.applyWorkspacePreset(presetId).isEmpty());
    QCOMPARE(controller.workspace()->layoutId(), QStringLiteral("primary"));
    QCOMPARE(controller.workspace()->primaryRoomRatio(), 0.67);
}

void AppControllerTest::persistsGlobalAudioPolicy()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QVERIFY(controller.setAudioMode(QStringLiteral("multi")));
    QVERIFY(controller.setGlobalMuted(true));
    QCOMPARE(controller.workspace()->audioMode(), QStringLiteral("multi"));
    QVERIFY(controller.workspace()->globalMuted());
    QVERIFY(!controller.setAudioMode(QStringLiteral("unsupported")));
    QCOMPARE(controller.workspace()->audioMode(), QStringLiteral("multi"));
}

void AppControllerTest::preservesFavoriteWhenReaddingLibraryRoom()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
    QCOMPARE(controller.setFavorite(QStringLiteral("63136"), true), QString());
    QCOMPARE(controller.removeRoom(QStringLiteral("63136")), QString());
    QCOMPARE(controller.libraryRooms().first().toMap().value(QStringLiteral("favorite")).toBool(), true);

    QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
    QCOMPARE(controller.libraryRooms().first().toMap().value(QStringLiteral("favorite")).toBool(), true);
    QCOMPARE(controller.rooms()->data(controller.rooms()->index(0, 0), RoomListModel::FavoriteRole).toBool(), true);
}

void AppControllerTest::ordersFavoritesByManualOrderAndMovesThem()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
    QCOMPARE(controller.addRoom(QStringLiteral("63137")), QString());
    QCOMPARE(controller.addRoom(QStringLiteral("63138")), QString());
    QCOMPARE(controller.setFavorite(QStringLiteral("63136"), true), QString());
    QCOMPARE(controller.setFavorite(QStringLiteral("63137"), true), QString());

    QCOMPARE(controller.libraryRooms().at(0).toMap().value(QStringLiteral("roomId")).toString(),
             QStringLiteral("63136"));
    QCOMPARE(controller.libraryRooms().at(1).toMap().value(QStringLiteral("roomId")).toString(),
             QStringLiteral("63137"));
    QCOMPARE(controller.moveFavoriteRoom(QStringLiteral("63137"), 0), QString());
    QCOMPARE(controller.libraryRooms().at(0).toMap().value(QStringLiteral("roomId")).toString(),
             QStringLiteral("63137"));
    QCOMPARE(controller.libraryRooms().at(1).toMap().value(QStringLiteral("roomId")).toString(),
             QStringLiteral("63136"));
}

void AppControllerTest::appliesWorkspacePresetToAllRoomsRepeatedly()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
    QCOMPARE(controller.addRoom(QStringLiteral("63137")), QString());
    QCOMPARE(controller.addRoom(QStringLiteral("63138")), QString());
    QCOMPARE(controller.addRoom(QStringLiteral("63139")), QString());
    QCOMPARE(controller.addRoom(QStringLiteral("63140")), QString());
    const QString presetId = controller.saveWorkspacePreset(QStringLiteral("五路"));
    QVERIFY(!presetId.isEmpty());

    QCOMPARE(controller.addRoom(QStringLiteral("63141")), QString());
    QCOMPARE(controller.applyWorkspacePreset(presetId), QString());
    QCOMPARE(controller.rooms()->rowCount(), 5);
    QCOMPARE(controller.rooms()->data(controller.rooms()->index(0, 0), RoomListModel::RoomIdRole).toString(),
             QStringLiteral("63136"));
    QCOMPARE(controller.rooms()->data(controller.rooms()->index(1, 0), RoomListModel::RoomIdRole).toString(),
             QStringLiteral("63137"));
    QCOMPARE(controller.rooms()->data(controller.rooms()->index(2, 0), RoomListModel::RoomIdRole).toString(),
             QStringLiteral("63138"));
    QCOMPARE(controller.rooms()->data(controller.rooms()->index(3, 0), RoomListModel::RoomIdRole).toString(),
             QStringLiteral("63139"));
    QCOMPARE(controller.rooms()->data(controller.rooms()->index(4, 0), RoomListModel::RoomIdRole).toString(),
             QStringLiteral("63140"));

    QCOMPARE(controller.applyWorkspacePreset(presetId), QString());
    QCOMPARE(controller.rooms()->rowCount(), 5);
    QCOMPARE(controller.rooms()->data(controller.rooms()->index(0, 0), RoomListModel::RoomIdRole).toString(),
             QStringLiteral("63136"));
    QCOMPARE(controller.rooms()->data(controller.rooms()->index(1, 0), RoomListModel::RoomIdRole).toString(),
             QStringLiteral("63137"));
}

void AppControllerTest::restoresPresetRoomsMissingFromLibrary()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    settings.setValue(
        QStringLiteral("DouyuMonitor/nativeWorkspaceV1"),
        QByteArray(R"JSON({"version":2,"library":[{"roomId":"63136","metadata":{"roomId":"63136","anchorName":"主播 1","title":"标题 1","category":"游戏","viewerLabel":"1"},"requestedQuality":"auto","favorite":false,"lastOpenedAtMs":0,"volume":100,"danmakuEnabled":true}],"groups":[],"activeRoomIds":["63136"],"activeGroupId":"","primaryRoomId":"63136","audioRoomId":"63136","presets":[{"id":"p1","name":"五路预设","layoutId":"grid-3x2","activeGroupId":"","primaryRoomId":"63136","audioRoomId":"63136","roomIds":["63136","63137","63138","63139","63140"],"sidebarVisible":true,"danmakuEnabled":true}]})JSON"));
    FakeNotificationSink sink;

    AppController controller(fakeServicePath(), &settings, &sink);
    QCOMPARE(controller.workspace()->presets().size(), 1);
    QCOMPARE(controller.applyWorkspacePreset(QStringLiteral("p1")), QString());
    QCOMPARE(controller.rooms()->rowCount(), 5);
    QCOMPARE(controller.workspace()->primaryRoomId(), QStringLiteral("63136"));
    QCOMPARE(controller.rooms()->data(controller.rooms()->index(4, 0), RoomListModel::RoomIdRole).toString(),
             QStringLiteral("63140"));

    QCOMPARE(controller.removeRoom(QStringLiteral("63136")), QString());
    QCOMPARE(controller.rooms()->rowCount(), 4);
    QCOMPARE(controller.workspace()->primaryRoomId(), QStringLiteral("63137"));
    QCOMPARE(controller.rooms()->data(controller.rooms()->index(0, 0), RoomListModel::RoomIdRole).toString(),
             QStringLiteral("63137"));
}

void AppControllerTest::persistsNotificationPreferenceAndUpdatesMonitoringState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString settingsPath = directory.filePath(QStringLiteral("workspace.ini"));
    FakeNotificationSink sink;

    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        AppController controller(fakeServicePath(), &settings, &sink);
        QCOMPARE(controller.setNotificationsEnabled(false), QString());
        QCOMPARE(controller.monitoring()->notificationStatus(), QStringLiteral("disabled"));
    }

    QSettings restoredSettings(settingsPath, QSettings::IniFormat);
    AppController restored(fakeServicePath(), &restoredSettings, &sink);
    QCOMPARE(restored.monitoring()->notificationStatus(), QStringLiteral("disabled"));
}

void AppControllerTest::persistsNotificationEventPreferences()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString settingsPath = directory.filePath(QStringLiteral("workspace.ini"));
    FakeNotificationSink sink;

    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        AppController controller(fakeServicePath(), &settings, &sink);
        QCOMPARE(controller.setNotificationPreferences(true, false, true, false, true), QString());
        const QVariantMap preferences = controller.notificationPreferences();
        QCOMPARE(preferences.value(QStringLiteral("roomOnline")).toBool(), false);
        QCOMPARE(preferences.value(QStringLiteral("roomOffline")).toBool(), true);
        QCOMPARE(preferences.value(QStringLiteral("playbackFailed")).toBool(), false);
        QCOMPARE(preferences.value(QStringLiteral("playbackRecovered")).toBool(), true);
    }

    QSettings restoredSettings(settingsPath, QSettings::IniFormat);
    AppController restored(fakeServicePath(), &restoredSettings, &sink);
    const QVariantMap restoredPreferences = restored.notificationPreferences();
    QCOMPARE(restoredPreferences.value(QStringLiteral("roomOnline")).toBool(), false);
    QCOMPARE(restoredPreferences.value(QStringLiteral("roomOffline")).toBool(), true);
    QCOMPARE(restoredPreferences.value(QStringLiteral("playbackFailed")).toBool(), false);
    QCOMPARE(restoredPreferences.value(QStringLiteral("playbackRecovered")).toBool(), true);
}

void AppControllerTest::synchronizesDanmakuFromGlobalRoomLiveAndActiveState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    FakeDanmakuFactoryState state;
    const DanmakuClientFactory factory = [&state](const QString &roomId, QObject *parent) {
        state.rooms.push_back(roomId);
        return std::unique_ptr<DanmakuClient>(new FakeDanmakuClient(roomId, &state, parent));
    };
    AppController controller(fakeServicePath(), &settings, &sink, factory);

    QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
    QTRY_COMPARE_WITH_TIMEOUT(state.starts, 1, 3000);
    QCOMPARE(controller.danmaku()->activeSessionCountForTest(), 1);

    controller.danmaku()->setGlobalEnabled(false);
    QTRY_COMPARE_WITH_TIMEOUT(state.stops, 1, 1000);
    QCOMPARE(controller.danmaku()->activeSessionCountForTest(), 0);

    controller.danmaku()->setGlobalEnabled(true);
    QTRY_COMPARE_WITH_TIMEOUT(state.starts, 2, 1000);
    controller.toggleDanmaku(QStringLiteral("63136"));
    QTRY_COMPARE_WITH_TIMEOUT(state.stops, 2, 1000);
    QCOMPARE(controller.danmaku()->activeSessionCountForTest(), 0);

    QCOMPARE(controller.removeRoom(QStringLiteral("63136")), QString());
    QCOMPARE(controller.danmaku()->activeSessionCountForTest(), 0);
}

void AppControllerTest::stopsDanmakuBeforeServiceShutdown()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    FakeDanmakuFactoryState state;
    const DanmakuClientFactory factory = [&state](const QString &roomId, QObject *parent) {
        return std::unique_ptr<DanmakuClient>(new FakeDanmakuClient(roomId, &state, parent));
    };
    auto controller = std::make_unique<AppController>(fakeServicePath(), &settings, &sink, factory);
    QCOMPARE(controller->addRoom(QStringLiteral("63136")), QString());
    QTRY_COMPARE_WITH_TIMEOUT(state.starts, 1, 3000);
    controller->shutdown();
    QCOMPARE(controller->danmaku()->activeSessionCountForTest(), 0);
    QVERIFY(state.stops >= 1);
}

void AppControllerTest::activatesGroupAndReplacesActiveRoomSet()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
    QCOMPARE(controller.addRoom(QStringLiteral("63137")), QString());
    QCOMPARE(controller.addRoom(QStringLiteral("63138")), QString());
    const QString groupId = controller.createGroup(QStringLiteral("赛事"));
    QVERIFY(!groupId.isEmpty());
    QCOMPARE(controller.assignRoomToGroup(QStringLiteral("63138"), groupId), QString());
    QCOMPARE(controller.assignRoomToGroup(QStringLiteral("63136"), groupId), QString());

    controller.setActiveGroup(groupId);
    QCOMPARE(controller.rooms()->rowCount(), 2);
    QCOMPARE(controller.rooms()->data(controller.rooms()->index(0, 0), RoomListModel::RoomIdRole).toString(),
             QStringLiteral("63138"));
    QCOMPARE(controller.rooms()->data(controller.rooms()->index(1, 0), RoomListModel::RoomIdRole).toString(),
             QStringLiteral("63136"));
    QCOMPARE(controller.workspace()->groupItems().first().toMap().value(QStringLiteral("active")).toBool(),
             true);
}

void AppControllerTest::managesGroupMembersAndOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
    QCOMPARE(controller.addRoom(QStringLiteral("63137")), QString());
    const QString groupId = controller.createGroup(QStringLiteral("管理"));
    QVERIFY(!groupId.isEmpty());
    QCOMPARE(controller.assignRoomToGroup(QStringLiteral("63136"), groupId), QString());
    QCOMPARE(controller.assignRoomToGroup(QStringLiteral("63137"), groupId), QString());
    QCOMPARE(controller.moveRoomInGroup(groupId, QStringLiteral("63137"), -1), QString());
    QCOMPARE(controller.workspace()->groups().first().roomIds,
             QStringList({QStringLiteral("63137"), QStringLiteral("63136")}));
    QCOMPARE(controller.removeRoomFromGroup(groupId, QStringLiteral("63137")), QString());
    QCOMPARE(controller.workspace()->groups().first().roomIds,
             QStringList({QStringLiteral("63136")}));
}

void AppControllerTest::allowsRoomMembershipInMultipleGroupsAndEnforcesCapacity()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    QJsonArray library;
    for (int index = 0; index < 10; ++index) {
        const QString roomId = QString::number(63136 + index);
        library.append(QJsonObject{{"roomId", roomId},
                                   {"metadata", QJsonObject{{"roomId", roomId},
                                                             {"anchorName", roomId},
                                                             {"title", ""},
                                                             {"category", ""},
                                                             {"viewerLabel", ""}}},
                                   {"requestedQuality", "auto"},
                                   {"favorite", false},
                                   {"lastOpenedAtMs", 0},
                                   {"volume", 100},
                                   {"danmakuEnabled", false}});
    }
    settings.setValue(QStringLiteral("DouyuMonitor/nativeWorkspaceV1"),
                      QJsonDocument(QJsonObject{{"version", 2},
                                                {"library", library},
                                                {"groups", QJsonArray{}},
                                                {"activeRoomIds", QJsonArray{}},
                                                {"activeGroupId", ""},
                                                {"primaryRoomId", ""},
                                                {"audioRoomId", ""},
                                                {"presets", QJsonArray{}}})
                          .toJson(QJsonDocument::Compact));
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    for (int index = 0; index < 9; ++index) {
        QCOMPARE(controller.addRoom(QString::number(63136 + index)), QString());
    }
    const QString firstGroup = controller.createGroup(QStringLiteral("A"));
    const QString secondGroup = controller.createGroup(QStringLiteral("B"));
    for (int index = 0; index < 9; ++index) {
        QCOMPARE(controller.assignRoomToGroup(QString::number(63136 + index), firstGroup), QString());
    }
    QCOMPARE(controller.workspace()->groups().at(0).roomIds.size(), 9);
    QCOMPARE(controller.assignRoomToGroup(QStringLiteral("63145"), firstGroup),
             QStringLiteral("分组最多包含 9 个房间"));
    QCOMPARE(controller.assignRoomToGroup(QStringLiteral("63136"), secondGroup), QString());
    QCOMPARE(controller.workspace()->groups().at(0).roomIds.size(), 9);
    QCOMPARE(controller.workspace()->groups().at(1).roomIds,
             QStringList({QStringLiteral("63136")}));
}

void AppControllerTest::assignsLibraryRoomToActiveGroupAfterSwitch()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
    QCOMPARE(controller.addRoom(QStringLiteral("63137")), QString());
    QCOMPARE(controller.addRoom(QStringLiteral("63138")), QString());
    const QString groupId = controller.createGroup(QStringLiteral("活动"));
    QVERIFY(!groupId.isEmpty());
    QCOMPARE(controller.assignRoomToGroup(QStringLiteral("63136"), groupId), QString());
    controller.setActiveGroup(groupId);

    QCOMPARE(controller.assignRoomToGroup(QStringLiteral("63138"), groupId), QString());
    QCOMPARE(controller.workspace()->groups().first().roomIds,
             QStringList({QStringLiteral("63136"), QStringLiteral("63138")}));
    QCOMPARE(controller.rooms()->rowCount(), 2);
    QCOMPARE(controller.rooms()->data(controller.rooms()->index(1, 0), RoomListModel::RoomIdRole)
                 .toString(),
             QStringLiteral("63138"));
}

void AppControllerTest::restoresActiveGroupMembershipInOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString settingsPath = directory.filePath(QStringLiteral("workspace.ini"));
    FakeNotificationSink sink;
    QString groupId;
    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        AppController controller(fakeServicePath(), &settings, &sink);
        QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
        QCOMPARE(controller.addRoom(QStringLiteral("63137")), QString());
        groupId = controller.createGroup(QStringLiteral("恢复"));
        QVERIFY(!groupId.isEmpty());
        QCOMPARE(controller.assignRoomToGroup(QStringLiteral("63137"), groupId), QString());
        QCOMPARE(controller.assignRoomToGroup(QStringLiteral("63136"), groupId), QString());
        controller.setActiveGroup(groupId);
    }

    QSettings restoredSettings(settingsPath, QSettings::IniFormat);
    AppController restored(fakeServicePath(), &restoredSettings, &sink);
    QCOMPARE(restored.workspace()->groupItems().first().toMap().value(QStringLiteral("active"))
                 .toBool(),
             true);
    QCOMPARE(restored.rooms()->rowCount(), 2);
    QCOMPARE(restored.rooms()->data(restored.rooms()->index(0, 0), RoomListModel::RoomIdRole)
                 .toString(),
             QStringLiteral("63137"));
    QCOMPARE(restored.rooms()->data(restored.rooms()->index(1, 0), RoomListModel::RoomIdRole)
                 .toString(),
             QStringLiteral("63136"));
}

void AppControllerTest::searchesRoomCandidatesAndAddsMetadata()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    controller.searchRooms(QStringLiteral("https://www.douyu.com/63136"));
    QTRY_COMPARE_WITH_TIMEOUT(controller.searchStatus(), QStringLiteral("success"), 3000);
    QCOMPARE(controller.searchResults().size(), 1);
    const QVariantMap candidate = controller.searchResults().first().toMap();
    QCOMPARE(candidate.value(QStringLiteral("roomId")).toString(), QStringLiteral("63136"));
    QCOMPARE(candidate.value(QStringLiteral("anchorName")).toString(), QStringLiteral("Fake Anchor"));

    QCOMPARE(controller.addRoomCandidate(QStringLiteral("63136")), QString());
    QCOMPARE(controller.rooms()->rowCount(), 1);
    const QModelIndex room = controller.rooms()->index(0, 0);
    QCOMPARE(controller.rooms()->data(room, RoomListModel::AnchorNameRole).toString(),
             QStringLiteral("Fake Anchor"));
    QCOMPARE(controller.rooms()->data(room, RoomListModel::TitleRole).toString(),
             QStringLiteral("Fake Room"));
}

void AppControllerTest::projectsRefreshedRoomMetadataAndStatus()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms()->rowCount(), 1, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(
        controller.rooms()->data(controller.rooms()->index(0, 0), RoomListModel::LiveStateRole)
            .toString(),
        QStringLiteral("online"),
        3000);

    const QModelIndex room = controller.rooms()->index(0, 0);
    QCOMPARE(controller.rooms()->data(room, RoomListModel::AnchorNameRole).toString(),
             QStringLiteral("Fake Anchor"));
    QCOMPARE(controller.rooms()->data(room, RoomListModel::TitleRole).toString(),
             QStringLiteral("Fake Room"));
    QCOMPARE(controller.rooms()->data(room, RoomListModel::ViewerLabelRole).toString(),
             QStringLiteral("1,234"));
    QCOMPARE(controller.rooms()->data(room, RoomListModel::AvatarUrlRole).toUrl(),
             QUrl(QStringLiteral("https://example.invalid/avatar.jpg")));
    QTRY_COMPARE_WITH_TIMEOUT(controller.workspace()->lastMessage(),
                              QStringLiteral("房间数据已更新"),
                              3000);
    QCOMPARE(controller.workspace()->lastMessageLevel(), QStringLiteral("success"));
}

void AppControllerTest::publishesCommandFailureToToast()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.retryPlayback(QStringLiteral("999")), QStringLiteral("未找到该房间"));
    QCOMPARE(controller.workspace()->lastMessage(), QStringLiteral("未找到该房间"));
}

QTEST_GUILESS_MAIN(AppControllerTest)

#include "app_controller_test.moc"
