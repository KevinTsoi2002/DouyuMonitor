#include <QSignalSpy>
#include <QWidget>
#include <QtTest/QtTest>

#include "service/streamget_process_client.h"
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
    void acceptsSourceAndReportsReady();
    void cancelSuppressesLateSource();
    void removesSessionStateWithoutLeakingSurface();
    void mapsControllerErrorsWithoutRawDiagnostics();
};

void RoomSessionTest::startsResolvingWithUserAndEffectiveQuality()
{
    StreamgetProcessClient client(fakeServicePath());
    QWidget host;
    RoomSession session(&client, QStringLiteral("63136"), StreamQuality::High, &host);

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
    QWidget host;
    RoomSession session(&client, QStringLiteral("63136"), StreamQuality::High, &host);

    QVERIFY(session.setRequestedQuality(StreamQuality::Super));
    QCOMPARE(session.userQuality(), StreamQuality::Super);
    QCOMPARE(session.effectiveQuality(), StreamQuality::High);
    QVERIFY(!session.setRequestedQuality(StreamQuality::Super));
    client.shutdown();
}

void RoomSessionTest::acceptsSourceAndReportsReady()
{
    StreamgetProcessClient client(fakeServicePath());
    QWidget host;
    RoomSession session(&client, QStringLiteral("63136"), StreamQuality::Auto, &host);
    QSignalSpy ready(&session, &RoomSession::sourceReady);

    QVERIFY(session.resolve() > 0);
    QTRY_VERIFY_WITH_TIMEOUT(ready.count() == 1, 3000);
    QCOMPARE(session.state(), RoomSession::State::Ready);
    QVERIFY(session.surface() != nullptr);
    QVERIFY(session.surface()->playbackState() == PlayerSurface::PlaybackState::Loading
            || session.surface()->playbackState() == PlayerSurface::PlaybackState::Error);
    client.shutdown();
}

void RoomSessionTest::cancelSuppressesLateSource()
{
    StreamgetProcessClient client(fakeServicePath(),
                                  {QStringLiteral("--delay-ms"), QStringLiteral("150"),
                                   QStringLiteral("--ignore-cancel")});
    QWidget host;
    RoomSession session(&client, QStringLiteral("63136"), StreamQuality::Auto, &host);
    QSignalSpy ready(&session, &RoomSession::sourceReady);

    QVERIFY(session.resolve() > 0);
    session.cancel();
    QCOMPARE(session.state(), RoomSession::State::Idle);
    QTest::qWait(400);
    QCOMPARE(ready.count(), 0);
    client.shutdown();
}

void RoomSessionTest::removesSessionStateWithoutLeakingSurface()
{
    StreamgetProcessClient client(fakeServicePath());
    QWidget host;
    auto *session = new RoomSession(&client, QStringLiteral("63136"), StreamQuality::Auto, &host);
    QVERIFY(session->surface() != nullptr);
    session->release();
    QCOMPARE(session->state(), RoomSession::State::Idle);
    QCOMPARE(session->surface()->playbackState(), PlayerSurface::PlaybackState::Idle);
    delete session;
    client.shutdown();
}

void RoomSessionTest::mapsControllerErrorsWithoutRawDiagnostics()
{
    StreamgetProcessClient client(fakeServicePath(),
                                  {QStringLiteral("--offline-after"), QStringLiteral("1")});
    QWidget host;
    RoomSession session(&client, QStringLiteral("63136"), StreamQuality::Auto, &host);
    QSignalSpy failures(&session, &RoomSession::failed);

    QVERIFY(session.resolve() > 0);
    QTRY_VERIFY_WITH_TIMEOUT(failures.count() == 1, 3000);
    QCOMPARE(failures.at(0).at(0).toString(), QStringLiteral("ROOM_OFFLINE"));
    QVERIFY(!failures.at(0).at(0).toString().contains(QStringLiteral("https://")));
    QCOMPARE(session.state(), RoomSession::State::Error);
    client.shutdown();
}

QTEST_MAIN(RoomSessionTest)

#include "room_session_test.moc"
