#include <QSignalSpy>
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
};

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

QTEST_GUILESS_MAIN(RoomSessionTest)

#include "room_session_test.moc"
