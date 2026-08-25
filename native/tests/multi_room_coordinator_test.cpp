#include <QSignalSpy>
#include <QWidget>
#include <QtTest/QtTest>

#include "service/streamget_process_client.h"
#include "workspace/multi_room_coordinator.h"

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
    QTRY_VERIFY_WITH_TIMEOUT(failures.count() >= 1, 3000);
    QCOMPARE(failures.at(0).at(0).toString(), QStringLiteral("63136"));
    QCOMPARE(coordinator.roomCount(), 2);
    QVERIFY(coordinator.surfaceForRoom(QStringLiteral("63137")) != nullptr);
    client.shutdown();
}

QTEST_MAIN(MultiRoomCoordinatorTest)

#include "multi_room_coordinator_test.moc"
