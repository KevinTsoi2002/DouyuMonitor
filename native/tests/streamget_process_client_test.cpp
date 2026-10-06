#include <QtTest/QtTest>

#include "service/streamget_process_client.h"

#include <QSignalSpy>
#include <QTemporaryDir>

#ifndef FAKE_STREAMGET_SERVICE_PATH
#define FAKE_STREAMGET_SERVICE_PATH "fake_streamget_service"
#endif

namespace {

QString fakeServicePath()
{
    return QString::fromLocal8Bit(FAKE_STREAMGET_SERVICE_PATH);
}

ServiceResponse responseFrom(const QSignalSpy &spy, int index = 0)
{
    return qvariant_cast<ServiceResponse>(spy.at(index).at(0));
}

void waitForSignalCount(const QSignalSpy &spy, qsizetype count, int timeout = 3000)
{
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= count, timeout);
}

} // namespace

class StreamgetProcessClientTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void startsLazilyAndPings();
    void searchesWithTypedResults();
    void searchesNumericRoomWithScriptedStatus();
    void refreshesRoomStatus();
    void correlatesRequestsAndKeepsTwoInFlight();
    void prioritizesPlaybackOverQueuedBackgroundChecks();
    void doesNotStarveBackgroundChecksDuringPlaybackBurst();
    void timesOutAndCancelsLateResponses();
    void rejectsMalformedChildOutput();
    void failsPendingWorkAndRestartsAfterCrash();
    void shutsDownWithinBound();
    void killsAnUnresponsiveServiceAtShutdownBoundary();
    void startsANewClientAfterImmediateShutdown();
};

void StreamgetProcessClientTest::initTestCase()
{
    qRegisterMetaType<ServiceResponse>();
}

void StreamgetProcessClientTest::startsLazilyAndPings()
{
    StreamgetProcessClient client(fakeServicePath());
    QSignalSpy started(&client, &StreamgetProcessClient::childStarted);
    QSignalSpy stopped(&client, &StreamgetProcessClient::childStopped);
    QSignalSpy responses(&client, &StreamgetProcessClient::responseReceived);
    QSignalSpy failures(&client, &StreamgetProcessClient::requestFailed);

    QVERIFY(!client.isRunning());
    const quint64 requestId = client.ping();
    QVERIFY(requestId > 0);
    waitForSignalCount(started, 1);
    QTRY_VERIFY2_WITH_TIMEOUT(responses.count() >= 1,
                              qPrintable(QStringLiteral("responses=%1 failures=%2 code=%3 running=%4")
                                             .arg(responses.count())
                                             .arg(failures.count())
                                             .arg(failures.isEmpty() ? QString() : failures.at(0).at(1).toString())
                                             .arg(client.isRunning())),
                              3000);

    const ServiceResponse response = responseFrom(responses);
    QCOMPARE(response.requestId, requestId);
    QVERIFY(response.ok);
    QVERIFY(response.pong);
    QCOMPARE(started.count(), 1);
    QVERIFY(client.isRunning());

    client.shutdown(1000);
    waitForSignalCount(stopped, 1);
    QVERIFY(!client.isRunning());
}

void StreamgetProcessClientTest::searchesWithTypedResults()
{
    StreamgetProcessClient client(fakeServicePath());
    QSignalSpy responses(&client, &StreamgetProcessClient::responseReceived);

    const quint64 requestId = client.search(QStringLiteral("主播"), 1000);
    waitForSignalCount(responses, 1);
    const ServiceResponse response = responseFrom(responses);
    QCOMPARE(response.requestId, requestId);
    QVERIFY(response.ok);
    QVERIFY(response.search);
    QVERIFY(response.results.isEmpty());
    client.shutdown();
}

void StreamgetProcessClientTest::searchesNumericRoomWithScriptedStatus()
{
    StreamgetProcessClient client(
        fakeServicePath(),
        {QStringLiteral("--search-script"), QStringLiteral("online,offline,online")});
    QSignalSpy responses(&client, &StreamgetProcessClient::responseReceived);

    const QVector<bool> expectedOnline{true, false, true};
    for (const bool expected : expectedOnline) {
        const quint64 requestId = client.search(QStringLiteral("63136"), 1000);
        waitForSignalCount(responses, responses.count() + 1);
        const ServiceResponse response = responseFrom(responses, responses.count() - 1);
        QCOMPARE(response.requestId, requestId);
        QVERIFY(response.ok);
        QVERIFY(response.search);
        QCOMPARE(response.results.size(), 1);
        const RoomSearchResult &result = response.results.front();
        QCOMPARE(result.roomId, QStringLiteral("63136"));
        QCOMPARE(result.anchorName, QStringLiteral("Fake Anchor"));
        QCOMPARE(result.title, QStringLiteral("Fake Room"));
        QCOMPARE(result.category, QStringLiteral("Game"));
        QCOMPARE(result.viewerLabel, QStringLiteral("1,234"));
        QCOMPARE(result.online, expected);
        QVERIFY(result.avatarUrl.isValid());
    }

    client.shutdown();
}

void StreamgetProcessClientTest::refreshesRoomStatus()
{
    StreamgetProcessClient client(fakeServicePath(),
                                  {QStringLiteral("--search-script"),
                                   QStringLiteral("online,offline")});
    QSignalSpy responses(&client, &StreamgetProcessClient::responseReceived);

    const QStringList expectedStates{QStringLiteral("online"), QStringLiteral("offline")};
    for (int index = 0; index < expectedStates.size(); ++index) {
        const quint64 requestId = client.status(QStringLiteral("63136"), 1000);
        waitForSignalCount(responses, responses.count() + 1);
        const ServiceResponse response = responseFrom(responses, responses.count() - 1);
        QCOMPARE(response.requestId, requestId);
        QVERIFY(response.ok);
        QVERIFY(response.status);
        QCOMPARE(response.isLive, expectedStates.at(index) == QStringLiteral("online"));
    }

    client.shutdown();
}

void StreamgetProcessClientTest::correlatesRequestsAndKeepsTwoInFlight()
{
    StreamgetProcessClient client(fakeServicePath(), {QStringLiteral("--delay-ms"), QStringLiteral("80")});
    QSignalSpy responses(&client, &StreamgetProcessClient::responseReceived);

    const quint64 first = client.resolve(QStringLiteral("63136"), StreamQuality::Auto, 1000);
    const quint64 second = client.resolve(QStringLiteral("100"), StreamQuality::High, 1000);
    const quint64 third = client.resolve(QStringLiteral("200"), StreamQuality::Standard, 1000);
    QVERIFY(first != second);
    QVERIFY(second != third);
    QTRY_COMPARE_WITH_TIMEOUT(client.activeRequestCount(), 2, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(client.queuedRequestCount(), 1, 1000);

    waitForSignalCount(responses, 3, 3000);
    QSet<quint64> ids;
    for (const QList<QVariant> &arguments : responses) {
        ids.insert(qvariant_cast<ServiceResponse>(arguments.at(0)).requestId);
    }
    QCOMPARE(ids, QSet<quint64>({first, second, third}));
    QCOMPARE(client.activeRequestCount(), 0);
    QCOMPARE(client.queuedRequestCount(), 0);

    client.shutdown();
}

void StreamgetProcessClientTest::prioritizesPlaybackOverQueuedBackgroundChecks()
{
    StreamgetProcessClient client(fakeServicePath(), {"--delay-ms", "1500"});
    QSignalSpy failures(&client, &StreamgetProcessClient::requestFailed);
    QSignalSpy responses(&client, &StreamgetProcessClient::responseReceived);
    client.resolve("100", StreamQuality::Auto, 150);
    client.resolve("200", StreamQuality::Auto, 5000);
    QTRY_COMPARE(client.activeRequestCount(), 2);
    const quint64 background1 = client.status("300");
    const quint64 background2 = client.status("400");
    const quint64 background3 = client.status("500");
    const quint64 playback = client.resolve("600", StreamQuality::Auto, 5000);
    QCOMPARE(client.queuedRequestCount(), 4);
    QTRY_COMPARE_WITH_TIMEOUT(failures.count(), 1, 1000);
    QCOMPARE(client.activeRequestCount(), 2);
    QCOMPARE(client.queuedRequestCount(), 3);
    QVERIFY(client.cancel(background1));
    QVERIFY(client.cancel(background2));
    QVERIFY(client.cancel(background3));
    QCOMPARE(client.queuedRequestCount(), 0);
    QTRY_VERIFY_WITH_TIMEOUT(responses.count() >= 2, 4000);
    bool receivedPlayback = false;
    for (const auto &arguments : responses)
        receivedPlayback |= qvariant_cast<ServiceResponse>(arguments.at(0)).requestId == playback;
    QVERIFY(receivedPlayback);
    client.shutdown();
}

void StreamgetProcessClientTest::doesNotStarveBackgroundChecksDuringPlaybackBurst()
{
    StreamgetProcessClient client(fakeServicePath(), {"--delay-ms", "5000"});
    QSignalSpy responses(&client, &StreamgetProcessClient::responseReceived);
    for (int index = 0; index < 8; ++index)
        client.resolve(QString::number(100 + index), StreamQuality::Auto, 100);
    const auto background = client.status("300");
    QTRY_VERIFY_WITH_TIMEOUT(!responses.isEmpty(), 1000);
    QCOMPARE(responseFrom(responses).requestId, background);
    QVERIFY(client.activeRequestCount() <= 2);
    client.shutdown();
}

void StreamgetProcessClientTest::timesOutAndCancelsLateResponses()
{
    StreamgetProcessClient client(fakeServicePath(), {QStringLiteral("--delay-ms"), QStringLiteral("200"),
                                                       QStringLiteral("--ignore-cancel")});
    QSignalSpy responses(&client, &StreamgetProcessClient::responseReceived);
    QSignalSpy failures(&client, &StreamgetProcessClient::requestFailed);

    const quint64 timedOut = client.resolve(QStringLiteral("63136"), StreamQuality::Auto, 40);
    waitForSignalCount(failures, 1);
    QCOMPARE(failures.at(0).at(0).toULongLong(), timedOut);
    QCOMPARE(failures.at(0).at(1).toString(), QStringLiteral("TIMEOUT"));

    const quint64 cancelled = client.resolve(QStringLiteral("100"), StreamQuality::Auto, 1000);
    QVERIFY(client.cancel(cancelled));
    waitForSignalCount(failures, 2);
    QCOMPARE(failures.at(1).at(0).toULongLong(), cancelled);
    QCOMPARE(failures.at(1).at(1).toString(), QStringLiteral("CANCELLED"));

    QTest::qWait(500);
    for (const QList<QVariant> &arguments : responses) {
        const quint64 id = qvariant_cast<ServiceResponse>(arguments.at(0)).requestId;
        QVERIFY(id != timedOut);
        QVERIFY(id != cancelled);
    }

    client.shutdown();
}

void StreamgetProcessClientTest::rejectsMalformedChildOutput()
{
    StreamgetProcessClient client(fakeServicePath(), {QStringLiteral("--malformed")});
    QSignalSpy failures(&client, &StreamgetProcessClient::requestFailed);

    const quint64 requestId = client.ping(1500);
    waitForSignalCount(failures, 1);
    QCOMPARE(failures.at(0).at(0).toULongLong(), requestId);
    QCOMPARE(failures.at(0).at(1).toString(), QStringLiteral("INVALID_RESPONSE"));

    client.shutdown();
}

void StreamgetProcessClientTest::failsPendingWorkAndRestartsAfterCrash()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString marker = temporaryDirectory.filePath(QStringLiteral("crashed-once.marker"));
    StreamgetProcessClient client(
        fakeServicePath(),
        {QStringLiteral("--crash-after"), QStringLiteral("1"), QStringLiteral("--crash-once-file"), marker});
    QSignalSpy started(&client, &StreamgetProcessClient::childStarted);
    QSignalSpy responses(&client, &StreamgetProcessClient::responseReceived);
    QSignalSpy failures(&client, &StreamgetProcessClient::requestFailed);

    const quint64 first = client.resolve(QStringLiteral("63136"), StreamQuality::Auto, 1000);
    waitForSignalCount(failures, 1);
    QCOMPARE(failures.at(0).at(1).toString(), QStringLiteral("SERVICE_FAILED"));
    QCOMPARE(failures.at(0).at(0).toULongLong(), first);
    QVERIFY(!client.isRunning());

    const quint64 restarted = client.ping(1000);
    waitForSignalCount(started, 2);
    waitForSignalCount(responses, 1);
    QVERIFY(responseFrom(responses).ok);
    QCOMPARE(responseFrom(responses).requestId, restarted);
    client.shutdown();
}

void StreamgetProcessClientTest::shutsDownWithinBound()
{
    StreamgetProcessClient client(fakeServicePath(), {QStringLiteral("--delay-ms"), QStringLiteral("1000")});
    QSignalSpy stopped(&client, &StreamgetProcessClient::childStopped);

    client.resolve(QStringLiteral("63136"), StreamQuality::Auto, 5000);
    const QElapsedTimer timer;
    QElapsedTimer elapsed;
    elapsed.start();
    client.shutdown(100);
    QVERIFY(elapsed.elapsed() < 1000);
    waitForSignalCount(stopped, 1, 1000);
    QVERIFY(!client.isRunning());
}

void StreamgetProcessClientTest::killsAnUnresponsiveServiceAtShutdownBoundary()
{
    StreamgetProcessClient client(fakeServicePath(),
                                  {QStringLiteral("--ignore-shutdown")});
    QSignalSpy started(&client, &StreamgetProcessClient::childStarted);
    QSignalSpy responses(&client, &StreamgetProcessClient::responseReceived);
    QSignalSpy stopped(&client, &StreamgetProcessClient::childStopped);

    const quint64 requestId = client.ping(1000);
    QVERIFY(requestId > 0);
    waitForSignalCount(started, 1);
    waitForSignalCount(responses, 1);

    QElapsedTimer elapsed;
    elapsed.start();
    client.shutdown(250);
    QVERIFY2(elapsed.elapsed() < 1500,
             qPrintable(QStringLiteral("shutdown took %1 ms").arg(elapsed.elapsed())));
    waitForSignalCount(stopped, 1);
    QVERIFY(!client.isRunning());
}

void StreamgetProcessClientTest::startsANewClientAfterImmediateShutdown()
{
    {
        StreamgetProcessClient client(fakeServicePath());
        QVERIFY(client.resolve(QStringLiteral("63136"), StreamQuality::Auto, 5000) > 0);
        client.shutdown(100);
    }

    StreamgetProcessClient restarted(fakeServicePath());
    QSignalSpy responses(&restarted, &StreamgetProcessClient::responseReceived);
    const quint64 requestId = restarted.ping(1000);
    QVERIFY(requestId > 0);
    waitForSignalCount(responses, 1);
    QCOMPARE(responseFrom(responses).requestId, requestId);
    QVERIFY(responseFrom(responses).ok);
    restarted.shutdown();
}

QTEST_GUILESS_MAIN(StreamgetProcessClientTest)

#include "streamget_process_client_test.moc"
