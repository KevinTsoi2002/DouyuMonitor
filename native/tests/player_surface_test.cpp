#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "media/media_source.h"
#include "media/player_surface.h"

class PlayerSurfaceTest final : public QObject {
    Q_OBJECT

private slots:
    void isOwnedByGuiThread();
    void rejectsMissingMediaWithErrorState();
    void togglesPauseState();
    void loadsLocalImageAndPresentsFirstFrame();
    void stopsAfterFirstFrame();
    void repeatedStopIsIdempotent();
    void releasesAndLoadsFreshMedia();
    void rejectsInvalidSourceWithoutChangingIdleState();
    void acceptsValidatedRemoteSourceForLoading();
};

void PlayerSurfaceTest::isOwnedByGuiThread()
{
    PlayerSurface surface;

    QCOMPARE(surface.thread(), QThread::currentThread());
    QVERIFY(surface.isMpvInitialized());
}

void PlayerSurfaceTest::rejectsMissingMediaWithErrorState()
{
    PlayerSurface surface;

    QCOMPARE(surface.playbackState(), PlayerSurface::PlaybackState::Idle);
    QVERIFY(!surface.loadLocalMedia(QStringLiteral("missing-file.ppm")));
    QCOMPARE(surface.playbackState(), PlayerSurface::PlaybackState::Error);
    QVERIFY(!surface.mediaError().isEmpty());
}

void PlayerSurfaceTest::togglesPauseState()
{
    PlayerSurface surface;

    QVERIFY(surface.setPaused(true));
    QVERIFY(surface.isPaused());
    QVERIFY(surface.setPaused(false));
    QVERIFY(!surface.isPaused());
}

void PlayerSurfaceTest::loadsLocalImageAndPresentsFirstFrame()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    QFile fixture(temporaryDirectory.filePath(QStringLiteral("frame.ppm")));
    QVERIFY(fixture.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray ppm =
        "P3\n"
        "2 2\n"
        "255\n"
        "255 0 0  0 255 0\n"
        "0 0 255  255 255 255\n";
    QCOMPARE(fixture.write(ppm), static_cast<qint64>(ppm.size()));
    fixture.close();

    PlayerSurface surface;
    surface.resize(320, 240);
    surface.show();
    QVERIFY(QTest::qWaitForWindowExposed(&surface));

    QVERIFY(surface.loadLocalMedia(fixture.fileName()));
    QCOMPARE(surface.playbackState(), PlayerSurface::PlaybackState::Loading);
    QTRY_VERIFY_WITH_TIMEOUT(surface.isMediaLoaded(), 10000);
    QTRY_VERIFY_WITH_TIMEOUT(surface.isFirstFrameRendered(), 10000);
    const auto state = surface.playbackState();
    QVERIFY(state == PlayerSurface::PlaybackState::Playing
            || state == PlayerSurface::PlaybackState::Ended);
}

void PlayerSurfaceTest::stopsAfterFirstFrame()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    QFile fixture(temporaryDirectory.filePath(QStringLiteral("frame.ppm")));
    QVERIFY(fixture.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray ppm = "P3\n1 1\n255\n255 0 0\n";
    QCOMPARE(fixture.write(ppm), static_cast<qint64>(ppm.size()));
    fixture.close();

    PlayerSurface surface;
    surface.resize(320, 240);
    surface.show();
    QVERIFY(QTest::qWaitForWindowExposed(&surface));
    QVERIFY(surface.loadLocalMedia(fixture.fileName()));
    QTRY_VERIFY_WITH_TIMEOUT(surface.isFirstFrameRendered(), 10000);

    QVERIFY(surface.stop());
    QVERIFY(surface.playbackState() == PlayerSurface::PlaybackState::Ended
            || surface.playbackState() == PlayerSurface::PlaybackState::Idle);
    QVERIFY(!surface.isMediaLoaded());
    QVERIFY(!surface.isFirstFrameRendered());
    QVERIFY(surface.mediaError().isEmpty());
}

void PlayerSurfaceTest::repeatedStopIsIdempotent()
{
    PlayerSurface surface;

    QVERIFY(surface.stop());
    const auto state = surface.playbackState();
    QVERIFY(surface.stop());
    QCOMPARE(surface.playbackState(), state);
    QVERIFY(!surface.isMediaLoaded());
    QVERIFY(!surface.isFirstFrameRendered());
    QVERIFY(surface.mediaError().isEmpty());
}

void PlayerSurfaceTest::releasesAndLoadsFreshMedia()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    QFile fixture(temporaryDirectory.filePath(QStringLiteral("frame.ppm")));
    QVERIFY(fixture.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray ppm = "P3\n1 1\n255\n0 255 0\n";
    QCOMPARE(fixture.write(ppm), static_cast<qint64>(ppm.size()));
    fixture.close();

    PlayerSurface surface;
    surface.resize(320, 240);
    surface.show();
    QVERIFY(QTest::qWaitForWindowExposed(&surface));
    QVERIFY(surface.loadLocalMedia(fixture.fileName()));
    QTRY_VERIFY_WITH_TIMEOUT(surface.isFirstFrameRendered(), 10000);

    surface.release();
    QCOMPARE(surface.playbackState(), PlayerSurface::PlaybackState::Idle);
    QVERIFY(!surface.isMediaLoaded());
    QVERIFY(!surface.isFirstFrameRendered());
    QVERIFY(surface.mediaError().isEmpty());

    QVERIFY(surface.loadLocalMedia(fixture.fileName()));
    QTRY_VERIFY_WITH_TIMEOUT(surface.isFirstFrameRendered(), 10000);
    QVERIFY(surface.playbackState() == PlayerSurface::PlaybackState::Playing
            || surface.playbackState() == PlayerSurface::PlaybackState::Ended);
}

void PlayerSurfaceTest::rejectsInvalidSourceWithoutChangingIdleState()
{
    const StreamVariant unsafeVariant{
        QStringLiteral("flv-auto"),
        QStringLiteral("Auto"),
        StreamQuality::Auto,
        QStringLiteral("flv"),
        QUrl(QStringLiteral("https://example.invalid/live.flv?token=redacted")),
    };
    const auto source = MediaSource::fromRemoteVariant(QStringLiteral("63136"), unsafeVariant);
    PlayerSurface surface;

    QVERIFY(!source.has_value());
    QCOMPARE(surface.playbackState(), PlayerSurface::PlaybackState::Idle);
    QVERIFY(!surface.mediaError().contains(QStringLiteral("token"), Qt::CaseInsensitive));
}

void PlayerSurfaceTest::acceptsValidatedRemoteSourceForLoading()
{
    const StreamVariant variant{
        QStringLiteral("flv-auto"),
        QStringLiteral("Auto"),
        StreamQuality::Auto,
        QStringLiteral("flv"),
        QUrl(QStringLiteral("https://live.douyucdn.cn/live/test.flv?wsAuth=redacted")),
    };
    const auto source = MediaSource::fromRemoteVariant(QStringLiteral("63136"), variant);
    PlayerSurface surface;

    QVERIFY(source.has_value());
    QVERIFY(surface.loadSource(*source));
    QCOMPARE(surface.playbackState(), PlayerSurface::PlaybackState::Loading);
    QVERIFY(surface.stop());
}

QTEST_MAIN(PlayerSurfaceTest)

#include "player_surface_test.moc"
