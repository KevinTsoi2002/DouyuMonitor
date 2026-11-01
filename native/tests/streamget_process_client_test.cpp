#include <QtTest/QtTest>

#include "service/streamget_process_client.h"

#include <QSignalSpy>
#include <QProcess>
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
    void rejectsWrongSuccessShapeWithoutFailingOtherRequest();
    void rejectsExplicitMismatchedOperation();
    void acceptsExplicitMatchingOperation();
    void rejectsRequestsWhenQueueIsFull();
    void queueTimeoutIncludesWaitingTime();
    void activeTimeoutUsesRemainingTotalBudget();
    void cancelsQueuedRequestWithoutAffectingNeighbors();
    void prioritizesPlaybackOverQueuedBackgroundChecks();
    void doesNotStarveBackgroundChecksDuringPlaybackBurst();
    void timesOutAndCancelsLateResponses();
    void rejectsMalformedChildOutput();
    void rejectsOversizedChildLine();
    void rejectsOversizedChildOutputWithoutNewline();
    void drainsBurstStderrWithoutMixingWithStdout();
    void drainsContinuousStderrWhileDeadlineExpires();
    void drainsStderrAcrossExitAndRestart();
    void drainsStderrOnProtocolViolation();
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

    // The deadline includes process startup; leave enough margin that this test
    // observes the deliberate child crash instead of racing the request timer.
    const quint64 first = client.resolve(QStringLiteral("63136"), StreamQuality::Auto, 5000);
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

void StreamgetProcessClientTest::rejectsWrongSuccessShapeWithoutFailingOtherRequest()
{
    StreamgetProcessClient client(fakeServicePath(), {QStringLiteral("--wrong-search-shape")});
    QSignalSpy responses(&client, &StreamgetProcessClient::responseReceived);
    QSignalSpy failures(&client, &StreamgetProcessClient::requestFailed);

    const quint64 searchId = client.search(QStringLiteral("test"), 1500);
    const quint64 statusId = client.status(QStringLiteral("63136"), 1500);
    QVERIFY(searchId > 0);
    QVERIFY(statusId > 0);
    QVERIFY(searchId != statusId);
    QTRY_VERIFY_WITH_TIMEOUT(failures.count() >= 1, 2500);
    QTRY_VERIFY2_WITH_TIMEOUT(responses.count() >= 1,
                              qPrintable(QStringLiteral("responses=%1 failures=%2 active=%3 running=%4")
                                             .arg(responses.count()).arg(failures.count())
                                             .arg(client.activeRequestCount()).arg(client.isRunning())),
                              2500);
    QCOMPARE(failures.at(0).at(0).toULongLong(), searchId);
    QCOMPARE(failures.at(0).at(1).toString(), QStringLiteral("INVALID_RESPONSE"));
    QCOMPARE(responseFrom(responses).requestId, statusId);
    QVERIFY(responseFrom(responses).status);
    QCOMPARE(responses.count(), 1);
    QCOMPARE(failures.count(), 1);
    QCOMPARE(client.activeRequestCount(), 0);

    const quint64 pingId = client.ping(1500);
    QTRY_VERIFY_WITH_TIMEOUT(responses.count() >= 2, 2500);
    QCOMPARE(responseFrom(responses, 1).requestId, pingId);
    QVERIFY(responseFrom(responses, 1).pong);
    client.shutdown();
}

void StreamgetProcessClientTest::rejectsExplicitMismatchedOperation()
{
    StreamgetProcessClient client(fakeServicePath(), {QStringLiteral("--wrong-ping-operation")});
    QSignalSpy responses(&client, &StreamgetProcessClient::responseReceived);
    QSignalSpy failures(&client, &StreamgetProcessClient::requestFailed);

    const quint64 pingId = client.ping(1500);
    QTRY_VERIFY_WITH_TIMEOUT(failures.count() >= 1, 2500);
    QCOMPARE(failures.at(0).at(0).toULongLong(), pingId);
    QCOMPARE(failures.at(0).at(1).toString(), QStringLiteral("INVALID_RESPONSE"));
    QCOMPARE(responses.count(), 0);
    QCOMPARE(client.activeRequestCount(), 0);
    client.shutdown();
}

void StreamgetProcessClientTest::acceptsExplicitMatchingOperation()
{
    StreamgetProcessClient client(fakeServicePath(), {QStringLiteral("--matching-ping-operation")});
    QSignalSpy responses(&client, &StreamgetProcessClient::responseReceived);
    QSignalSpy failures(&client, &StreamgetProcessClient::requestFailed);

    const quint64 pingId = client.ping(1500);
    QTRY_VERIFY_WITH_TIMEOUT(responses.count() >= 1, 2500);
    const ServiceResponse response = responseFrom(responses);
    QCOMPARE(response.requestId, pingId);
    QVERIFY(response.ok);
    QVERIFY(response.pong);
    QVERIFY(response.operation.has_value());
    QCOMPARE(*response.operation, ServiceOperation::Ping);
    QCOMPARE(failures.count(), 0);
    client.shutdown();
}

void StreamgetProcessClientTest::rejectsRequestsWhenQueueIsFull()
{
    StreamgetProcessClient client(fakeServicePath(),
                                  {QStringLiteral("--delay-ms"), QStringLiteral("5000")});
    QSignalSpy failures(&client, &StreamgetProcessClient::requestFailed);

    QVERIFY(client.resolve(QStringLiteral("63136"), StreamQuality::Auto, 60000) > 0);
    QVERIFY(client.resolve(QStringLiteral("100"), StreamQuality::Auto, 60000) > 0);
    QTRY_COMPARE_WITH_TIMEOUT(client.activeRequestCount(), 2, 1000);

    for (int index = 0; index < StreamgetProcessClient::kMaxQueuedRequests; ++index) {
        QVERIFY(client.status(QString::number(200 + index), 60000) > 0);
    }
    QCOMPARE(client.queuedRequestCount(), StreamgetProcessClient::kMaxQueuedRequests);

    QCOMPARE(client.status(QStringLiteral("999"), 60000), quint64(0));
    QCOMPARE(client.queuedRequestCount(), StreamgetProcessClient::kMaxQueuedRequests);
    QCOMPARE(failures.count(), 0);

    client.shutdown(100);
}

void StreamgetProcessClientTest::queueTimeoutIncludesWaitingTime()
{
    StreamgetProcessClient client(fakeServicePath(),
                                  {QStringLiteral("--delay-ms"), QStringLiteral("350")});
    QSignalSpy responses(&client, &StreamgetProcessClient::responseReceived);
    QSignalSpy failures(&client, &StreamgetProcessClient::requestFailed);

    QVERIFY(client.resolve(QStringLiteral("63136"), StreamQuality::Auto, 5000) > 0);
    QVERIFY(client.resolve(QStringLiteral("100"), StreamQuality::Auto, 5000) > 0);
    QTRY_COMPARE_WITH_TIMEOUT(client.activeRequestCount(), 2, 1000);

    const quint64 queued = client.resolve(QStringLiteral("200"), StreamQuality::Auto, 100);
    QVERIFY(queued > 0);
    QCOMPARE(client.queuedRequestCount(), 1);

    QTRY_VERIFY_WITH_TIMEOUT(( [&] {
        for (const QList<QVariant> &arguments : failures) {
            if (arguments.at(0).toULongLong() == queued
                && arguments.at(1).toString() == QStringLiteral("TIMEOUT")) {
                return true;
            }
        }
        return false;
    }() ), 250);
    QCOMPARE(client.queuedRequestCount(), 0);

    QTest::qWait(500);
    for (const QList<QVariant> &arguments : responses) {
        QVERIFY(qvariant_cast<ServiceResponse>(arguments.at(0)).requestId != queued);
    }

    client.shutdown(100);
}

void StreamgetProcessClientTest::activeTimeoutUsesRemainingTotalBudget()
{
    StreamgetProcessClient client(fakeServicePath(),
                                  {QStringLiteral("--delay-ms"), QStringLiteral("350")});
    QSignalSpy failures(&client, &StreamgetProcessClient::requestFailed);

    QVERIFY(client.resolve(QStringLiteral("63136"), StreamQuality::Auto, 5000) > 0);
    QVERIFY(client.resolve(QStringLiteral("100"), StreamQuality::Auto, 5000) > 0);
    QTRY_COMPARE_WITH_TIMEOUT(client.activeRequestCount(), 2, 1000);

    QElapsedTimer elapsed;
    elapsed.start();
    const quint64 queued = client.resolve(QStringLiteral("200"), StreamQuality::Auto, 400);
    QVERIFY(queued > 0);
    QTRY_VERIFY_WITH_TIMEOUT(( [&] {
        for (const QList<QVariant> &arguments : failures) {
            if (arguments.at(0).toULongLong() == queued
                && arguments.at(1).toString() == QStringLiteral("TIMEOUT")) {
                return true;
            }
        }
        return false;
    }() ), 1000);
    QVERIFY2(elapsed.elapsed() < 600,
             qPrintable(QStringLiteral("timeout elapsed=%1 ms").arg(elapsed.elapsed())));
    QCOMPARE(client.queuedRequestCount(), 0);

    client.shutdown(100);
}

void StreamgetProcessClientTest::cancelsQueuedRequestWithoutAffectingNeighbors()
{
    StreamgetProcessClient client(fakeServicePath(),
                                  {QStringLiteral("--delay-ms"), QStringLiteral("120")});
    QSignalSpy responses(&client, &StreamgetProcessClient::responseReceived);
    QSignalSpy failures(&client, &StreamgetProcessClient::requestFailed);

    QVERIFY(client.resolve(QStringLiteral("63136"), StreamQuality::Auto, 5000) > 0);
    QVERIFY(client.resolve(QStringLiteral("100"), StreamQuality::Auto, 5000) > 0);
    QTRY_COMPARE_WITH_TIMEOUT(client.activeRequestCount(), 2, 1000);

    const quint64 cancelled = client.status(QStringLiteral("200"), 5000);
    const quint64 retained = client.status(QStringLiteral("201"), 5000);
    QVERIFY(cancelled > 0);
    QVERIFY(retained > 0);
    QCOMPARE(client.queuedRequestCount(), 2);

    QVERIFY(client.cancel(cancelled));
    QCOMPARE(client.queuedRequestCount(), 1);
    QCOMPARE(failures.count(), 1);
    QCOMPARE(failures.at(0).at(0).toULongLong(), cancelled);
    QCOMPARE(failures.at(0).at(1).toString(), QStringLiteral("CANCELLED"));

    waitForSignalCount(responses, 3, 3000);
    bool retainedCompleted = false;
    for (const QList<QVariant> &arguments : responses) {
        retainedCompleted |= qvariant_cast<ServiceResponse>(arguments.at(0)).requestId == retained;
    }
    QVERIFY(retainedCompleted);
    QCOMPARE(failures.count(), 1);
    client.shutdown(100);
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

void StreamgetProcessClientTest::rejectsOversizedChildLine()
{
    StreamgetProcessClient client(fakeServicePath(), {QStringLiteral("--oversized-line")});
    QSignalSpy failures(&client, &StreamgetProcessClient::requestFailed);

    const quint64 requestId = client.ping(1500);
    waitForSignalCount(failures, 1);
    QCOMPARE(failures.at(0).at(0).toULongLong(), requestId);
    QCOMPARE(failures.at(0).at(1).toString(), QStringLiteral("INVALID_RESPONSE"));
    client.shutdown(100);
}

void StreamgetProcessClientTest::rejectsOversizedChildOutputWithoutNewline()
{
    StreamgetProcessClient client(fakeServicePath(), {QStringLiteral("--oversized-no-newline")});
    QSignalSpy failures(&client, &StreamgetProcessClient::requestFailed);

    const quint64 requestId = client.ping(1500);
    waitForSignalCount(failures, 1);
    QCOMPARE(failures.at(0).at(0).toULongLong(), requestId);
    QCOMPARE(failures.at(0).at(1).toString(), QStringLiteral("INVALID_RESPONSE"));
    client.shutdown(100);
}

void StreamgetProcessClientTest::drainsBurstStderrWithoutMixingWithStdout()
{
    StreamgetProcessClient client(fakeServicePath(),
                                  {QStringLiteral("--stderr-bytes"), QStringLiteral("4194304")});
    auto *process = client.findChild<QProcess *>();
    QVERIFY(process != nullptr);
    QSignalSpy responses(&client, &StreamgetProcessClient::responseReceived);
    QSignalSpy failures(&client, &StreamgetProcessClient::requestFailed);

    const quint64 requestId = client.ping(5000);
    waitForSignalCount(responses, 1, 5000);
    QCOMPARE(responseFrom(responses).requestId, requestId);
    QVERIFY(responseFrom(responses).pong);
    QCOMPARE(failures.count(), 0);
    QCOMPARE(process->readAllStandardError().size(), qsizetype(0));
    const quint64 second = client.ping(5000);
    waitForSignalCount(responses, 2, 5000);
    QCOMPARE(responseFrom(responses, 1).requestId, second);
    QCOMPARE(process->readAllStandardError().size(), qsizetype(0));
    client.shutdown(500);
}

void StreamgetProcessClientTest::drainsContinuousStderrWhileDeadlineExpires()
{
    StreamgetProcessClient client(fakeServicePath(),
                                  {QStringLiteral("--stderr-continuous")});
    auto *process = client.findChild<QProcess *>();
    QVERIFY(process != nullptr);
    QSignalSpy failures(&client, &StreamgetProcessClient::requestFailed);
    QSignalSpy responses(&client, &StreamgetProcessClient::responseReceived);
    QElapsedTimer elapsed;
    elapsed.start();
    const quint64 requestId = client.ping(250);
    waitForSignalCount(failures, 1, 2000);
    QCOMPARE(failures.at(0).at(0).toULongLong(), requestId);
    QCOMPARE(failures.at(0).at(1).toString(), QStringLiteral("TIMEOUT"));
    QVERIFY2(elapsed.elapsed() < 1500, "stderr activity delayed the request deadline");
    QCOMPARE(responses.count(), 0);
    QCOMPARE(process->readAllStandardError().size(), qsizetype(0));
    client.shutdown(500);
    QTest::qWait(50);
    QCOMPARE(failures.count(), 1);
    QVERIFY(!client.isRunning());
}

void StreamgetProcessClientTest::drainsStderrAcrossExitAndRestart()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString marker = temporaryDirectory.filePath(QStringLiteral("crashed-once.marker"));
    StreamgetProcessClient client(fakeServicePath(),
                                  {QStringLiteral("--stderr-bytes"), QStringLiteral("1048576"),
                                   QStringLiteral("--crash-after"), QStringLiteral("1"),
                                   QStringLiteral("--crash-once-file"), marker,
                                   QStringLiteral("--stderr-on-exit")});
    auto *process = client.findChild<QProcess *>();
    QVERIFY(process != nullptr);
    QSignalSpy failures(&client, &StreamgetProcessClient::requestFailed);
    QSignalSpy responses(&client, &StreamgetProcessClient::responseReceived);
    QSignalSpy stopped(&client, &StreamgetProcessClient::childStopped);

    const quint64 first = client.ping(3000);
    waitForSignalCount(failures, 1);
    QCOMPARE(failures.at(0).at(0).toULongLong(), first);
    QCOMPARE(failures.at(0).at(1).toString(), QStringLiteral("SERVICE_FAILED"));
    waitForSignalCount(stopped, 1);
    QCOMPARE(process->readAllStandardError().size(), qsizetype(0));

    const quint64 second = client.ping(3000);
    waitForSignalCount(responses, 1);
    QCOMPARE(responseFrom(responses).requestId, second);
    QVERIFY(responseFrom(responses).pong);
    QCOMPARE(process->readAllStandardError().size(), qsizetype(0));
    client.shutdown(500);
    waitForSignalCount(stopped, 2);
    QCOMPARE(process->readAllStandardError().size(), qsizetype(0));
    QCOMPARE(failures.count(), 1);
}

void StreamgetProcessClientTest::drainsStderrOnProtocolViolation()
{
    StreamgetProcessClient client(fakeServicePath(),
                                  {QStringLiteral("--stderr-bytes"), QStringLiteral("1048576"),
                                   QStringLiteral("--malformed")});
    auto *process = client.findChild<QProcess *>();
    QVERIFY(process != nullptr);
    QSignalSpy failures(&client, &StreamgetProcessClient::requestFailed);
    QSignalSpy responses(&client, &StreamgetProcessClient::responseReceived);
    QSignalSpy stopped(&client, &StreamgetProcessClient::childStopped);

    const quint64 requestId = client.ping(3000);
    waitForSignalCount(failures, 1);
    QCOMPARE(failures.at(0).at(0).toULongLong(), requestId);
    QCOMPARE(failures.at(0).at(1).toString(), QStringLiteral("INVALID_RESPONSE"));
    waitForSignalCount(stopped, 1);
    QCOMPARE(process->readAllStandardError().size(), qsizetype(0));
    QCOMPARE(responses.count(), 0);
    QCOMPARE(failures.count(), 1);
    client.shutdown(100);
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

    const quint64 restarted = client.ping(5000);
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
