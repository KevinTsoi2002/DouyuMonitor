#include <QtTest/QtTest>

#include "danmaku/danmaku_session_manager.h"

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
    void startsAtMostNineEligibleRooms();
    void stopsRoomsThatBecomeIneligible();
    void deduplicatesAndBoundsEachRoomQueue();
    void appliesGovernanceBeforeQueueing();
    void clearsQueuesAndStopsAllOnShutdown();
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

void DanmakuSessionManagerTest::startsAtMostNineEligibleRooms()
{
    QVector<FakeDanmakuClient *> fakes;
    DanmakuSessionManager manager(
        [&](const QString &roomId, QObject *) -> std::unique_ptr<DanmakuClient> {
            auto fake = std::make_unique<FakeDanmakuClient>(roomId);
            fakes.push_back(fake.get());
            return fake;
        });
    QVector<DanmakuRoomEligibility> rooms;
    for (int i = 0; i < 10; ++i) rooms.push_back(eligible(QString::number(i + 1)));

    manager.synchronize(rooms);
    QCOMPARE(manager.activeSessionCount(), 9);
    QCOMPARE(fakes.size(), 9);
    QCOMPARE(manager.statusForRoom(QStringLiteral("10")).state,
             DanmakuConnectionState::Idle);
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

QTEST_GUILESS_MAIN(DanmakuSessionManagerTest)

#include "danmaku_session_manager_test.moc"
