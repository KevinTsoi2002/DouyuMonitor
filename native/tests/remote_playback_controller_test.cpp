#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "media/remote_playback_controller.h"
#include "service/streamget_process_client.h"

#ifndef FAKE_STREAMGET_SERVICE_PATH
#define FAKE_STREAMGET_SERVICE_PATH "fake_streamget_service"
#endif

namespace {

QString fakeServicePath()
{
    return QString::fromLocal8Bit(FAKE_STREAMGET_SERVICE_PATH);
}

StreamVariant validVariant()
{
    return {
        QStringLiteral("flv-auto"),
        QStringLiteral("fake"),
        StreamQuality::Auto,
        QStringLiteral("flv"),
        QUrl(QStringLiteral("https://live.douyucdn.cn/fake.flv")),
    };
}

ServiceResponse validResolveResponse(quint64 requestId, const QString &roomId)
{
    ServiceResponse response;
    response.requestId = requestId;
    response.ok = true;
    response.roomId = roomId;
    response.isLive = true;
    response.variants.push_back(validVariant());
    return response;
}

void waitForSignalCount(const QSignalSpy &spy, qsizetype count, int timeout = 3000)
{
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= count, timeout);
}

} // namespace

class RemotePlaybackControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void resolvesOneTypedSource();
    void mapsOfflineAndServiceErrorsToFixedCodes();
    void cancelSuppressesLateSource();
    void newerGenerationSuppressesOlderResponse();
    void timeoutAndRestartRemainRetryable();
    void releaseReturnsToIdleAndInvalidatesWork();
};

void RemotePlaybackControllerTest::initTestCase()
{
    qRegisterMetaType<ServiceResponse>();
    qRegisterMetaType<MediaSource>();
}

void RemotePlaybackControllerTest::resolvesOneTypedSource()
{
    StreamgetProcessClient client(fakeServicePath());
    RemotePlaybackController controller(&client);
    QSignalSpy sources(&controller, &RemotePlaybackController::sourceReady);
    QSignalSpy failures(&controller, &RemotePlaybackController::failed);

    const quint64 requestId = controller.resolve(QStringLiteral("63136"), StreamQuality::Auto);
    QVERIFY(requestId > 0);
    waitForSignalCount(sources, 1);

    const MediaSource source = qvariant_cast<MediaSource>(sources.at(0).at(0));
    QCOMPARE(source.kind(), MediaSource::Kind::RemoteStream);
    QCOMPARE(source.roomId(), QStringLiteral("63136"));
    QCOMPARE(source.variantId(), QStringLiteral("flv-auto"));
    QCOMPARE(source.stableDescription(), QStringLiteral("remote-stream"));
    QCOMPARE(controller.state(), RemotePlaybackController::State::Ready);
    QVERIFY(failures.isEmpty());

    client.shutdown();
}

void RemotePlaybackControllerTest::mapsOfflineAndServiceErrorsToFixedCodes()
{
    {
        StreamgetProcessClient client(fakeServicePath(), {QStringLiteral("--offline-after"), QStringLiteral("1")});
        RemotePlaybackController controller(&client);
        QSignalSpy failures(&controller, &RemotePlaybackController::failed);

        controller.resolve(QStringLiteral("63136"), StreamQuality::Auto);
        waitForSignalCount(failures, 1);
        QCOMPARE(failures.at(0).at(0).toString(), QStringLiteral("ROOM_OFFLINE"));
        QCOMPARE(controller.state(), RemotePlaybackController::State::Error);
        client.shutdown();
    }

    {
        StreamgetProcessClient client(fakeServicePath(),
                                      {QStringLiteral("--error-code"), QStringLiteral("STREAMGET_UNAVAILABLE")});
        RemotePlaybackController controller(&client);
        QSignalSpy failures(&controller, &RemotePlaybackController::failed);

        controller.resolve(QStringLiteral("63136"), StreamQuality::Auto);
        waitForSignalCount(failures, 1);
        QCOMPARE(failures.at(0).at(0).toString(), QStringLiteral("STREAMGET_UNAVAILABLE"));
        QCOMPARE(controller.state(), RemotePlaybackController::State::Error);
        client.shutdown();
    }
}

void RemotePlaybackControllerTest::cancelSuppressesLateSource()
{
    StreamgetProcessClient client(fakeServicePath(),
                                  {QStringLiteral("--delay-ms"), QStringLiteral("150"),
                                   QStringLiteral("--ignore-cancel")});
    RemotePlaybackController controller(&client);
    QSignalSpy sources(&controller, &RemotePlaybackController::sourceReady);
    QSignalSpy failures(&controller, &RemotePlaybackController::failed);

    controller.resolve(QStringLiteral("63136"), StreamQuality::Auto);
    controller.cancel();
    QCOMPARE(controller.state(), RemotePlaybackController::State::Idle);
    QTest::qWait(400);
    QCOMPARE(sources.count(), 0);
    QCOMPARE(failures.count(), 0);
    client.shutdown();
}

void RemotePlaybackControllerTest::newerGenerationSuppressesOlderResponse()
{
    StreamgetProcessClient client(fakeServicePath(), {QStringLiteral("--delay-ms"), QStringLiteral("200")});
    RemotePlaybackController controller(&client);
    QSignalSpy sources(&controller, &RemotePlaybackController::sourceReady);

    const quint64 olderRequest = controller.resolve(QStringLiteral("63136"), StreamQuality::Auto);
    const quint64 newerRequest = controller.resolve(QStringLiteral("100"), StreamQuality::High);
    QVERIFY(olderRequest != newerRequest);
    QVERIFY(controller.generation() >= 2);

    client.responseReceived(validResolveResponse(olderRequest, QStringLiteral("63136")));
    QCOMPARE(sources.count(), 0);
    client.responseReceived(validResolveResponse(newerRequest, QStringLiteral("100")));
    waitForSignalCount(sources, 1);
    const MediaSource source = qvariant_cast<MediaSource>(sources.at(0).at(0));
    QCOMPARE(source.roomId(), QStringLiteral("100"));
    controller.release();
    client.shutdown();
}

void RemotePlaybackControllerTest::timeoutAndRestartRemainRetryable()
{
    {
        StreamgetProcessClient client(fakeServicePath(), {QStringLiteral("--delay-ms"), QStringLiteral("11000")});
        RemotePlaybackController controller(&client);
        QSignalSpy failures(&controller, &RemotePlaybackController::failed);

        controller.resolve(QStringLiteral("63136"), StreamQuality::Auto);
        waitForSignalCount(failures, 1, 12000);
        QCOMPARE(failures.at(0).at(0).toString(), QStringLiteral("TIMEOUT"));
        QCOMPARE(controller.state(), RemotePlaybackController::State::Error);
        client.shutdown();
    }

    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());
    const QString marker = temporaryDirectory.filePath(QStringLiteral("crashed-once.marker"));
    StreamgetProcessClient client(
        fakeServicePath(),
        {QStringLiteral("--crash-after"), QStringLiteral("1"),
         QStringLiteral("--crash-once-file"), marker});
    RemotePlaybackController controller(&client);
    QSignalSpy sources(&controller, &RemotePlaybackController::sourceReady);
    QSignalSpy failures(&controller, &RemotePlaybackController::failed);

    controller.resolve(QStringLiteral("63136"), StreamQuality::Auto);
    waitForSignalCount(failures, 1);
    QCOMPARE(failures.at(0).at(0).toString(), QStringLiteral("SERVICE_FAILED"));

    controller.resolve(QStringLiteral("63136"), StreamQuality::Auto);
    waitForSignalCount(sources, 1, 3000);
    QCOMPARE(controller.state(), RemotePlaybackController::State::Ready);
    client.shutdown();
}

void RemotePlaybackControllerTest::releaseReturnsToIdleAndInvalidatesWork()
{
    StreamgetProcessClient client(fakeServicePath(),
                                  {QStringLiteral("--delay-ms"), QStringLiteral("150"),
                                   QStringLiteral("--ignore-cancel")});
    RemotePlaybackController controller(&client);
    QSignalSpy sources(&controller, &RemotePlaybackController::sourceReady);

    controller.resolve(QStringLiteral("63136"), StreamQuality::Auto);
    const quint64 beforeRelease = controller.generation();
    controller.release();
    QCOMPARE(controller.state(), RemotePlaybackController::State::Idle);
    QVERIFY(controller.generation() > beforeRelease);
    QTest::qWait(400);
    QCOMPARE(sources.count(), 0);
    client.shutdown();
}

QTEST_GUILESS_MAIN(RemotePlaybackControllerTest)

#include "remote_playback_controller_test.moc"
