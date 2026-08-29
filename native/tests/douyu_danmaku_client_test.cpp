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

    void connectTo(const QUrl &url) override { connectedUrls.push_back(url); }
    void sendBinary(const QByteArray &frame) override { writes.push_back(frame); }
    void close() override { ++closeCount; }

    void emitConnectedSignal() { emit connected(); }
    void emitFrame(const QByteArray &frame) { emit binaryFrameReceived(frame); }
    void emitNetworkFailure() { emit networkFailure(); }
    void emitClosed(int code, const QString &reason) { emit closed(code, reason); }
    void emitAuthenticationRequested() { emit authenticationRequested(); }

    QVector<QUrl> connectedUrls;
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

QTEST_GUILESS_MAIN(DouyuDanmakuClientTest)

#include "douyu_danmaku_client_test.moc"
