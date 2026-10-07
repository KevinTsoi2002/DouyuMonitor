#include <QSignalSpy>
#include <QTimer>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QImage>
#include <QtTest/QtTest>

#include "service/streamget_process_client.h"
#include "ui/mpv_quick_item.h"
#include "workspace/room_session.h"

#ifndef FAKE_STREAMGET_SERVICE_PATH
#define FAKE_STREAMGET_SERVICE_PATH "fake_streamget_service"
#endif

namespace {

QString fakeServicePath()
{
    return QString::fromLocal8Bit(FAKE_STREAMGET_SERVICE_PATH);
}

} // namespace

class RoomSessionTest final : public QObject {
    Q_OBJECT

private slots:
    void startsResolvingWithUserAndEffectiveQuality();
    void updatesRequestedQualityWithoutChangingEffectiveQuality();
    void appliesRateBasedQualityOptions();
    void mapsOfflineResolveToLiveOfflineWithoutPlaybackFailure();
    void appliesValidatedMetadataAndLiveState();
    void preservesLiveStateWhenMetadataStatusIsUnknown();
    void acceptsMetadataWithOptionalEmptyPresentationFields();
    void dropsUnsafeAvatarUrlFromMetadata();
    void mapsQuickPlayerFailureToPlaybackError();
    void acceptsSourceAndKeepsItPendingWithoutRenderContext();
    void defersResolvedSourceUntilQuickPlayerIsAttached();
    void defersResolvedSourceUntilQuickRendererIsReady();
    void preservesPendingSourceWhenQuickPlayerIsReattached();
    void cancelSuppressesLateSource();
    void releasesSessionWithQuickPlayer();
    void mapsControllerErrorsWithoutRawDiagnostics();
    void retriesRemotePlayerFailureOnceAndPreservesQuality();
    void stopsPendingRemoteRecovery();
    void cancelsRemoteRecoveryWhenRoomGoesOffline();
    void boundsConsecutiveRecoveryAndResetsOnManualResolve();
    void retriesResolutionFailureDuringRecovery();
    void doesNotRecoverLocalPlayerFailure();
    void schedulesSourcePrefetchAfterPlaybackProgress();
    void isolatesPrefetchFailureAndCancelsOnStop();
    void prefetchesWithoutChangingActivePlaybackOrQuality();
    void boundsPrefetchRetriesAndInvalidatesOnQualityChange();
    void rejectsLatePrefetchAfterOfflineAndDetach();
    void sustainsLivePlaybackAcrossTwoSourceRotations();
};

void RoomSessionTest::prefetchesWithoutChangingActivePlaybackOrQuality()
{
    StreamgetProcessClient client(fakeServicePath());
    RoomSession session(&client, "63136", StreamQuality::High, nullptr, {}, 8);
    MpvQuickItem player;
    QVERIFY(session.attachPlayer(&player));
    QVERIFY(session.resolve());
    QTRY_VERIFY(session.hasPendingSourceForTest());
    session.activeSource_ = session.pendingSource_;
    session.pendingSource_.reset();
    session.setState(RoomSession::State::Ready);
    session.setPlaybackHealth(RoomPlaybackHealth::Playing);
    player.playbackProgress();
    auto *timer = session.findChild<QTimer *>("sourcePrefetchTimer");
    QVERIFY(timer->isActive());
    const int delay = timer->remainingTime();
    player.playbackProgress();
    QVERIFY(timer->remainingTime() <= delay);
    QSignalSpy states(&session, &RoomSession::stateChanged);
    QSignalSpy failures(&session, &RoomSession::failed);
    timer->start(1);
    QTRY_VERIFY(session.prefetchedSource_.has_value());
    QCOMPARE(session.state(), RoomSession::State::Ready);
    QCOMPARE(session.playbackHealth(), RoomPlaybackHealth::Playing);
    QCOMPARE(states.count(), 0);
    QCOMPARE(failures.count(), 0);
    QCOMPARE(session.effectiveQuality(), StreamQuality::High);
    QCOMPARE(session.effectiveQualityRate(), 8);
    QVERIFY(session.activeSource_);
    session.stop();
    QVERIFY(!session.prefetchedSource_);
    client.shutdown();
}

void RoomSessionTest::boundsPrefetchRetriesAndInvalidatesOnQualityChange()
{
    StreamgetProcessClient client(fakeServicePath());
    RoomSession session(&client, "63136", StreamQuality::High, nullptr, {}, 8);
    MpvQuickItem player;
    QVERIFY(session.attachPlayer(&player));
    StreamVariant variant{"auto", "auto", StreamQuality::Auto, "flv",
                          QUrl("https://unit-test.douyucdn.cn/live.flv")};
    session.activeSource_ = MediaSource::fromRemoteVariant("63136", variant);
    session.setState(RoomSession::State::Ready);
    session.prefetchAttempts_ = 3;
    session.onPrefetchFailed("TIMEOUT");
    QVERIFY(!session.sourcePrefetchTimer_->isActive());
    session.prefetchedSource_ = session.activeSource_;
    session.prefetchAge_.start();
    QVERIFY(session.setEffectiveQualityRate(4));
    QVERIFY(!session.prefetchedSource_);
    QCOMPARE(session.prefetchController_->state(), RemotePlaybackController::State::Idle);
    QVERIFY(!session.sourcePrefetchTimer_->isActive());
    session.stop();
    client.shutdown();
}

void RoomSessionTest::rejectsLatePrefetchAfterOfflineAndDetach()
{
    StreamgetProcessClient client(fakeServicePath(), {"--delay-ms", "150", "--ignore-cancel"});
    RoomSession session(&client, "63136", StreamQuality::Auto);
    MpvQuickItem player;
    QVERIFY(session.attachPlayer(&player));
    QVERIFY(session.resolve());
    QTRY_VERIFY(session.hasPendingSourceForTest());
    session.activeSource_ = session.pendingSource_;
    session.pendingSource_.reset();
    session.setState(RoomSession::State::Ready);
    session.beginSourcePrefetch();
    session.setLiveStatus(RoomLiveStatus::Offline);
    QTest::qWait(250);
    QVERIFY(!session.prefetchedSource_);
    session.setLiveStatus(RoomLiveStatus::Online);
    session.beginSourcePrefetch();
    session.detachPlayer(&player);
    QTest::qWait(250);
    QVERIFY(!session.prefetchedSource_);
    QVERIFY(!session.sourcePrefetchTimer_->isActive());
    client.shutdown();
}

void RoomSessionTest::sustainsLivePlaybackAcrossTwoSourceRotations()
{
    const QString service = qEnvironmentVariable("DOUYU_LIVE_VERIFY_SERVICE");
    if (service.isEmpty()) QSKIP("Set DOUYU_LIVE_VERIFY_SERVICE for opt-in real-network verification.");
    StreamgetProcessClient client(service);
    RoomSession session(&client, "217331", StreamQuality::Auto);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QQuickWindow window;
    window.resize(640, 360);
    auto *player = new MpvQuickItem(window.contentItem());
    player->setSize(QSizeF(640, 360));
    QVERIFY(session.attachPlayer(player));
    QVERIFY(session.setVolume(37));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTRY_VERIFY_WITH_TIMEOUT(player->isRenderContextReady(), 10000);
    QSignalSpy endings(player, &MpvQuickItem::remoteStreamEnded);
    QSignalSpy failures(&session, &RoomSession::failed);
    QSignalSpy progress(player, &MpvQuickItem::playbackProgress);
    QVERIFY(session.resolve());
    QTRY_VERIFY_WITH_TIMEOUT(player->isFirstFrameRendered(), 30000);
    QElapsedTimer elapsed;
    elapsed.start();
    int previousEndings = 0;
    qint64 lastRotationAt = 0;
    int framesAfterRotation = 0;
    while (elapsed.elapsed() < 670'000) {
        QTest::qWait(1000);
        QVERIFY2(failures.isEmpty(), "Real stream entered delayed error recovery.");
        QVERIFY(session.state() == RoomSession::State::Ready);
        if (endings.count() != previousEndings) {
            previousEndings = endings.count();
            lastRotationAt = elapsed.elapsed();
            framesAfterRotation = 0;
            qInfo() << "live source rotation" << previousEndings << "elapsedMs" << lastRotationAt;
        }
        const QImage frame = window.grabWindow();
        QVERIFY(!frame.isNull());
        int lit = 0;
        for (int y = 0; y < frame.height(); y += 24) {
            for (int x = 0; x < frame.width(); x += 24) {
                const QColor pixel = frame.pixelColor(x, y);
                if (pixel.red() + pixel.green() + pixel.blue() > 45) ++lit;
            }
        }
        if (previousEndings > 0 && lit > 20 && player->isFirstFrameRendered()) ++framesAfterRotation;
        if (previousEndings >= 2 && elapsed.elapsed() - lastRotationAt > 12000) break;
    }
    QVERIFY(endings.count() >= 2);
    QVERIFY(framesAfterRotation >= 5);
    QVERIFY(progress.count() > 150);
    QCOMPARE(player->volume(), 37);
    QVERIFY(player->isMuted());
    session.stop();
    client.shutdown();
}

void RoomSessionTest::schedulesSourcePrefetchAfterPlaybackProgress()
{
    StreamgetProcessClient client(fakeServicePath());
    RoomSession session(&client, "63136", StreamQuality::High, nullptr, {}, 8);
    MpvQuickItem player;
    QVERIFY(session.attachPlayer(&player));
    QVERIFY(session.resolve());
    QTRY_VERIFY(session.hasPendingSourceForTest());
    player.playbackProgress();
    auto *timer = session.findChild<QTimer *>("sourcePrefetchTimer");
    QVERIFY(timer);
    QVERIFY(!timer->isActive()); // No renderer has started this source yet.
    session.stop();
    QVERIFY(!timer->isActive());
    client.shutdown();
}

void RoomSessionTest::isolatesPrefetchFailureAndCancelsOnStop()
{
    StreamgetProcessClient client(fakeServicePath());
    RoomSession session(&client, "63136", StreamQuality::High, nullptr, {}, 8);
    MpvQuickItem player;
    QVERIFY(session.attachPlayer(&player));
    QVERIFY(session.resolve());
    QTRY_VERIFY(session.hasPendingSourceForTest());
    const auto previousState = session.state();
    QSignalSpy failures(&session, &RoomSession::failed);
    QVERIFY(QMetaObject::invokeMethod(&session, "onPrefetchFailed", Qt::DirectConnection,
                                      Q_ARG(QString, QStringLiteral("TIMEOUT"))));
    QCOMPARE(session.state(), previousState);
    QCOMPARE(failures.count(), 0);
    session.stop();
    auto *timer = session.findChild<QTimer *>("sourcePrefetchTimer");
    QVERIFY(timer);
    QVERIFY(!timer->isActive());
    client.shutdown();
}

void RoomSessionTest::boundsConsecutiveRecoveryAndResetsOnManualResolve()
{
    StreamgetProcessClient client(fakeServicePath());
    RoomSession session(&client, "63136", StreamQuality::Auto);
    MpvQuickItem player;
    QVERIFY(session.attachPlayer(&player));
    QSignalSpy responses(&client, &StreamgetProcessClient::responseReceived);
    QVERIFY(session.resolve());
    QTRY_VERIFY(session.hasPendingSourceForTest());
    auto *timer = session.findChild<QTimer *>("playbackRecoveryTimer");
    QVERIFY(timer);
    for (int attempt = 0; attempt < 5; ++attempt) {
        player.playbackFailed();
        QVERIFY(timer->isActive());
        QCOMPARE(timer->interval(), 3'000 << attempt);
        timer->start(1);
        QTRY_COMPARE(responses.count(), attempt + 2);
        QTRY_VERIFY(session.hasPendingSourceForTest());
    }
    player.playbackFailed();
    QVERIFY(!timer->isActive());
    QVERIFY(session.resolve());
    QTRY_COMPARE(responses.count(), 7);
    QTRY_VERIFY(session.hasPendingSourceForTest());
    player.playbackFailed();
    QVERIFY(timer->isActive());
    QCOMPARE(timer->interval(), 3000);
    session.stop();
    client.shutdown();
}

void RoomSessionTest::retriesResolutionFailureDuringRecovery()
{
    StreamgetProcessClient client(fakeServicePath());
    RoomSession session(&client, "63136", StreamQuality::Auto);
    MpvQuickItem player;
    QVERIFY(session.attachPlayer(&player));
    QVERIFY(session.resolve());
    QTRY_VERIFY(session.hasPendingSourceForTest());
    player.playbackFailed();
    auto *timer = session.findChild<QTimer *>("playbackRecoveryTimer");
    QVERIFY(timer);
    QVERIFY(QMetaObject::invokeMethod(&session, "recoverPlayback", Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(&session, "onControllerFailed", Qt::DirectConnection,
                                      Q_ARG(QString, QStringLiteral("TIMEOUT"))));
    QVERIFY(timer->isActive());
    session.stop();
    client.shutdown();
}

void RoomSessionTest::doesNotRecoverLocalPlayerFailure()
{
    StreamgetProcessClient client(fakeServicePath());
    RoomSession session(&client, "63136", StreamQuality::Auto);
    MpvQuickItem player;
    QVERIFY(session.attachPlayer(&player));
    QVERIFY(session.resolve());
    const auto source = MediaSource::fromDescriptor(QCoreApplication::applicationFilePath());
    QVERIFY(source);
    QVERIFY(QMetaObject::invokeMethod(&session, "onControllerSourceReady", Qt::DirectConnection,
                                      Q_ARG(MediaSource, *source)));
    player.playbackFailed();
    QCOMPARE(session.state(), RoomSession::State::Error);
    auto *timer = session.findChild<QTimer *>("playbackRecoveryTimer");
    QVERIFY(timer);
    QVERIFY(!timer->isActive());
    client.shutdown();
}

void RoomSessionTest::retriesRemotePlayerFailureOnceAndPreservesQuality()
{
    StreamgetProcessClient client(fakeServicePath());
    RoomSession session(&client, "63136", StreamQuality::High, nullptr, {}, 8);
    MpvQuickItem player;
    QVERIFY(session.attachPlayer(&player));
    QSignalSpy responses(&client, &StreamgetProcessClient::responseReceived);
    QVERIFY(session.resolve());
    QTRY_VERIFY(session.hasPendingSourceForTest());
    player.playbackFailed();
    auto *timer = session.findChild<QTimer *>("playbackRecoveryTimer");
    QVERIFY(timer);
    QVERIFY(timer->isActive());
    const int delay = timer->interval();
    player.playbackFailed();
    QCOMPARE(timer->interval(), delay);
    QCOMPARE(responses.count(), 1);
    timer->start(1);
    QTRY_COMPARE(responses.count(), 2);
    QCOMPARE(session.effectiveQuality(), StreamQuality::High);
    QCOMPARE(session.effectiveQualityRate(), 8);
    QVERIFY(session.hasPendingSourceForTest());
    client.shutdown();
}

void RoomSessionTest::stopsPendingRemoteRecovery()
{
    StreamgetProcessClient client(fakeServicePath());
    RoomSession session(&client, "63136", StreamQuality::Auto);
    MpvQuickItem player;
    QVERIFY(session.attachPlayer(&player));
    QSignalSpy responses(&client, &StreamgetProcessClient::responseReceived);
    QVERIFY(session.resolve());
    QTRY_VERIFY(session.hasPendingSourceForTest());
    player.playbackFailed();
    auto *timer = session.findChild<QTimer *>("playbackRecoveryTimer");
    QVERIFY(timer);
    QVERIFY(timer->isActive());
    session.stop();
    QVERIFY(!timer->isActive());
    QTest::qWait(100);
    QCOMPARE(responses.count(), 1);
    QCOMPARE(session.state(), RoomSession::State::Idle);
    client.shutdown();
}

void RoomSessionTest::cancelsRemoteRecoveryWhenRoomGoesOffline()
{
    StreamgetProcessClient client(fakeServicePath());
    RoomSession session(&client, "63136", StreamQuality::Auto);
    MpvQuickItem player;
    QVERIFY(session.attachPlayer(&player));
    QSignalSpy responses(&client, &StreamgetProcessClient::responseReceived);
    QVERIFY(session.resolve());
    QTRY_VERIFY(session.hasPendingSourceForTest());
    player.playbackFailed();
    auto *timer = session.findChild<QTimer *>("playbackRecoveryTimer");
    QVERIFY(timer);
    RoomSearchResult metadata;
    metadata.roomId = "63136";
    metadata.anchorName = "Anchor";
    metadata.online = false;
    session.applyMetadata(metadata);
    QVERIFY(!timer->isActive());
    QTest::qWait(100);
    QCOMPARE(responses.count(), 1);
    client.shutdown();
}

void RoomSessionTest::preservesLiveStateWhenMetadataStatusIsUnknown()
{
    StreamgetProcessClient client(fakeServicePath());
    RoomSession session(&client, "123", StreamQuality::Auto);
    RoomSearchResult metadata;
    metadata.roomId = "123";
    metadata.anchorName = "Anchor";
    metadata.online = true;
    session.applyMetadata(metadata);
    QCOMPARE(session.liveStatus(), RoomLiveStatus::Online);
    metadata.online = false;
    metadata.statusKnown = false;
    session.applyMetadata(metadata);
    QCOMPARE(session.liveStatus(), RoomLiveStatus::Online);
}

void RoomSessionTest::startsResolvingWithUserAndEffectiveQuality()
{
    StreamgetProcessClient client(fakeServicePath());
    RoomSession session(&client, QStringLiteral("63136"), StreamQuality::High);

    QCOMPARE(session.roomId(), QStringLiteral("63136"));
    QCOMPARE(session.userQuality(), StreamQuality::High);
    QVERIFY(session.setEffectiveQuality(StreamQuality::Standard));
    QCOMPARE(session.effectiveQuality(), StreamQuality::Standard);
    QVERIFY(session.resolve() > 0);
    QCOMPARE(session.state(), RoomSession::State::Resolving);
    client.shutdown();
}

void RoomSessionTest::updatesRequestedQualityWithoutChangingEffectiveQuality()
{
    StreamgetProcessClient client(fakeServicePath());
    RoomSession session(&client, QStringLiteral("63136"), StreamQuality::High);

    QVERIFY(session.setRequestedQuality(StreamQuality::Super));
    QCOMPARE(session.userQuality(), StreamQuality::Super);
    QCOMPARE(session.effectiveQuality(), StreamQuality::High);
    QVERIFY(!session.setRequestedQuality(StreamQuality::Super));
    client.shutdown();
}

void RoomSessionTest::appliesRateBasedQualityOptions()
{
    StreamgetProcessClient client(fakeServicePath());
    RoomSession session(&client, QStringLiteral("63136"), StreamQuality::Auto, nullptr, {}, 8);
    QSignalSpy variants(&session, &RoomSession::variantsChanged);

    QCOMPARE(session.userQualityRate(), 8);
    QCOMPARE(session.effectiveQualityRate(), 8);
    QVERIFY(session.resolve() > 0);
    QTRY_VERIFY_WITH_TIMEOUT(variants.count() == 1, 3000);

    const QVariantList options = session.availableQualities();
    QCOMPARE(options.size(), 5);
    QCOMPARE(options.at(0).toMap().value(QStringLiteral("rate")).toInt(), 0);
    QCOMPARE(options.at(1).toMap().value(QStringLiteral("label")).toString(),
             QStringLiteral("quality-8"));
    client.shutdown();
}

void RoomSessionTest::mapsOfflineResolveToLiveOfflineWithoutPlaybackFailure()
{
    StreamgetProcessClient client(fakeServicePath(),
                                  {QStringLiteral("--offline-after"), QStringLiteral("1")});
    RoomSession session(&client, QStringLiteral("63136"), StreamQuality::Auto);
    QSignalSpy failures(&session, &RoomSession::failed);

    QVERIFY(session.resolve() > 0);
    QTRY_COMPARE_WITH_TIMEOUT(session.liveStatus(), RoomLiveStatus::Offline, 3000);
    QCOMPARE(session.playbackHealth(), RoomPlaybackHealth::Pending);
    QCOMPARE(failures.count(), 0);
    QCOMPARE(session.state(), RoomSession::State::Idle);
    client.shutdown();
}

void RoomSessionTest::appliesValidatedMetadataAndLiveState()
{
    StreamgetProcessClient client(fakeServicePath());
    RoomSession session(&client, QStringLiteral("63136"), StreamQuality::Auto);
    const RoomSearchResult result{
        QStringLiteral("63136"), QStringLiteral("主播 A"), QStringLiteral("标题"),
        QStringLiteral("游戏"), true, QStringLiteral("1.2万"),
        QUrl::fromLocalFile(QCoreApplication::applicationFilePath())};

    session.applyMetadata(result);
    QCOMPARE(session.metadata().anchorName, QStringLiteral("主播 A"));
    QCOMPARE(session.metadata().title, QStringLiteral("标题"));
    QCOMPARE(session.liveStatus(), RoomLiveStatus::Online);
    client.shutdown();
}

void RoomSessionTest::acceptsMetadataWithOptionalEmptyPresentationFields()
{
    StreamgetProcessClient client(fakeServicePath());
    RoomSession session(&client, QStringLiteral("63136"), StreamQuality::Auto);
    const RoomSearchResult result{
        QStringLiteral("63136"), QStringLiteral("主播 A"), QString(), QString(), true, QString(), QUrl()};

    session.applyMetadata(result);
    QCOMPARE(session.metadata().anchorName, QStringLiteral("主播 A"));
    QCOMPARE(session.liveStatus(), RoomLiveStatus::Online);
    client.shutdown();
}

void RoomSessionTest::dropsUnsafeAvatarUrlFromMetadata()
{
    StreamgetProcessClient client(fakeServicePath());
    RoomSession session(&client, QStringLiteral("63136"), StreamQuality::Auto);
    const RoomSearchResult result{
        QStringLiteral("63136"), QStringLiteral("主播 A"), QStringLiteral("标题"),
        QStringLiteral("游戏"), true, QStringLiteral("1.2万"),
        QUrl(QStringLiteral("file:///private/avatar.jpg"))};

    session.applyMetadata(result);
    QCOMPARE(session.metadata().anchorName, QStringLiteral("主播 A"));
    QVERIFY(session.metadata().avatarUrl.isEmpty());
    client.shutdown();
}

void RoomSessionTest::mapsQuickPlayerFailureToPlaybackError()
{
    StreamgetProcessClient client(fakeServicePath());
    RoomSession session(&client, QStringLiteral("63136"), StreamQuality::Auto);
    QSignalSpy failures(&session, &RoomSession::failed);
    const auto source = MediaSource::fromDescriptor(QCoreApplication::applicationFilePath());
    MpvQuickItem player;

    QVERIFY(source.has_value());
    QVERIFY(session.attachPlayer(&player));
    QVERIFY(session.resolve() > 0);
    QCOMPARE(session.state(), RoomSession::State::Resolving);
    QVERIFY(QMetaObject::invokeMethod(&session, "onControllerSourceReady", Qt::DirectConnection,
                                      Q_ARG(MediaSource, *source)));
    QCOMPARE(session.state(), RoomSession::State::Resolving);
    QVERIFY(session.hasPendingSourceForTest());
    QVERIFY(QMetaObject::invokeMethod(&player, "playbackFailed", Qt::DirectConnection));
    QCOMPARE(session.playbackHealth(), RoomPlaybackHealth::Error);
    QCOMPARE(session.state(), RoomSession::State::Error);
    QCOMPARE(failures.count(), 1);
    QCOMPARE(failures.at(0).at(0).toString(), QStringLiteral("PLAYER_FAILED"));
    client.shutdown();
}

void RoomSessionTest::acceptsSourceAndKeepsItPendingWithoutRenderContext()
{
    StreamgetProcessClient client(fakeServicePath());
    RoomSession session(&client, QStringLiteral("63136"), StreamQuality::Auto);
    MpvQuickItem player;
    QVERIFY(session.attachPlayer(&player));

    QVERIFY(session.resolve() > 0);
    QTRY_VERIFY_WITH_TIMEOUT(session.hasPendingSourceForTest(), 3000);
    QCOMPARE(session.state(), RoomSession::State::Resolving);
    QCOMPARE(player.playbackState(), MpvQuickItem::PlaybackState::Idle);
    client.shutdown();
}

void RoomSessionTest::defersResolvedSourceUntilQuickPlayerIsAttached()
{
    StreamgetProcessClient client(fakeServicePath());
    RoomSession session(&client, QStringLiteral("63136"), StreamQuality::Auto);

    QVERIFY(session.resolve() > 0);
    QTRY_COMPARE_WITH_TIMEOUT(session.state(), RoomSession::State::Resolving, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(session.hasPendingSourceForTest(), 3000);

    MpvQuickItem player;
    QVERIFY(session.attachPlayer(&player));
    QCOMPARE(session.state(), RoomSession::State::Resolving);
    QVERIFY(session.hasPendingSourceForTest());
    client.shutdown();
}

void RoomSessionTest::defersResolvedSourceUntilQuickRendererIsReady()
{
    StreamgetProcessClient client(fakeServicePath());
    RoomSession session(&client, QStringLiteral("63136"), StreamQuality::Auto);

    QVERIFY(session.resolve() > 0);
    QTRY_VERIFY_WITH_TIMEOUT(session.hasPendingSourceForTest(), 3000);

    MpvQuickItem player;

    QVERIFY(session.attachPlayer(&player));
    QVERIFY(session.hasPendingSourceForTest());
    QCOMPARE(session.state(), RoomSession::State::Resolving);
    client.shutdown();
}

void RoomSessionTest::preservesPendingSourceWhenQuickPlayerIsReattached()
{
    StreamgetProcessClient client(fakeServicePath());
    RoomSession session(&client, QStringLiteral("63136"), StreamQuality::Auto);
    const auto source = MediaSource::fromDescriptor(QCoreApplication::applicationFilePath());
    MpvQuickItem firstPlayer;
    MpvQuickItem replacementPlayer;

    QVERIFY(source.has_value());
    QVERIFY(session.attachPlayer(&firstPlayer));
    QVERIFY(QMetaObject::invokeMethod(&session, "onControllerSourceReady", Qt::DirectConnection,
                                      Q_ARG(MediaSource, *source)));
    QCOMPARE(session.state(), RoomSession::State::Idle);
    QVERIFY(session.hasPendingSourceForTest());

    session.detachPlayer(&firstPlayer);
    QVERIFY(session.attachPlayer(&replacementPlayer));
    QCOMPARE(session.state(), RoomSession::State::Idle);
    QVERIFY(session.hasPendingSourceForTest());
    QCOMPARE(replacementPlayer.playbackState(), MpvQuickItem::PlaybackState::Idle);
    client.shutdown();
}

void RoomSessionTest::cancelSuppressesLateSource()
{
    StreamgetProcessClient client(fakeServicePath(),
                                  {QStringLiteral("--delay-ms"), QStringLiteral("150"),
                                   QStringLiteral("--ignore-cancel")});
    RoomSession session(&client, QStringLiteral("63136"), StreamQuality::Auto);
    QSignalSpy ready(&session, &RoomSession::sourceReady);

    QVERIFY(session.resolve() > 0);
    session.cancel();
    QCOMPARE(session.state(), RoomSession::State::Idle);
    QTest::qWait(400);
    QCOMPARE(ready.count(), 0);
    client.shutdown();
}

void RoomSessionTest::releasesSessionWithQuickPlayer()
{
    StreamgetProcessClient client(fakeServicePath());
    auto *session = new RoomSession(&client, QStringLiteral("63136"), StreamQuality::Auto);
    MpvQuickItem player;
    QVERIFY(session->attachPlayer(&player));
    session->release();
    QCOMPARE(session->state(), RoomSession::State::Idle);
    QCOMPARE(session->player(), nullptr);
    QCOMPARE(player.playbackState(), MpvQuickItem::PlaybackState::Idle);
    delete session;
    client.shutdown();
}

void RoomSessionTest::mapsControllerErrorsWithoutRawDiagnostics()
{
    StreamgetProcessClient client(fakeServicePath(),
                                  {QStringLiteral("--error-code"), QStringLiteral("SERVICE_FAILED")});
    RoomSession session(&client, QStringLiteral("63136"), StreamQuality::Auto);
    QSignalSpy failures(&session, &RoomSession::failed);

    QVERIFY(session.resolve() > 0);
    QTRY_VERIFY_WITH_TIMEOUT(failures.count() == 1, 3000);
    QCOMPARE(failures.at(0).at(0).toString(), QStringLiteral("SERVICE_FAILED"));
    QVERIFY(!failures.at(0).at(0).toString().contains(QStringLiteral("://")));
    QCOMPARE(session.state(), RoomSession::State::Error);
    client.shutdown();
}

QTEST_MAIN(RoomSessionTest)

#include "room_session_test.moc"
