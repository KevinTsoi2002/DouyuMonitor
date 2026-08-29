#include <QSignalSpy>
#include <QtTest/QtTest>

#include "workspace/room_status_scheduler.h"

namespace {

RoomSnapshot snapshot(const QString &roomId, RoomLiveStatus liveStatus)
{
    RoomSnapshot room;
    room.roomId = roomId;
    room.liveStatus = liveStatus;
    return room;
}

} // namespace

class RoomStatusSchedulerTest final : public QObject {
    Q_OBJECT

private slots:
    void usesIntervalsAndPreventsConcurrentRequests();
    void cancelsRemovedRoomsAndRejectsLateResponses();
    void backsOffFailuresAndResetsAfterSuccess();
    void rejectsCompletionFromRemovedAndReaddedRoom();
};

void RoomStatusSchedulerTest::usesIntervalsAndPreventsConcurrentRequests()
{
    QVector<QString> calls;
    const RoomRefreshTiming timing{
        .onlineIntervalMs = 20,
        .offlineIntervalMs = 40,
        .retryDelaysMs = {5, 10, 20, 40},
        .jitterPercent = 0,
    };
    RoomStatusScheduler scheduler(
        [&calls](const QString &roomId) {
            calls.push_back(roomId);
            return static_cast<quint64>(calls.size());
        },
        [](quint64) {}, timing);

    scheduler.synchronize({snapshot(QStringLiteral("63136"), RoomLiveStatus::Online),
                           snapshot(QStringLiteral("63137"), RoomLiveStatus::Offline)});
    QCOMPARE(calls.count(QStringLiteral("63137")), 0);
    scheduler.requestNow(QStringLiteral("63136"));
    scheduler.requestNow(QStringLiteral("63136"));
    QCOMPARE(calls.count(QStringLiteral("63136")), 1);
    QCOMPARE(scheduler.takeCompletedRequest(1, true), std::optional<QString>{QStringLiteral("63136")});

    QTRY_COMPARE_WITH_TIMEOUT(calls.count(QStringLiteral("63136")), 2, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(calls.count(QStringLiteral("63137")), 1, 1000);
}

void RoomStatusSchedulerTest::cancelsRemovedRoomsAndRejectsLateResponses()
{
    QVector<quint64> cancelled;
    const RoomRefreshTiming timing{.onlineIntervalMs = 20,
                                   .offlineIntervalMs = 40,
                                   .retryDelaysMs = {5, 10, 20, 40},
                                   .jitterPercent = 0};
    RoomStatusScheduler scheduler(
        [](const QString &) { return quint64{1}; },
        [&cancelled](quint64 requestId) { cancelled.push_back(requestId); }, timing);

    scheduler.synchronize({snapshot(QStringLiteral("63136"), RoomLiveStatus::Online)});
    scheduler.requestNow(QStringLiteral("63136"));
    scheduler.synchronize({});

    QCOMPARE(cancelled, QVector<quint64>{1});
    QVERIFY(!scheduler.takeCompletedRequest(1, true).has_value());
}

void RoomStatusSchedulerTest::backsOffFailuresAndResetsAfterSuccess()
{
    QVector<quint64> requests;
    const RoomRefreshTiming timing{.onlineIntervalMs = 1000,
                                   .offlineIntervalMs = 1000,
                                   .retryDelaysMs = {5, 10, 20, 40},
                                   .jitterPercent = 0};
    RoomStatusScheduler scheduler(
        [&requests](const QString &) {
            requests.push_back(static_cast<quint64>(requests.size() + 1));
            return requests.back();
        },
        [](quint64) {}, timing);

    scheduler.synchronize({snapshot(QStringLiteral("63136"), RoomLiveStatus::Online)});
    scheduler.requestNow(QStringLiteral("63136"));
    QVERIFY(scheduler.takeCompletedRequest(1, false).has_value());
    QTRY_COMPARE_WITH_TIMEOUT(requests.size(), 2, 500);
    QVERIFY(scheduler.takeCompletedRequest(2, true).has_value());
    QTest::qWait(20);
    QCOMPARE(requests.size(), 2);
    QTRY_COMPARE_WITH_TIMEOUT(requests.size(), 3, 1200);
}

void RoomStatusSchedulerTest::rejectsCompletionFromRemovedAndReaddedRoom()
{
    quint64 nextRequestId = 0;
    RoomStatusScheduler scheduler(
        [&nextRequestId](const QString &) { return ++nextRequestId; },
        [](quint64) {},
        RoomRefreshTiming{.onlineIntervalMs = 1000,
                          .offlineIntervalMs = 1000,
                          .retryDelaysMs = {5},
                          .jitterPercent = 0});

    scheduler.synchronize({snapshot(QStringLiteral("63136"), RoomLiveStatus::Online)});
    scheduler.requestNow(QStringLiteral("63136"));
    scheduler.synchronize({});
    scheduler.synchronize({snapshot(QStringLiteral("63136"), RoomLiveStatus::Online)});
    scheduler.requestNow(QStringLiteral("63136"));

    QVERIFY(!scheduler.takeCompletedRequest(1, true).has_value());
    QCOMPARE(scheduler.takeCompletedRequest(1, false), std::optional<QString>{});
}

QTEST_GUILESS_MAIN(RoomStatusSchedulerTest)

#include "room_status_scheduler_test.moc"
