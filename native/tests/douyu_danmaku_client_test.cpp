#include <QtEndian>
#include <QtTest/QtTest>

#include <memory>

#include "danmaku/danmaku_socket.h"
#include "danmaku/danmaku_timer_scheduler.h"
#include "danmaku/douyu_danmaku_client.h"
#include "danmaku/douyu_danmaku_protocol.h"

class FakeDanmakuSocket final : public DanmakuSocket {
    Q_OBJECT

public:
    using DanmakuSocket::DanmakuSocket;

    void connectTo(const QUrl &url, quint64 attemptId) override
    {
        connectedUrls.push_back(url);
        attemptIds.push_back(attemptId);
    }
    void sendBinary(const QByteArray &frame) override { writes.push_back(frame); }
    void close() override { ++closeCount; }

    quint64 currentAttemptId() const { return attemptIds.isEmpty() ? 0 : attemptIds.constLast(); }
    void emitConnectedSignal(quint64 attemptId = 0)
    {
        emit connected(attemptId == 0 ? currentAttemptId() : attemptId);
    }
    void emitFrame(const QByteArray &frame, quint64 attemptId = 0)
    {
        emit binaryFrameReceived(frame, attemptId == 0 ? currentAttemptId() : attemptId);
    }
    void emitNetworkFailure(quint64 attemptId = 0)
    {
        emit networkFailure(attemptId == 0 ? currentAttemptId() : attemptId);
    }
    void emitClosed(int code, const QString &reason, quint64 attemptId = 0)
    {
        emit closed(code, reason, attemptId == 0 ? currentAttemptId() : attemptId);
    }
    void emitAuthenticationRequested(quint64 attemptId = 0)
    {
        emit authenticationRequested(attemptId == 0 ? currentAttemptId() : attemptId);
    }

    QVector<QUrl> connectedUrls;
    QVector<quint64> attemptIds;
    QVector<QByteArray> writes;
    int closeCount = 0;
};

class FakeDanmakuTimerScheduler final : public DanmakuTimerScheduler {
public:
    TimerId once(int delayMs, Callback callback) override
    {
        return add(delayMs, 0, std::move(callback));
    }

    TimerId repeating(int intervalMs, Callback callback) override
    {
        return add(intervalMs, intervalMs, std::move(callback));
    }

    void cancel(TimerId id) override { timers.remove(id); }

    void advanceBy(int milliseconds)
    {
        nowMs += milliseconds;
        bool fired = true;
        while (fired) {
            fired = false;
            const auto ids = timers.keys();
            for (const TimerId id : ids) {
                auto it = timers.find(id);
                if (it == timers.end() || it->dueMs > nowMs) continue;
                const auto callback = it->callback;
                if (it->intervalMs == 0) {
                    timers.erase(it);
                } else {
                    it->dueMs += it->intervalMs;
                }
                callback();
                fired = true;
            }
        }
    }

    bool fireOneDueTimer()
    {
        const auto ids = timers.keys();
        for (const TimerId id : ids) {
            auto it = timers.find(id);
            if (it == timers.end() || it->dueMs > nowMs) continue;
            const auto callback = it->callback;
            if (it->intervalMs == 0) {
                timers.erase(it);
            } else {
                it->dueMs += it->intervalMs;
            }
            callback();
            return true;
        }
        return false;
    }

private:
    struct Timer {
        qint64 dueMs = 0;
        int intervalMs = 0;
        Callback callback;
    };

    TimerId add(int delayMs, int intervalMs, Callback callback)
    {
        const TimerId id = nextId++;
        timers.insert(id, Timer{nowMs + delayMs, intervalMs, std::move(callback)});
        return id;
    }

    TimerId nextId = 1;
    qint64 nowMs = 0;
    QHash<TimerId, Timer> timers;
};

class DouyuDanmakuClientTest final : public QObject {
    Q_OBJECT

private slots:
    void sendsLoginJoinAndHeartbeatAfterOpening();
    void becomesConnectedAfterLoginResponse();
    void rotatesEndpointsAndRetriesWithConfiguredDelays();
    void blocksOnExplicitAuthenticationAndWaitsForManualRetry();
    void stopsTimersAndIgnoresLateSocketEvents();
    void connectionDeadlineRetriesAndRejectsLateConnect();
    void reconnectsAfterConnectedServerClosure();
    void shortConnectionsPreserveBackoff();
    void stableConnectionResetsBackoff();
    void lateHandshakeFromRetiredAttemptIsIgnored();
    void handshakeBoundaryUsesDeliveryOrder();
    void dispatchesLargeMessagesInBoundedBatches();
    void stopsPendingFrameDispatch();
};

namespace {

QString payloadFromClientFrame(const QByteArray &frame)
{
    const quint32 bodyLength = qFromLittleEndian<quint32>(frame.constData());
    return QString::fromUtf8(frame.mid(12, static_cast<qsizetype>(bodyLength) - 9));
}

QMap<QString, QString> frameFields(const QByteArray &frame)
{
    return DouyuDanmakuProtocol::parseStt(payloadFromClientFrame(frame));
}

QByteArray serverFrame(const QString &payload)
{
    return DouyuDanmakuProtocol::encodeFrame(payload, 690);
}

} // namespace

void DouyuDanmakuClientTest::sendsLoginJoinAndHeartbeatAfterOpening()
{
    auto socket = std::make_unique<FakeDanmakuSocket>();
    auto *socketPtr = socket.get();
    FakeDanmakuTimerScheduler scheduler;
    DouyuDanmakuClient client(QStringLiteral("63136"), std::move(socket), &scheduler);

    client.start();
    QCOMPARE(socketPtr->connectedUrls.size(), 1);
    QCOMPARE(socketPtr->connectedUrls.constFirst().host(),
             QStringLiteral("danmuproxy.douyu.com"));
    socketPtr->emitConnectedSignal();
    QCOMPARE(socketPtr->writes.size(), 2);
    QCOMPARE(frameFields(socketPtr->writes.at(0)).value(QStringLiteral("type")),
             QStringLiteral("loginreq"));
    QCOMPARE(frameFields(socketPtr->writes.at(0)).value(QStringLiteral("roomid")),
             QStringLiteral("63136"));
    QCOMPARE(frameFields(socketPtr->writes.at(1)).value(QStringLiteral("type")),
             QStringLiteral("joingroup"));
    QCOMPARE(frameFields(socketPtr->writes.at(1)).value(QStringLiteral("rid")),
             QStringLiteral("63136"));
    QCOMPARE(frameFields(socketPtr->writes.at(1)).value(QStringLiteral("gid")),
             QStringLiteral("-9999"));

    socketPtr->emitFrame(serverFrame(QStringLiteral("type@=loginres/")));
    scheduler.advanceBy(45'000);
    QCOMPARE(frameFields(socketPtr->writes.constLast()).value(QStringLiteral("type")),
             QStringLiteral("mrkl"));
}

void DouyuDanmakuClientTest::becomesConnectedAfterLoginResponse()
{
    auto socket = std::make_unique<FakeDanmakuSocket>();
    auto *socketPtr = socket.get();
    FakeDanmakuTimerScheduler scheduler;
    QVector<DanmakuConnectionStatus> statuses;
    DouyuDanmakuClient client(QStringLiteral("63136"), std::move(socket), &scheduler);
    connect(&client, &DanmakuClient::statusChanged,
            this, [&](const DanmakuConnectionStatus &status) { statuses.push_back(status); });

    client.start();
    socketPtr->emitConnectedSignal();
    socketPtr->emitFrame(serverFrame(QStringLiteral("type@=loginres/")));
    QVERIFY(!statuses.isEmpty());
    QCOMPARE(statuses.constLast().state, DanmakuConnectionState::Connected);
}

void DouyuDanmakuClientTest::rotatesEndpointsAndRetriesWithConfiguredDelays()
{
    auto socket = std::make_unique<FakeDanmakuSocket>();
    auto *socketPtr = socket.get();
    FakeDanmakuTimerScheduler scheduler;
    DouyuDanmakuClient client(QStringLiteral("63136"), std::move(socket), &scheduler);

    client.start();
    socketPtr->emitNetworkFailure();
    scheduler.advanceBy(999);
    QCOMPARE(socketPtr->connectedUrls.size(), 1);
    scheduler.advanceBy(1);
    QCOMPARE(socketPtr->connectedUrls.size(), 2);
    QVERIFY(socketPtr->connectedUrls.at(0) != socketPtr->connectedUrls.at(1));
}

void DouyuDanmakuClientTest::blocksOnExplicitAuthenticationAndWaitsForManualRetry()
{
    auto socket = std::make_unique<FakeDanmakuSocket>();
    auto *socketPtr = socket.get();
    FakeDanmakuTimerScheduler scheduler;
    QVector<DanmakuConnectionStatus> statuses;
    DouyuDanmakuClient client(QStringLiteral("63136"), std::move(socket), &scheduler);
    connect(&client, &DanmakuClient::statusChanged,
            this, [&](const DanmakuConnectionStatus &status) { statuses.push_back(status); });

    client.start();
    socketPtr->emitAuthenticationRequested();
    QCOMPARE(statuses.constLast().state, DanmakuConnectionState::PlatformBlocked);
    QCOMPARE(statuses.constLast().errorCode, DanmakuErrorCode::AuthRequired);
    scheduler.advanceBy(60'000);
    QCOMPARE(socketPtr->connectedUrls.size(), 1);
    client.retry();
    QCOMPARE(socketPtr->connectedUrls.size(), 2);
}

void DouyuDanmakuClientTest::stopsTimersAndIgnoresLateSocketEvents()
{
    auto socket = std::make_unique<FakeDanmakuSocket>();
    auto *socketPtr = socket.get();
    FakeDanmakuTimerScheduler scheduler;
    QVector<DanmakuConnectionStatus> statuses;
    DouyuDanmakuClient client(QStringLiteral("63136"), std::move(socket), &scheduler);
    connect(&client, &DanmakuClient::statusChanged,
            this, [&](const DanmakuConnectionStatus &status) { statuses.push_back(status); });

    client.start();
    client.stop();
    const int statusCount = statuses.size();
    socketPtr->emitConnectedSignal();
    socketPtr->emitNetworkFailure();
    scheduler.advanceBy(60'000);
    QCOMPARE(socketPtr->writes.size(), 0);
    QCOMPARE(statuses.size(), statusCount);
}

void DouyuDanmakuClientTest::connectionDeadlineRetriesAndRejectsLateConnect()
{
    auto socket = std::make_unique<FakeDanmakuSocket>();
    auto *socketPtr = socket.get();
    FakeDanmakuTimerScheduler scheduler;
    QVector<DanmakuConnectionStatus> statuses;
    DouyuDanmakuClient client(QStringLiteral("63136"), std::move(socket), &scheduler);
    connect(&client, &DanmakuClient::statusChanged,
            this, [&](const DanmakuConnectionStatus &status) { statuses.push_back(status); });

    client.start();
    const quint64 firstAttempt = socketPtr->currentAttemptId();
    scheduler.advanceBy(9'999);
    QCOMPARE(statuses.constLast().state, DanmakuConnectionState::Connecting);

    scheduler.advanceBy(1);
    QCOMPARE(statuses.constLast().state, DanmakuConnectionState::Reconnecting);
    QCOMPARE(statuses.constLast().errorCode, DanmakuErrorCode::NetworkUnavailable);
    QCOMPARE(socketPtr->connectedUrls.size(), 1);

    scheduler.advanceBy(1'000);
    QCOMPARE(socketPtr->connectedUrls.size(), 2);
    const quint64 secondAttempt = socketPtr->currentAttemptId();
    QVERIFY(secondAttempt != firstAttempt);

    socketPtr->emitConnectedSignal(firstAttempt);
    QCOMPARE(socketPtr->writes.size(), 0);
    socketPtr->emitConnectedSignal(secondAttempt);
    QCOMPARE(socketPtr->writes.size(), 2);

    client.stop();
}

void DouyuDanmakuClientTest::reconnectsAfterConnectedServerClosure()
{
    auto socket = std::make_unique<FakeDanmakuSocket>();
    auto *raw = socket.get();
    FakeDanmakuTimerScheduler scheduler;
    DouyuDanmakuClient client(QStringLiteral("63136"), std::move(socket), &scheduler);
    QVector<DanmakuConnectionStatus> statuses;
    connect(&client, &DanmakuClient::statusChanged, this,
            [&](const DanmakuConnectionStatus &status) { statuses.append(status); });
    client.start();
    raw->emitConnectedSignal();
    raw->emitFrame(serverFrame(QStringLiteral("type@=loginres/")));
    raw->emitClosed(1000, {});
    QCOMPARE(statuses.constLast().state, DanmakuConnectionState::Reconnecting);
    raw->emitNetworkFailure();
    raw->emitClosed(1000, {});
    scheduler.advanceBy(1000);
    QCOMPARE(raw->connectedUrls.size(), 2);
    client.stop();
    raw->emitClosed(1000, {});
    scheduler.advanceBy(60000);
    QCOMPARE(raw->connectedUrls.size(), 2);
}

void DouyuDanmakuClientTest::shortConnectionsPreserveBackoff()
{
    auto socket = std::make_unique<FakeDanmakuSocket>();
    auto *raw = socket.get();
    FakeDanmakuTimerScheduler scheduler;
    DouyuDanmakuClient client(QStringLiteral("63136"), std::move(socket), &scheduler);

    const QVector<int> expectedDelays{1000, 2000, 4000, 8000, 15000, 15000};
    client.start();
    for (qsizetype index = 0; index < expectedDelays.size(); ++index) {
        const int delay = expectedDelays.at(index);
        const quint64 attemptId = raw->currentAttemptId();
        raw->emitConnectedSignal(attemptId);
        raw->emitFrame(serverFrame(QStringLiteral("type@=loginres/")), attemptId);
        scheduler.advanceBy(100);
        raw->emitClosed(1000, {}, attemptId);
        scheduler.advanceBy(delay - 1);
        QCOMPARE(raw->connectedUrls.size(), static_cast<int>(index) + 1);
        scheduler.advanceBy(1);
    }

    const quint64 exhaustedAttemptId = raw->currentAttemptId();
    raw->emitConnectedSignal(exhaustedAttemptId);
    raw->emitFrame(serverFrame(QStringLiteral("type@=loginres/")), exhaustedAttemptId);
    raw->emitClosed(1000, {}, exhaustedAttemptId);
    scheduler.advanceBy(60'000);
    QCOMPARE(raw->connectedUrls.size(), expectedDelays.size() + 1);
}

void DouyuDanmakuClientTest::stableConnectionResetsBackoff()
{
    auto socket = std::make_unique<FakeDanmakuSocket>();
    auto *raw = socket.get();
    FakeDanmakuTimerScheduler scheduler;
    DouyuDanmakuClient client(QStringLiteral("63136"), std::move(socket), &scheduler);

    client.start();
    const quint64 attemptId = raw->currentAttemptId();
    raw->emitConnectedSignal(attemptId);
    raw->emitFrame(serverFrame(QStringLiteral("type@=loginres/")), attemptId);
    scheduler.advanceBy(60'000);
    raw->emitClosed(1000, {}, attemptId);
    scheduler.advanceBy(999);
    QCOMPARE(raw->connectedUrls.size(), 1);
    scheduler.advanceBy(1);
    QCOMPARE(raw->connectedUrls.size(), 2);
}

void DouyuDanmakuClientTest::lateHandshakeFromRetiredAttemptIsIgnored()
{
    const QVector<QString> handshakePayloads{
        QStringLiteral("type@=loginres/"),
        QStringLiteral("type@=setmsggroup/"),
        QStringLiteral("type@=chatmsg/rid@=63136/nn@=late/txt@=hello/"),
    };

    for (const QString &payload : handshakePayloads) {
        auto socket = std::make_unique<FakeDanmakuSocket>();
        auto *raw = socket.get();
        FakeDanmakuTimerScheduler scheduler;
        QVector<DanmakuConnectionStatus> statuses;
        int chatCount = 0;
        DouyuDanmakuClient client(QStringLiteral("63136"), std::move(socket), &scheduler);
        connect(&client, &DanmakuClient::statusChanged, this,
                [&](const DanmakuConnectionStatus &status) { statuses.push_back(status); });
        connect(&client, &DanmakuClient::chatReceived, this,
                [&](const QString &, const QMap<QString, QString> &) { ++chatCount; });

        client.start();
        const quint64 retiredAttemptId = raw->currentAttemptId();
        raw->emitConnectedSignal(retiredAttemptId);
        scheduler.advanceBy(10'000);
        raw->emitFrame(serverFrame(payload), retiredAttemptId);
        QCOMPARE(statuses.constLast().state, DanmakuConnectionState::Reconnecting);
        scheduler.advanceBy(999);
        QCOMPARE(raw->connectedUrls.size(), 1);
        scheduler.advanceBy(1);
        QCOMPARE(raw->connectedUrls.size(), 2);

        const quint64 currentAttemptId = raw->currentAttemptId();
        raw->emitConnectedSignal(currentAttemptId);
        raw->emitFrame(serverFrame(payload), currentAttemptId);
        QCOMPARE(statuses.constLast().state, DanmakuConnectionState::Connected);
        const int statusCount = statuses.size();
        const int chatCountAfterCurrentAttempt = chatCount;
        raw->emitConnectedSignal(retiredAttemptId);
        raw->emitFrame(serverFrame(QStringLiteral("type@=loginres/")), retiredAttemptId);
        raw->emitFrame(serverFrame(QStringLiteral("type@=chatmsg/rid@=63136/nn@=late/txt@=stale/")),
                       retiredAttemptId);
        raw->emitClosed(1000, {}, retiredAttemptId);
        raw->emitNetworkFailure(retiredAttemptId);
        QCOMPARE(statuses.size(), statusCount);
        QCOMPARE(chatCount, chatCountAfterCurrentAttempt);
        QCOMPARE(raw->connectedUrls.size(), 2);
    }
}

void DouyuDanmakuClientTest::handshakeBoundaryUsesDeliveryOrder()
{
    {
        auto socket = std::make_unique<FakeDanmakuSocket>();
        auto *raw = socket.get();
        FakeDanmakuTimerScheduler scheduler;
        QVector<DanmakuConnectionStatus> statuses;
        DouyuDanmakuClient client(QStringLiteral("63136"), std::move(socket), &scheduler);
        connect(&client, &DanmakuClient::statusChanged, this,
                [&](const DanmakuConnectionStatus &status) { statuses.push_back(status); });

        client.start();
        const quint64 attemptId = raw->currentAttemptId();
        raw->emitConnectedSignal(attemptId);
        scheduler.advanceBy(9'999);
        raw->emitFrame(serverFrame(QStringLiteral("type@=loginres/")), attemptId);
        scheduler.advanceBy(1);
        QCOMPARE(statuses.constLast().state, DanmakuConnectionState::Connected);
        QCOMPARE(raw->connectedUrls.size(), 1);
    }

    {
        auto socket = std::make_unique<FakeDanmakuSocket>();
        auto *raw = socket.get();
        FakeDanmakuTimerScheduler scheduler;
        QVector<DanmakuConnectionStatus> statuses;
        DouyuDanmakuClient client(QStringLiteral("63136"), std::move(socket), &scheduler);
        connect(&client, &DanmakuClient::statusChanged, this,
                [&](const DanmakuConnectionStatus &status) { statuses.push_back(status); });

        client.start();
        const quint64 attemptId = raw->currentAttemptId();
        raw->emitConnectedSignal(attemptId);
        scheduler.advanceBy(10'000);
        raw->emitFrame(serverFrame(QStringLiteral("type@=loginres/")), attemptId);
        QCOMPARE(statuses.constLast().state, DanmakuConnectionState::Reconnecting);
        scheduler.advanceBy(1'000);
        QCOMPARE(raw->connectedUrls.size(), 2);
    }
}

void DouyuDanmakuClientTest::dispatchesLargeMessagesInBoundedBatches()
{
    auto socket = std::make_unique<FakeDanmakuSocket>();
    auto *raw = socket.get();
    FakeDanmakuTimerScheduler scheduler;
    int chats = 0;
    DouyuDanmakuClient client(QStringLiteral("63136"), std::move(socket), &scheduler);
    connect(&client, &DanmakuClient::chatReceived, this,
            [&](const QString &, const QMap<QString, QString> &) { ++chats; });

    client.start();
    const quint64 attemptId = raw->currentAttemptId();
    raw->emitConnectedSignal(attemptId);
    QByteArray message;
    for (int i = 0; i < 70; ++i) {
        message += serverFrame(QStringLiteral("type@=chatmsg/rid@=63136/cid@=")
                               + QString::number(i) + QStringLiteral("/txt@=message/"));
    }
    raw->emitFrame(message, attemptId);
    QCOMPARE(chats, 32);
    QVERIFY(scheduler.fireOneDueTimer());
    QCOMPARE(chats, 64);
    QVERIFY(scheduler.fireOneDueTimer());
    QCOMPARE(chats, 70);
}

void DouyuDanmakuClientTest::stopsPendingFrameDispatch()
{
    auto socket = std::make_unique<FakeDanmakuSocket>();
    auto *raw = socket.get();
    FakeDanmakuTimerScheduler scheduler;
    int chats = 0;
    DouyuDanmakuClient client(QStringLiteral("63136"), std::move(socket), &scheduler);
    connect(&client, &DanmakuClient::chatReceived, this,
            [&](const QString &, const QMap<QString, QString> &) { ++chats; });

    client.start();
    const quint64 attemptId = raw->currentAttemptId();
    raw->emitConnectedSignal(attemptId);
    QByteArray message;
    for (int i = 0; i < 70; ++i) {
        message += serverFrame(QStringLiteral("type@=chatmsg/rid@=63136/cid@=")
                               + QString::number(i) + QStringLiteral("/txt@=message/"));
    }
    raw->emitFrame(message, attemptId);
    QCOMPARE(chats, 32);
    client.stop();
    QVERIFY(!scheduler.fireOneDueTimer());
    QCOMPARE(chats, 32);
}

QTEST_GUILESS_MAIN(DouyuDanmakuClientTest)

#include "douyu_danmaku_client_test.moc"
