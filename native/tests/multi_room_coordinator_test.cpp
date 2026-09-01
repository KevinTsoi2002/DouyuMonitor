#include <QSignalSpy>
#include "ui/mpv_quick_item.h"
#include <QtTest/QtTest>

#include "service/streamget_process_client.h"
#include "workspace/multi_room_coordinator.h"
#include "workspace/room_workspace_types.h"

#ifndef FAKE_STREAMGET_SERVICE_PATH
#define FAKE_STREAMGET_SERVICE_PATH "fake_streamget_service"
#endif

namespace {

QString fakeServicePath()
{
    return QString::fromLocal8Bit(FAKE_STREAMGET_SERVICE_PATH);
}

QString roomId(int index)
{
    return QString::number(63136 + index);
}

} // namespace

class MultiRoomCoordinatorTest final : public QObject {
    Q_OBJECT

private slots:
    void acceptsNineRoomsAndRejectsTheTenth();
    void addsNineRoomsWithoutAWidgetParent();
    void movesAndRetriesOnlyTheRequestedRoom();
    void rejectsDuplicateRoomIds();
    void appliesUserQualityAtFourRooms();
    void appliesPrimaryOriginalAndOthers720pAtFiveRooms();
    void switchingPrimaryOnlyReloadsAffectedRooms();
    void droppingToFourRoomsRestoresUserQuality();
    void removesRoomAndReflowsOrder();
    void oneRoomFailureDoesNotBlockOtherRooms();
    void returnsSpecificResultsForManagementCommands();
    void publishesOrderedSnapshotsWithPolicyOverrides();
    void restoresChangedRequestedQualityAfterDroppingToFourRooms();
    void togglesFavoriteAndPublishesIt();
    void appliesSingleAudioFocusAndPublishesIt();
    void appliesAudioModesAndGlobalMute();
    void defaultsSingleAudioFocusToFirstRoom();
    void releasesSessionsWithoutDanglingSnapshotAccess();
    void replaysWhenMetadataChangesOfflineToOnline();
    void stopsWhenMetadataChangesOnlineToOffline();
    void publishesSnapshotWhenResolveMarksRoomOnline();
    void refreshesRoomMetadataImmediatelyAfterAdd();
    void replacesRoomsInRequestedOrderAndReleasesRemovedSessions();
    void preservesManualLayoutWhenRoomCountChanges();
    void reducesLayoutModesAndMigratesLegacyChoices();
};

void MultiRoomCoordinatorTest::acceptsNineRoomsAndRejectsTheTenth()
{
    StreamgetProcessClient client(fakeServicePath());
    MultiRoomCoordinator coordinator(&client);

    for (int index = 0; index < 9; ++index) QVERIFY(coordinator.addRoom(roomId(index)));
    QCOMPARE(coordinator.roomCount(), 9);
    QCOMPARE(coordinator.layoutId(), QStringLiteral("auto"));
    QVERIFY(!coordinator.addRoom(QStringLiteral("999999")));
    QCOMPARE(coordinator.roomCount(), 9);
    client.shutdown();
}

void MultiRoomCoordinatorTest::addsNineRoomsWithoutAWidgetParent()
{
    StreamgetProcessClient client(fakeServicePath());
    MultiRoomCoordinator coordinator(&client);

    for (int index = 0; index < MultiRoomCoordinator::kMaxRooms; ++index) {
        QCOMPARE(coordinator.addRoomDetailed(roomId(index)), RoomCommandResult::Accepted);
    }
    QCOMPARE(coordinator.addRoomDetailed(QStringLiteral("999999")),
             RoomCommandResult::RoomLimitReached);
    client.shutdown();
}

void MultiRoomCoordinatorTest::movesAndRetriesOnlyTheRequestedRoom()
{
    StreamgetProcessClient client(fakeServicePath());
    MultiRoomCoordinator coordinator(&client);

    QCOMPARE(coordinator.addRoomDetailed(QStringLiteral("63136")), RoomCommandResult::Accepted);
    QCOMPARE(coordinator.addRoomDetailed(QStringLiteral("63137")), RoomCommandResult::Accepted);
    QCOMPARE(coordinator.moveRoomDetailed(QStringLiteral("63137"), -1),
             RoomCommandResult::Accepted);
    QCOMPARE(coordinator.roomIds(), QStringList({QStringLiteral("63137"), QStringLiteral("63136")}));
    QCOMPARE(coordinator.setVolume(QStringLiteral("63137"), 37), RoomCommandResult::Accepted);
    QCOMPARE(coordinator.roomSnapshots().at(0).volume, 37);
    QCOMPARE(coordinator.retryRoomDetailed(QStringLiteral("63137")), RoomCommandResult::Accepted);
    client.shutdown();
}

void MultiRoomCoordinatorTest::rejectsDuplicateRoomIds()
{
    StreamgetProcessClient client(fakeServicePath());
    MultiRoomCoordinator coordinator(&client);

    QVERIFY(coordinator.addRoom(QStringLiteral("63136")));
    QVERIFY(!coordinator.addRoom(QStringLiteral("63136")));
    QVERIFY(!coordinator.addRoom(QStringLiteral("not-a-room-id")));
    QCOMPARE(coordinator.roomCount(), 1);
    client.shutdown();
}

void MultiRoomCoordinatorTest::appliesUserQualityAtFourRooms()
{
    StreamgetProcessClient client(fakeServicePath());
    MultiRoomCoordinator coordinator(&client);

    QVERIFY(coordinator.addRoom(QStringLiteral("63136"), StreamQuality::High));
    QVERIFY(coordinator.addRoom(QStringLiteral("63137"), StreamQuality::Original));
    QVERIFY(coordinator.addRoom(QStringLiteral("63138"), StreamQuality::Standard));
    QVERIFY(coordinator.addRoom(QStringLiteral("63139"), StreamQuality::Auto));
    QCOMPARE(coordinator.effectiveQuality(QStringLiteral("63136")), StreamQuality::High);
    QCOMPARE(coordinator.effectiveQuality(QStringLiteral("63137")), StreamQuality::Original);
    QCOMPARE(coordinator.effectiveQuality(QStringLiteral("63138")), StreamQuality::Standard);
    QCOMPARE(coordinator.effectiveQuality(QStringLiteral("63139")), StreamQuality::Auto);
    client.shutdown();
}

void MultiRoomCoordinatorTest::appliesPrimaryOriginalAndOthers720pAtFiveRooms()
{
    StreamgetProcessClient client(fakeServicePath());
    MultiRoomCoordinator coordinator(&client);

    for (int index = 0; index < 5; ++index) {
        QVERIFY(coordinator.addRoom(roomId(index), StreamQuality::High));
    }
    QCOMPARE(coordinator.primaryRoomId(), roomId(0));
    QCOMPARE(coordinator.effectiveQuality(roomId(0)), StreamQuality::Original);
    for (int index = 1; index < 5; ++index) {
        QCOMPARE(coordinator.effectiveQuality(roomId(index)), StreamQuality::Standard);
    }
    client.shutdown();
}

void MultiRoomCoordinatorTest::switchingPrimaryOnlyReloadsAffectedRooms()
{
    StreamgetProcessClient client(fakeServicePath());
    MultiRoomCoordinator coordinator(&client);
    QSignalSpy qualityChanges(&coordinator, &MultiRoomCoordinator::qualityChanged);

    for (int index = 0; index < 5; ++index) QVERIFY(coordinator.addRoom(roomId(index)));
    qualityChanges.clear();
    QVERIFY(coordinator.setPrimaryRoom(roomId(3)));
    QCOMPARE(coordinator.primaryRoomId(), roomId(3));
    QCOMPARE(coordinator.effectiveQuality(roomId(3)), StreamQuality::Original);
    QCOMPARE(coordinator.effectiveQuality(roomId(0)), StreamQuality::Standard);
    QCOMPARE(qualityChanges.count(), 2);
    client.shutdown();
}

void MultiRoomCoordinatorTest::droppingToFourRoomsRestoresUserQuality()
{
    StreamgetProcessClient client(fakeServicePath());
    MultiRoomCoordinator coordinator(&client);

    for (int index = 0; index < 5; ++index) {
        QVERIFY(coordinator.addRoom(roomId(index), StreamQuality::High));
    }
    QVERIFY(coordinator.removeRoom(roomId(4)));
    QCOMPARE(coordinator.roomCount(), 4);
    for (int index = 0; index < 4; ++index) {
        QCOMPARE(coordinator.effectiveQuality(roomId(index)), StreamQuality::High);
    }
    client.shutdown();
}

void MultiRoomCoordinatorTest::removesRoomAndReflowsOrder()
{
    StreamgetProcessClient client(fakeServicePath());
    MultiRoomCoordinator coordinator(&client);

    QVERIFY(coordinator.addRoom(QStringLiteral("63136")));
    QVERIFY(coordinator.addRoom(QStringLiteral("63137")));
    QVERIFY(coordinator.addRoom(QStringLiteral("63138")));
    QVERIFY(coordinator.removeRoom(QStringLiteral("63137")));
    QCOMPARE(coordinator.roomIds(), QStringList({QStringLiteral("63136"), QStringLiteral("63138")}));
    QCOMPARE(coordinator.layoutId(), QStringLiteral("auto"));
    QVERIFY(coordinator.sessionForRoom(QStringLiteral("63137")) == nullptr);
    client.shutdown();
}

void MultiRoomCoordinatorTest::oneRoomFailureDoesNotBlockOtherRooms()
{
    StreamgetProcessClient client(fakeServicePath(),
                                  {QStringLiteral("--delay-ms"), QStringLiteral("100"),
                                   QStringLiteral("--offline-after"), QStringLiteral("1")});
    MultiRoomCoordinator coordinator(&client);
    QSignalSpy failures(&coordinator, &MultiRoomCoordinator::failed);

    QVERIFY(coordinator.addRoom(QStringLiteral("63136")));
    QVERIFY(coordinator.addRoom(QStringLiteral("63137")));
    RoomSession *firstSession = coordinator.sessionForRoom(QStringLiteral("63136"));
    QVERIFY(firstSession != nullptr);
    QTRY_COMPARE_WITH_TIMEOUT(firstSession->state(), RoomSession::State::Idle, 3000);
    QCOMPARE(firstSession->liveStatus(), RoomLiveStatus::Online);
    QCOMPARE(failures.count(), 0);
    QCOMPARE(firstSession->playbackHealth(), RoomPlaybackHealth::Pending);
    QCOMPARE(coordinator.roomCount(), 2);
    QVERIFY(coordinator.sessionForRoom(QStringLiteral("63137")) != nullptr);
    client.shutdown();
}

void MultiRoomCoordinatorTest::returnsSpecificResultsForManagementCommands()
{
    StreamgetProcessClient client(fakeServicePath());
    MultiRoomCoordinator coordinator(&client);
    MultiRoomCoordinator unavailable(nullptr);

    QCOMPARE(unavailable.addRoomDetailed(QStringLiteral("63136")),
             RoomCommandResult::Unavailable);
    QCOMPARE(coordinator.addRoomDetailed(QStringLiteral("x")),
             RoomCommandResult::InvalidRoomId);
    QCOMPARE(coordinator.addRoomDetailed(QStringLiteral("63136")),
             RoomCommandResult::Accepted);
    QCOMPARE(coordinator.addRoomDetailed(QStringLiteral("63136")),
             RoomCommandResult::DuplicateRoomId);
    QCOMPARE(coordinator.removeRoomDetailed(QStringLiteral("63137")),
             RoomCommandResult::RoomNotFound);
    QCOMPARE(coordinator.setPrimaryRoomDetailed(QStringLiteral("63137")),
             RoomCommandResult::RoomNotFound);
    QCOMPARE(coordinator.setPrimaryRoomDetailed(QStringLiteral("63136")),
             RoomCommandResult::AlreadyPrimary);
    QCOMPARE(coordinator.setRequestedQuality(QStringLiteral("63137"), StreamQuality::High),
             RoomCommandResult::RoomNotFound);
    QCOMPARE(coordinator.setRequestedQuality(QStringLiteral("63136"), StreamQuality::Auto),
             RoomCommandResult::Unchanged);

    for (int index = 1; index < MultiRoomCoordinator::kMaxRooms; ++index) {
        QCOMPARE(coordinator.addRoomDetailed(roomId(index)), RoomCommandResult::Accepted);
    }
    QCOMPARE(coordinator.addRoomDetailed(QStringLiteral("999999")),
             RoomCommandResult::RoomLimitReached);
    client.shutdown();
}

void MultiRoomCoordinatorTest::publishesOrderedSnapshotsWithPolicyOverrides()
{
    StreamgetProcessClient client(fakeServicePath());
    MultiRoomCoordinator coordinator(&client);
    QSignalSpy snapshotChanges(&coordinator, &MultiRoomCoordinator::roomSnapshotsChanged);

    for (int index = 0; index < 5; ++index) {
        QCOMPARE(coordinator.addRoomDetailed(roomId(index), StreamQuality::High),
                 RoomCommandResult::Accepted);
    }

    QVERIFY(!snapshotChanges.isEmpty());
    const RoomSnapshots snapshots = snapshotChanges.last().at(0).value<RoomSnapshots>();
    QCOMPARE(snapshots.size(), 5);
    QCOMPARE(snapshots.at(0).roomId, roomId(0));
    QVERIFY(snapshots.at(0).isPrimary);
    QCOMPARE(snapshots.at(0).requestedQuality, StreamQuality::High);
    QCOMPARE(snapshots.at(0).effectiveQuality, StreamQuality::Original);
    QCOMPARE(snapshots.at(1).roomId, roomId(1));
    QVERIFY(!snapshots.at(1).isPrimary);
    QCOMPARE(snapshots.at(1).requestedQuality, StreamQuality::High);
    QCOMPARE(snapshots.at(1).effectiveQuality, StreamQuality::Standard);
    client.shutdown();
}

void MultiRoomCoordinatorTest::restoresChangedRequestedQualityAfterDroppingToFourRooms()
{
    StreamgetProcessClient client(fakeServicePath());
    MultiRoomCoordinator coordinator(&client);

    for (int index = 0; index < 5; ++index) {
        QCOMPARE(coordinator.addRoomDetailed(roomId(index), StreamQuality::High),
                 RoomCommandResult::Accepted);
    }
    QCOMPARE(coordinator.setRequestedQuality(roomId(1), StreamQuality::Super),
             RoomCommandResult::Accepted);
    QCOMPARE(coordinator.roomSnapshots().at(1).requestedQuality, StreamQuality::Super);
    QCOMPARE(coordinator.roomSnapshots().at(1).effectiveQuality, StreamQuality::Standard);

    QCOMPARE(coordinator.removeRoomDetailed(roomId(4)), RoomCommandResult::Accepted);
    QCOMPARE(coordinator.roomSnapshots().size(), 4);
    QCOMPARE(coordinator.roomSnapshots().at(1).requestedQuality, StreamQuality::Super);
    QCOMPARE(coordinator.roomSnapshots().at(1).effectiveQuality, StreamQuality::Super);
    client.shutdown();
}

void MultiRoomCoordinatorTest::togglesFavoriteAndPublishesIt()
{
    StreamgetProcessClient client(fakeServicePath());
    MultiRoomCoordinator coordinator(&client);

    QVERIFY(coordinator.addRoom(QStringLiteral("63136")));
    QVERIFY(coordinator.setFavorite(QStringLiteral("63136"), true));
    QVERIFY(coordinator.roomSnapshots().at(0).favorite);
    QVERIFY(!coordinator.setFavorite(QStringLiteral("63136"), true));
    QVERIFY(coordinator.setFavorite(QStringLiteral("63136"), false));
    QVERIFY(!coordinator.roomSnapshots().at(0).favorite);
    client.shutdown();
}

void MultiRoomCoordinatorTest::appliesSingleAudioFocusAndPublishesIt()
{
    StreamgetProcessClient client(fakeServicePath());
    MultiRoomCoordinator coordinator(&client);

    QVERIFY(coordinator.addRoom(QStringLiteral("63136")));
    QVERIFY(coordinator.addRoom(QStringLiteral("63137")));
    QVERIFY(coordinator.setAudioFocus(QStringLiteral("63137")));

    const RoomSnapshots snapshots = coordinator.roomSnapshots();
    QVERIFY(!snapshots.at(0).audioFocused);
    QVERIFY(snapshots.at(1).audioFocused);
    QVERIFY(snapshots.at(0).muted);
    QVERIFY(!snapshots.at(1).muted);
    client.shutdown();
}

void MultiRoomCoordinatorTest::appliesAudioModesAndGlobalMute()
{
    StreamgetProcessClient client(fakeServicePath());
    MultiRoomCoordinator coordinator(&client);

    QCOMPARE(coordinator.addRoomDetailed(QStringLiteral("63136")), RoomCommandResult::Accepted);
    QCOMPARE(coordinator.addRoomDetailed(QStringLiteral("63137")), RoomCommandResult::Accepted);
    QVERIFY(coordinator.setAudioFocus(QStringLiteral("63136")));

    QCOMPARE(coordinator.audioMode(), QStringLiteral("single"));
    QVERIFY(coordinator.setAudioMode(QStringLiteral("multi")));
    QVERIFY(!coordinator.roomSnapshots().at(0).muted);
    QVERIFY(!coordinator.roomSnapshots().at(1).muted);

    QVERIFY(coordinator.setGlobalMuted(true));
    QVERIFY(coordinator.roomSnapshots().at(0).muted);
    QVERIFY(coordinator.roomSnapshots().at(1).muted);
    QCOMPARE(coordinator.audioRoomId(), QStringLiteral("63136"));
    client.shutdown();
}

void MultiRoomCoordinatorTest::defaultsSingleAudioFocusToFirstRoom()
{
    StreamgetProcessClient client(fakeServicePath());
    MultiRoomCoordinator coordinator(&client);
    QCOMPARE(coordinator.addRoomDetailed(QStringLiteral("63136")), RoomCommandResult::Accepted);
    QCOMPARE(coordinator.addRoomDetailed(QStringLiteral("63137")), RoomCommandResult::Accepted);
    QCOMPARE(coordinator.audioRoomId(), QStringLiteral("63136"));
    QVERIFY(coordinator.roomSnapshots().at(0).audioFocused);
    QVERIFY(!coordinator.roomSnapshots().at(0).muted);
    QVERIFY(coordinator.roomSnapshots().at(1).muted);

    QCOMPARE(coordinator.removeRoomDetailed(QStringLiteral("63136")), RoomCommandResult::Accepted);
    QCOMPARE(coordinator.audioRoomId(), QStringLiteral("63137"));
    QVERIFY(coordinator.roomSnapshots().at(0).audioFocused);
    client.shutdown();
}

void MultiRoomCoordinatorTest::releasesSessionsWithoutDanglingSnapshotAccess()
{
    StreamgetProcessClient client(fakeServicePath());
    auto *coordinator = new MultiRoomCoordinator(&client);

    for (int index = 0; index < MultiRoomCoordinator::kMaxRooms; ++index) {
        QCOMPARE(coordinator->addRoomDetailed(roomId(index)), RoomCommandResult::Accepted);
    }
    client.shutdown();
    delete coordinator;
}

void MultiRoomCoordinatorTest::replaysWhenMetadataChangesOfflineToOnline()
{
    StreamgetProcessClient client(fakeServicePath(),
                                  {QStringLiteral("--search-script"),
                                   QStringLiteral("offline,online")});
    const RoomRefreshTiming timing{.onlineIntervalMs = 1000,
                                   .offlineIntervalMs = 1000,
                                   .retryDelaysMs = {5, 10, 20, 40},
                                   .jitterPercent = 0};
    MultiRoomCoordinator coordinator(&client, timing);
    QVERIFY(coordinator.addRoom(QStringLiteral("63136")));
    MpvQuickItem player;
    QVERIFY(coordinator.attachPlayer(QStringLiteral("63136"), &player));
    coordinator.refreshRoomStatusNow(QStringLiteral("63136"));
    QTRY_COMPARE_WITH_TIMEOUT(coordinator.roomSnapshots().at(0).liveStatus,
                              RoomLiveStatus::Offline, 3000);

    RoomSession *session = coordinator.sessionForRoom(QStringLiteral("63136"));
    QVERIFY(session != nullptr);
    QCOMPARE(session->playbackHealth(), RoomPlaybackHealth::Pending);
    QSignalSpy stateChanges(session, &RoomSession::stateChanged);
    QSignalSpy sourceReady(session, &RoomSession::sourceReady);

    coordinator.refreshRoomStatusNow(QStringLiteral("63136"));
    QTRY_COMPARE_WITH_TIMEOUT(coordinator.roomSnapshots().at(0).liveStatus,
                              RoomLiveStatus::Online, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(sourceReady.count() == 1, 3000);
    bool sawReady = false;
    for (const QList<QVariant> &arguments : stateChanges) {
        if (arguments.isEmpty()) continue;
        if (qvariant_cast<RoomSession::State>(arguments.at(0)) == RoomSession::State::Ready) {
            sawReady = true;
            break;
        }
    }
    QVERIFY(sawReady);
    QVERIFY(stateChanges.count() >= 2);
    client.shutdown();
}

void MultiRoomCoordinatorTest::stopsWhenMetadataChangesOnlineToOffline()
{
    StreamgetProcessClient client(fakeServicePath(),
                                  {QStringLiteral("--search-script"),
                                   QStringLiteral("online,offline")});
    const RoomRefreshTiming timing{.onlineIntervalMs = 1000,
                                   .offlineIntervalMs = 1000,
                                   .retryDelaysMs = {5, 10, 20, 40},
                                   .jitterPercent = 0};
    MultiRoomCoordinator coordinator(&client, timing);
    QSignalSpy removed(&coordinator, &MultiRoomCoordinator::roomRemoved);
    QVERIFY(coordinator.addRoom(QStringLiteral("63136")));
    MpvQuickItem player;
    QVERIFY(coordinator.attachPlayer(QStringLiteral("63136"), &player));
    coordinator.refreshRoomStatusNow(QStringLiteral("63136"));
    QTRY_COMPARE_WITH_TIMEOUT(coordinator.roomSnapshots().at(0).liveStatus,
                              RoomLiveStatus::Online, 3000);

    coordinator.refreshRoomStatusNow(QStringLiteral("63136"));
    QTRY_COMPARE_WITH_TIMEOUT(coordinator.roomSnapshots().at(0).liveStatus,
                              RoomLiveStatus::Offline, 3000);
    QTest::qWait(100);
    QCOMPARE(player.playbackState(), MpvQuickItem::PlaybackState::Idle);
    QCOMPARE(coordinator.sessionForRoom(QStringLiteral("63136"))->playbackHealth(),
             RoomPlaybackHealth::Pending);
    QCOMPARE(coordinator.roomIds(), QStringList({QStringLiteral("63136")}));
    QCOMPARE(removed.count(), 0);
    client.shutdown();
}

void MultiRoomCoordinatorTest::publishesSnapshotWhenResolveMarksRoomOnline()
{
    StreamgetProcessClient client(fakeServicePath());
    MultiRoomCoordinator coordinator(&client);
    QSignalSpy snapshotChanges(&coordinator, &MultiRoomCoordinator::roomSnapshotsChanged);

    QVERIFY(coordinator.addRoom(QStringLiteral("63136")));
    QTRY_COMPARE_WITH_TIMEOUT(coordinator.roomSnapshots().at(0).liveStatus,
                              RoomLiveStatus::Online, 3000);
    bool sawOnlineSnapshot = false;
    for (const QList<QVariant> &arguments : snapshotChanges) {
        if (arguments.isEmpty()) continue;
        const RoomSnapshots snapshots = arguments.at(0).value<RoomSnapshots>();
        if (!snapshots.isEmpty() && snapshots.first().liveStatus == RoomLiveStatus::Online) {
            sawOnlineSnapshot = true;
            break;
        }
    }
    QVERIFY(sawOnlineSnapshot);
    client.shutdown();
}

void MultiRoomCoordinatorTest::refreshesRoomMetadataImmediatelyAfterAdd()
{
    StreamgetProcessClient client(fakeServicePath());
    MultiRoomCoordinator coordinator(&client);

    QCOMPARE(coordinator.addRoomDetailed(QStringLiteral("63136")),
             RoomCommandResult::Accepted);
    QTRY_COMPARE_WITH_TIMEOUT(coordinator.roomSnapshots().at(0).metadata.anchorName,
                              QStringLiteral("Fake Anchor"), 3000);
    QCOMPARE(coordinator.roomSnapshots().at(0).metadata.title,
             QStringLiteral("Fake Room"));
    client.shutdown();
}

void MultiRoomCoordinatorTest::replacesRoomsInRequestedOrderAndReleasesRemovedSessions()
{
    StreamgetProcessClient client(fakeServicePath());
    MultiRoomCoordinator coordinator(&client);

    QCOMPARE(coordinator.addRoomDetailed(QStringLiteral("63136")),
             RoomCommandResult::Accepted);
    QCOMPARE(coordinator.addRoomDetailed(QStringLiteral("63137")),
             RoomCommandResult::Accepted);
    MpvQuickItem removedPlayer;
    QVERIFY(coordinator.attachPlayer(QStringLiteral("63137"), &removedPlayer));

    const QVector<CoordinatorRoomSpec> target{
        {QStringLiteral("63138"), StreamQuality::High, {}, 42, true},
        {QStringLiteral("63136"), StreamQuality::Auto, {}, 88, false},
    };
    QCOMPARE(coordinator.replaceRooms(target), RoomCommandResult::Accepted);
    QCOMPARE(coordinator.roomIds(), QStringList({QStringLiteral("63138"), QStringLiteral("63136")}));
    QVERIFY(coordinator.sessionForRoom(QStringLiteral("63137")) == nullptr);
    QCOMPARE(removedPlayer.playbackState(), MpvQuickItem::PlaybackState::Idle);
    QCOMPARE(coordinator.primaryRoomId(), QStringLiteral("63138"));
    QCOMPARE(coordinator.roomSnapshots().at(0).volume, 42);
    QVERIFY(coordinator.roomSnapshots().at(0).favorite);
    client.shutdown();
}

void MultiRoomCoordinatorTest::preservesManualLayoutWhenRoomCountChanges()
{
    StreamgetProcessClient client(fakeServicePath());
    MultiRoomCoordinator coordinator(&client);

    QVERIFY(coordinator.addRoom(QStringLiteral("63136")));
    QVERIFY(coordinator.addRoom(QStringLiteral("63137")));
    bool changed = false;
    QVERIFY(QMetaObject::invokeMethod(&coordinator,
                                      "setLayout",
                                      Q_RETURN_ARG(bool, changed),
                                      Q_ARG(QString, QStringLiteral("primary"))));
    QVERIFY(changed);
    QCOMPARE(coordinator.layoutId(), QStringLiteral("primary"));

    QVERIFY(coordinator.addRoom(QStringLiteral("63138")));
    QCOMPARE(coordinator.layoutId(), QStringLiteral("primary"));

    changed = true;
    QVERIFY(QMetaObject::invokeMethod(&coordinator,
                                      "setLayout",
                                      Q_RETURN_ARG(bool, changed),
                                      Q_ARG(QString, QStringLiteral("unsupported"))));
    QVERIFY(!changed);
    client.shutdown();
}

void MultiRoomCoordinatorTest::reducesLayoutModesAndMigratesLegacyChoices()
{
    StreamgetProcessClient client(fakeServicePath());
    MultiRoomCoordinator coordinator(&client);

    QVERIFY(!coordinator.setLayout(QStringLiteral("auto")));
    QCOMPARE(coordinator.layoutMode(), QStringLiteral("auto"));
    QCOMPARE(coordinator.layoutId(), QStringLiteral("auto"));

    QVERIFY(coordinator.setLayout(QStringLiteral("primary-two")));
    QCOMPARE(coordinator.layoutMode(), QStringLiteral("primary"));
    QCOMPARE(coordinator.layoutId(), QStringLiteral("primary"));

    QVERIFY(coordinator.setLayout(QStringLiteral("grid-3x3")));
    QCOMPARE(coordinator.layoutMode(), QStringLiteral("auto"));
    QCOMPARE(coordinator.layoutId(), QStringLiteral("auto"));

    QVERIFY(!coordinator.setLayout(QStringLiteral("unsupported")));
    client.shutdown();
}

QTEST_GUILESS_MAIN(MultiRoomCoordinatorTest)

#include "multi_room_coordinator_test.moc"
