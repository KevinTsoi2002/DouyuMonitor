#include <QSignalSpy>
#include <QWidget>
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
    void releasesSessionsWithoutDanglingSnapshotAccess();
};

void MultiRoomCoordinatorTest::acceptsNineRoomsAndRejectsTheTenth()
{
    StreamgetProcessClient client(fakeServicePath());
    QWidget host;
    MultiRoomCoordinator coordinator(&client, &host);

    for (int index = 0; index < 9; ++index) QVERIFY(coordinator.addRoom(roomId(index)));
    QCOMPARE(coordinator.roomCount(), 9);
    QCOMPARE(coordinator.layoutId(), QStringLiteral("grid-3x3"));
    QVERIFY(!coordinator.addRoom(QStringLiteral("999999")));
    QCOMPARE(coordinator.roomCount(), 9);
    client.shutdown();
}

void MultiRoomCoordinatorTest::rejectsDuplicateRoomIds()
{
    StreamgetProcessClient client(fakeServicePath());
    QWidget host;
    MultiRoomCoordinator coordinator(&client, &host);

    QVERIFY(coordinator.addRoom(QStringLiteral("63136")));
    QVERIFY(!coordinator.addRoom(QStringLiteral("63136")));
    QVERIFY(!coordinator.addRoom(QStringLiteral("https://example.invalid")));
    QCOMPARE(coordinator.roomCount(), 1);
    client.shutdown();
}

void MultiRoomCoordinatorTest::appliesUserQualityAtFourRooms()
{
    StreamgetProcessClient client(fakeServicePath());
    QWidget host;
    MultiRoomCoordinator coordinator(&client, &host);

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
    QWidget host;
    MultiRoomCoordinator coordinator(&client, &host);

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
    QWidget host;
    MultiRoomCoordinator coordinator(&client, &host);
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
    QWidget host;
    MultiRoomCoordinator coordinator(&client, &host);

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
    QWidget host;
    MultiRoomCoordinator coordinator(&client, &host);

    QVERIFY(coordinator.addRoom(QStringLiteral("63136")));
    QVERIFY(coordinator.addRoom(QStringLiteral("63137")));
    QVERIFY(coordinator.addRoom(QStringLiteral("63138")));
    QVERIFY(coordinator.removeRoom(QStringLiteral("63137")));
    QCOMPARE(coordinator.roomIds(), QStringList({QStringLiteral("63136"), QStringLiteral("63138")}));
    QCOMPARE(coordinator.layoutId(), QStringLiteral("grid-2x2"));
    QVERIFY(coordinator.surfaceForRoom(QStringLiteral("63137")) == nullptr);
    client.shutdown();
}

void MultiRoomCoordinatorTest::oneRoomFailureDoesNotBlockOtherRooms()
{
    StreamgetProcessClient client(fakeServicePath(),
                                  {QStringLiteral("--offline-after"), QStringLiteral("1")});
    QWidget host;
    MultiRoomCoordinator coordinator(&client, &host);
    QSignalSpy failures(&coordinator, &MultiRoomCoordinator::failed);

    QVERIFY(coordinator.addRoom(QStringLiteral("63136")));
    QVERIFY(coordinator.addRoom(QStringLiteral("63137")));
    QTRY_COMPARE_WITH_TIMEOUT(coordinator.sessionForRoom(QStringLiteral("63136"))->liveStatus(),
                              RoomLiveStatus::Offline, 3000);
    QCOMPARE(failures.count(), 0);
    QCOMPARE(coordinator.sessionForRoom(QStringLiteral("63136"))->playbackHealth(),
             RoomPlaybackHealth::Pending);
    QCOMPARE(coordinator.roomCount(), 2);
    QVERIFY(coordinator.surfaceForRoom(QStringLiteral("63137")) != nullptr);
    client.shutdown();
}

void MultiRoomCoordinatorTest::returnsSpecificResultsForManagementCommands()
{
    StreamgetProcessClient client(fakeServicePath());
    QWidget host;
    MultiRoomCoordinator coordinator(&client, &host);
    MultiRoomCoordinator unavailable(nullptr, &host);

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
    QWidget host;
    MultiRoomCoordinator coordinator(&client, &host);
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
    QWidget host;
    MultiRoomCoordinator coordinator(&client, &host);

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
    QWidget host;
    MultiRoomCoordinator coordinator(&client, &host);

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
    QWidget host;
    MultiRoomCoordinator coordinator(&client, &host);

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

void MultiRoomCoordinatorTest::releasesSessionsWithoutDanglingSnapshotAccess()
{
    StreamgetProcessClient client(fakeServicePath());
    QWidget host;
    auto *coordinator = new MultiRoomCoordinator(&client, &host);

    for (int index = 0; index < MultiRoomCoordinator::kMaxRooms; ++index) {
        QCOMPARE(coordinator->addRoomDetailed(roomId(index)), RoomCommandResult::Accepted);
    }
    client.shutdown();
    delete coordinator;
}

QTEST_MAIN(MultiRoomCoordinatorTest)

#include "multi_room_coordinator_test.moc"
