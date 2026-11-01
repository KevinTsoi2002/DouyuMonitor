#include <QtTest/QtTest>
#include <QPointer>

#include <algorithm>
#include <memory>

#include "danmaku/danmaku_session_manager.h"

class TrackingScheduler final : public QObject, public DanmakuTimerScheduler {
public:
    explicit TrackingScheduler(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    TimerId once(int, Callback) override { return nextId_++; }
    TimerId repeating(int, Callback) override { return nextId_++; }
    void cancel(TimerId) override {}

private:
    TimerId nextId_ = 1;
};

class NullDanmakuSocket final : public DanmakuSocket {
public:
    using DanmakuSocket::DanmakuSocket;

    void connectTo(const QUrl &, quint64) override {}
    void sendBinary(const QByteArray &) override {}
    void close() override {}
};

class FakeDanmakuClient final : public DanmakuClient {
    Q_OBJECT

public:
    explicit FakeDanmakuClient(const QString &roomId, QObject *parent = nullptr)
        : DanmakuClient(parent)
        , roomId_(roomId)
    {
    }

    void start() override
    {
        ++starts;
        emit statusChanged({roomId_, DanmakuConnectionState::Connecting, 0,
                            DanmakuErrorCode::None});
    }

    void stop() override
    {
        ++stops;
        if (externalStops) ++*externalStops;
        emit statusChanged({roomId_, DanmakuConnectionState::Idle, 0,
                            DanmakuErrorCode::None});
    }

    void retry() override { ++retries; }

    void emitChat(const QString &id, const QString &text)
    {
        emit chatReceived(roomId_, {{QStringLiteral("type"), QStringLiteral("chatmsg")},
                                    {QStringLiteral("rid"), roomId_},
                                    {QStringLiteral("cid"), id},
                                    {QStringLiteral("nn"), QStringLiteral("主播")},
                                    {QStringLiteral("txt"), text}});
    }

    int starts = 0;
    int stops = 0;
    int retries = 0;
    int *externalStops = nullptr;

private:
    QString roomId_;
};

class DanmakuSessionManagerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsDefaultBudgetByPriority();
    void capsSessionsAtTheDefaultBudget();
    void stopsRoomsThatBecomeIneligible();
    void reportsIneligibleRoomsAsIdle();
    void deduplicatesAndBoundsEachRoomQueue();
    void appliesGovernanceBeforeQueueing();
    void clearsQueuesAndStopsAllOnShutdown();
    void handlesReentrantClearDuringStop();
    void promotesWaitingRoomWhenPriorityChanges();
    void preservesTargetSessionsAcrossRepeatedSync();
    void replacesIneligibleSessionWithWaitingRoom();
    void deduplicatesLongIdsWithoutPrefixCollision();
    void evictsDeduplicationKeysInFifoOrder();
    void coalescesStatisticsNotificationsPerRoom();
    void releasesSchedulerWhenSessionIsRemoved();
};

namespace {

DanmakuRoomEligibility eligible(const QString &roomId)
{
    DanmakuRoomEligibility room;
    room.roomId = roomId;
    room.active = true;
    room.roomEnabled = true;
    room.globalEnabled = true;
    room.live = true;
    return room;
}

} // namespace

void DanmakuSessionManagerTest::startsDefaultBudgetByPriority()
{
    QVector<FakeDanmakuClient *> fakes;
    QVector<QString> startedRooms;
    DanmakuSessionManager manager(
        [&](const QString &roomId, QObject *) -> std::unique_ptr<DanmakuClient> {
            auto fake = std::make_unique<FakeDanmakuClient>(roomId);
            fakes.push_back(fake.get());
            startedRooms.push_back(roomId);
            return fake;
        });
    QVector<DanmakuRoomEligibility> rooms;
    for (int i = 0; i < 16; ++i) {
        auto room = eligible(QString::number(i + 1));
        room.priority = i;
        rooms.push_back(room);
    }
    std::swap(rooms[0].priority, rooms[15].priority);

    manager.synchronize(rooms);
    QCOMPARE(manager.activeSessionCount(), 12);
    QCOMPARE(fakes.size(), 12);
    QVERIFY(startedRooms.contains(QStringLiteral("16")));
    QVERIFY(!startedRooms.contains(QStringLiteral("1")));
    QCOMPARE(manager.statusForRoom(QStringLiteral("1")).state,
             DanmakuConnectionState::WaitingForSession);
}

void DanmakuSessionManagerTest::capsSessionsAtTheDefaultBudget()
{
    QVector<QString> startedRooms;
    DanmakuSessionManager manager(
        [&](const QString &roomId, QObject *) -> std::unique_ptr<DanmakuClient> {
            startedRooms.push_back(roomId);
            return std::make_unique<FakeDanmakuClient>(roomId);
        });
    QVector<DanmakuRoomEligibility> rooms;
    for (int i = 0; i < 17; ++i) {
        auto room = eligible(QString::number(i + 1));
        room.priority = i;
        rooms.push_back(room);
    }

    manager.synchronize(rooms);
    QCOMPARE(manager.activeSessionCount(), 12);
    QCOMPARE(startedRooms.size(), 12);
    QCOMPARE(manager.statusForRoom(QStringLiteral("17")).state,
             DanmakuConnectionState::WaitingForSession);
}

void DanmakuSessionManagerTest::stopsRoomsThatBecomeIneligible()
{
    int stopCount = 0;
    DanmakuSessionManager manager(
        [&](const QString &roomId, QObject *) -> std::unique_ptr<DanmakuClient> {
            auto client = std::make_unique<FakeDanmakuClient>(roomId);
            client->externalStops = &stopCount;
            return client;
        });
    auto room = eligible(QStringLiteral("63136"));
    manager.synchronize({room});
    room.live = false;
    manager.synchronize({room});
    QCOMPARE(manager.activeSessionCount(), 0);
    QCOMPARE(stopCount, 1);
    QCOMPARE(manager.pendingCount(QStringLiteral("63136")), 0);
}

void DanmakuSessionManagerTest::reportsIneligibleRoomsAsIdle()
{
    DanmakuSessionManager manager(
        [](const QString &roomId, QObject *parent) -> std::unique_ptr<DanmakuClient> {
            return std::make_unique<FakeDanmakuClient>(roomId, parent);
        });
    auto room = eligible(QStringLiteral("63136"));
    room.roomEnabled = false;
    manager.synchronize({room});

    QCOMPARE(manager.statusForRoom(QStringLiteral("63136")).state,
             DanmakuConnectionState::Idle);
}

void DanmakuSessionManagerTest::deduplicatesAndBoundsEachRoomQueue()
{
    FakeDanmakuClient *fake = nullptr;
    DanmakuSessionManager manager(
        [&](const QString &roomId, QObject *) -> std::unique_ptr<DanmakuClient> {
            auto client = std::make_unique<FakeDanmakuClient>(roomId);
            fake = client.get();
            return client;
        });
    auto room = eligible(QStringLiteral("63136"));
    room.governance.peakProtectionEnabled = false;
    manager.synchronize({room});
    for (int i = 1; i <= 101; ++i) fake->emitChat(QString::number(i), QString::number(i));
    QCOMPARE(manager.pendingCount(QStringLiteral("63136")), 100);
    QCOMPARE(manager.statsForRoom(QStringLiteral("63136")).queueOverflow, 1);
    QCOMPARE(manager.takeNextMessage(QStringLiteral("63136"))->id, QStringLiteral("2"));
    QCOMPARE(manager.takeNextMessage(QStringLiteral("63136"))->id, QStringLiteral("3"));
}

void DanmakuSessionManagerTest::appliesGovernanceBeforeQueueing()
{
    FakeDanmakuClient *fake = nullptr;
    DanmakuSessionManager manager(
        [&](const QString &roomId, QObject *) -> std::unique_ptr<DanmakuClient> {
            auto client = std::make_unique<FakeDanmakuClient>(roomId);
            fake = client.get();
            return client;
        });
    auto room = eligible(QStringLiteral("63136"));
    room.governance.keywordBlacklist = {QStringLiteral("blocked")};
    manager.synchronize({room});
    fake->emitChat(QStringLiteral("1"), QStringLiteral("blocked content"));
    QCOMPARE(manager.pendingCount(QStringLiteral("63136")), 0);
    QCOMPARE(manager.statsForRoom(QStringLiteral("63136")).filtered, 1);
}

void DanmakuSessionManagerTest::coalescesStatisticsNotificationsPerRoom()
{
    FakeDanmakuClient *fake = nullptr;
    DanmakuSessionManager manager(
        [&](const QString &roomId, QObject *) -> std::unique_ptr<DanmakuClient> {
            auto client = std::make_unique<FakeDanmakuClient>(roomId);
            fake = client.get();
            return client;
        });
    QSignalSpy changed(&manager, &DanmakuSessionManager::statisticsChanged);
    manager.synchronize({eligible(QStringLiteral("63136"))});

    fake->emitChat(QStringLiteral("1"), QStringLiteral("one"));
    fake->emitChat(QStringLiteral("2"), QStringLiteral("two"));
    QCOMPARE(changed.count(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, 1000);
    QCOMPARE(changed.at(0).at(0).toString(), QStringLiteral("63136"));

    manager.clearStats(QStringLiteral("63136"));
    QCOMPARE(changed.count(), 2);
    QCOMPARE(manager.statsForRoom(QStringLiteral("63136")).filtered, 0);
}

void DanmakuSessionManagerTest::releasesSchedulerWhenSessionIsRemoved()
{
    QPointer<DanmakuClient> client;
    QPointer<TrackingScheduler> scheduler;
    DanmakuSessionManager manager(
        [&client, &scheduler](const QString &roomId, QObject *) -> std::unique_ptr<DanmakuClient> {
            auto *createdScheduler = new TrackingScheduler;
            scheduler = createdScheduler;
            auto socket = std::make_unique<NullDanmakuSocket>();
            auto createdClient = std::make_unique<DouyuDanmakuClient>(
                roomId, std::move(socket), createdScheduler);
            client = createdClient.get();
            return createdClient;
        });

    manager.synchronize({eligible(QStringLiteral("63136"))});
    QVERIFY(scheduler != nullptr);
    QVERIFY(client != nullptr);
    QCOMPARE(scheduler->parent(), client.data());

    manager.clearRoom(QStringLiteral("63136"));
    QTRY_VERIFY(client.isNull());
    QTRY_VERIFY(scheduler.isNull());
    QCOMPARE(manager.activeSessionCount(), 0);
}

void DanmakuSessionManagerTest::clearsQueuesAndStopsAllOnShutdown()
{
    QVector<int> stopCounts(2, 0);
    int created = 0;
    DanmakuSessionManager manager(
        [&](const QString &roomId, QObject *) -> std::unique_ptr<DanmakuClient> {
            auto client = std::make_unique<FakeDanmakuClient>(roomId);
            client->externalStops = &stopCounts[created++];
            return client;
        });
    manager.synchronize({eligible(QStringLiteral("1")), eligible(QStringLiteral("2"))});
    manager.stopAll();
    QCOMPARE(manager.activeSessionCount(), 0);
    QCOMPARE(manager.pendingCount(QStringLiteral("1")), 0);
    QCOMPARE(stopCounts.at(0), 1);
    QCOMPARE(stopCounts.at(1), 1);
}


void DanmakuSessionManagerTest::handlesReentrantClearDuringStop()
{
    DanmakuSessionManager manager(
        [](const QString &roomId, QObject *parent) -> std::unique_ptr<DanmakuClient> {
            return std::make_unique<FakeDanmakuClient>(roomId, parent);
        });
    manager.synchronize({eligible(QStringLiteral("63136"))});

    bool reentered = false;
    QObject::connect(&manager, &DanmakuSessionManager::roomStateChanged,
                     &manager, [&manager, &reentered](const QString &roomId) {
                         if (roomId != QStringLiteral("63136") || reentered) return;
                         reentered = true;
                         manager.clearRoom(roomId);
                     });

    manager.clearRoom(QStringLiteral("63136"));

    QVERIFY(reentered);
    QCOMPARE(manager.activeSessionCount(), 0);
}

void DanmakuSessionManagerTest::promotesWaitingRoomWhenPriorityChanges()
{
    QHash<QString, int> starts;
    QHash<QString, std::shared_ptr<int>> stopCounters;
    DanmakuSessionManager manager(
        [&](const QString &roomId, QObject *) -> std::unique_ptr<DanmakuClient> {
            ++starts[roomId];
            auto client = std::make_unique<FakeDanmakuClient>(roomId);
            stopCounters.insert(roomId, std::make_shared<int>(0));
            client->externalStops = stopCounters.value(roomId).get();
            return client;
        });

    QVector<DanmakuRoomEligibility> rooms;
    for (int i = 0; i < 13; ++i) {
        auto room = eligible(QString::number(i + 1));
        room.priority = i;
        rooms.push_back(room);
    }
    manager.synchronize(rooms);
    QCOMPARE(manager.activeSessionCount(), 12);
    QCOMPARE(manager.statusForRoom(QStringLiteral("13")).state,
             DanmakuConnectionState::WaitingForSession);

    rooms[12].priority = -1;
    manager.synchronize(rooms);

    QCOMPARE(manager.activeSessionCount(), 12);
    QCOMPARE(starts.value(QStringLiteral("13")), 1);
    QCOMPARE(*stopCounters.value(QStringLiteral("12")), 1);
    QCOMPARE(manager.statusForRoom(QStringLiteral("13")).state,
             DanmakuConnectionState::Connecting);
}

void DanmakuSessionManagerTest::preservesTargetSessionsAcrossRepeatedSync()
{
    QHash<QString, int> starts;
    QHash<QString, std::shared_ptr<int>> stopCounters;
    DanmakuSessionManager manager(
        [&](const QString &roomId, QObject *) -> std::unique_ptr<DanmakuClient> {
            ++starts[roomId];
            auto client = std::make_unique<FakeDanmakuClient>(roomId);
            stopCounters.insert(roomId, std::make_shared<int>(0));
            client->externalStops = stopCounters.value(roomId).get();
            return client;
        });

    QVector<DanmakuRoomEligibility> rooms;
    for (int i = 0; i < 13; ++i) {
        auto room = eligible(QString::number(i + 1));
        room.priority = i;
        rooms.push_back(room);
    }
    manager.synchronize(rooms);
    const QHash<QString, int> initialStarts = starts;
    manager.synchronize(rooms);

    QCOMPARE(manager.activeSessionCount(), 12);
    QCOMPARE(starts, initialStarts);
    for (int i = 1; i <= 12; ++i) QCOMPARE(*stopCounters.value(QString::number(i)), 0);
}

void DanmakuSessionManagerTest::replacesIneligibleSessionWithWaitingRoom()
{
    QHash<QString, int> starts;
    QHash<QString, std::shared_ptr<int>> stopCounters;
    DanmakuSessionManager manager(
        [&](const QString &roomId, QObject *) -> std::unique_ptr<DanmakuClient> {
            ++starts[roomId];
            auto client = std::make_unique<FakeDanmakuClient>(roomId);
            stopCounters.insert(roomId, std::make_shared<int>(0));
            client->externalStops = stopCounters.value(roomId).get();
            return client;
        });

    QVector<DanmakuRoomEligibility> rooms;
    for (int i = 0; i < 13; ++i) {
        auto room = eligible(QString::number(i + 1));
        room.priority = i;
        rooms.push_back(room);
    }
    manager.synchronize(rooms);
    rooms[0].live = false;
    manager.synchronize(rooms);

    QCOMPARE(manager.activeSessionCount(), 12);
    QCOMPARE(*stopCounters.value(QStringLiteral("1")), 1);
    QCOMPARE(starts.value(QStringLiteral("13")), 1);
    QCOMPARE(manager.statusForRoom(QStringLiteral("1")).state,
             DanmakuConnectionState::Idle);
    QCOMPARE(manager.statusForRoom(QStringLiteral("13")).state,
             DanmakuConnectionState::Connecting);
}

void DanmakuSessionManagerTest::deduplicatesLongIdsWithoutPrefixCollision()
{
    FakeDanmakuClient *fake = nullptr;
    DanmakuSessionManager manager(
        [&](const QString &roomId, QObject *) -> std::unique_ptr<DanmakuClient> {
            auto client = std::make_unique<FakeDanmakuClient>(roomId);
            fake = client.get();
            return client;
        });
    auto room = eligible(QStringLiteral("63136"));
    room.governance.peakProtectionEnabled = false;
    manager.synchronize({room});

    const QString prefix(300, QChar('x'));
    const QString first = prefix + QStringLiteral("A");
    const QString second = prefix + QStringLiteral("B");
    fake->emitChat(first, QStringLiteral("first"));
    fake->emitChat(second, QStringLiteral("second"));
    fake->emitChat(first, QStringLiteral("first-again"));

    QCOMPARE(manager.pendingCount(QStringLiteral("63136")), 2);
}

void DanmakuSessionManagerTest::evictsDeduplicationKeysInFifoOrder()
{
    FakeDanmakuClient *fake = nullptr;
    DanmakuSessionManager manager(
        [&](const QString &roomId, QObject *) -> std::unique_ptr<DanmakuClient> {
            auto client = std::make_unique<FakeDanmakuClient>(roomId);
            fake = client.get();
            return client;
        });
    auto room = eligible(QStringLiteral("63136"));
    room.governance.peakProtectionEnabled = false;
    manager.synchronize({room});

    for (int i = 0; i < 201; ++i) fake->emitChat(QString::number(i), QString::number(i));
    while (manager.takeNextMessage(QStringLiteral("63136")).has_value()) {}
    fake->emitChat(QStringLiteral("0"), QStringLiteral("reaccepted"));
    QCOMPARE(manager.pendingCount(QStringLiteral("63136")), 1);
}
QTEST_GUILESS_MAIN(DanmakuSessionManagerTest)

#include "danmaku_session_manager_test.moc"
