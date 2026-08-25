#include <QFile>
#include <QToolButton>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "app/main_window.h"
#include "media/player_surface.h"

#ifndef FAKE_STREAMGET_SERVICE_PATH
#define FAKE_STREAMGET_SERVICE_PATH "fake_streamget_service"
#endif

namespace {

QString fakeServicePath()
{
    return QString::fromLocal8Bit(FAKE_STREAMGET_SERVICE_PATH);
}

} // namespace

class MainWindowTest final : public QObject {
    Q_OBJECT

private slots:
    void createsSinglePlayerSurface();
    void forwardsLocalMediaLoad();
    void rejectsUnsupportedSourceBeforeMpv();
    void stopsMediaWithStopButton();
    void togglesPauseControl();
    void synchronizesPauseButtonWhenLoadingAndStopping();
    void supportsNineRoomGridAndLimit();
    void removesRoomAndPreservesRemainingSurfaces();
};

void MainWindowTest::createsSinglePlayerSurface()
{
    MainWindow window;

    QVERIFY(window.playerSurface() != nullptr);
    QVERIFY(window.centralWidget() != nullptr);
    QVERIFY(window.playerSurface()->isMpvInitialized());
}

void MainWindowTest::forwardsLocalMediaLoad()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    QFile fixture(temporaryDirectory.filePath(QStringLiteral("frame.ppm")));
    QVERIFY(fixture.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray ppm =
        "P3\n"
        "1 1\n"
        "255\n"
        "255 0 0\n";
    QCOMPARE(fixture.write(ppm), static_cast<qint64>(ppm.size()));
    fixture.close();

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QVERIFY(window.loadLocalMedia(fixture.fileName()));
}

void MainWindowTest::rejectsUnsupportedSourceBeforeMpv()
{
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QVERIFY(!window.loadLocalMedia(QStringLiteral("https://example.invalid/live")));
    QCOMPARE(window.playerSurface()->playbackState(), PlayerSurface::PlaybackState::Idle);
    QVERIFY(window.playerSurface()->mediaError().isEmpty());
}

void MainWindowTest::stopsMediaWithStopButton()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    QFile fixture(temporaryDirectory.filePath(QStringLiteral("frame.ppm")));
    QVERIFY(fixture.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray ppm = "P3\n1 1\n255\n255 0 0\n";
    QCOMPARE(fixture.write(ppm), static_cast<qint64>(ppm.size()));
    fixture.close();

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *stopButton = window.findChild<QToolButton *>(QStringLiteral("stopButton"));
    QVERIFY(stopButton != nullptr);
    QVERIFY(window.loadLocalMedia(fixture.fileName()));
    QTRY_VERIFY_WITH_TIMEOUT(window.playerSurface()->isFirstFrameRendered(), 10000);

    stopButton->click();
    QVERIFY(window.playerSurface()->playbackState() == PlayerSurface::PlaybackState::Ended
            || window.playerSurface()->playbackState() == PlayerSurface::PlaybackState::Idle);
    QVERIFY(!window.playerSurface()->isMediaLoaded());
    QVERIFY(!window.playerSurface()->isFirstFrameRendered());
}

void MainWindowTest::togglesPauseControl()
{
    MainWindow window;

    QVERIFY(window.pauseButton() != nullptr);
    QVERIFY(!window.pauseButton()->isChecked());

    window.pauseButton()->click();
    QVERIFY(window.playerSurface()->isPaused());

    window.pauseButton()->click();
    QVERIFY(!window.playerSurface()->isPaused());
}

void MainWindowTest::synchronizesPauseButtonWhenLoadingAndStopping()
{
    QTemporaryDir temporaryDirectory;
    QVERIFY(temporaryDirectory.isValid());

    QFile fixture(temporaryDirectory.filePath(QStringLiteral("frame.ppm")));
    QVERIFY(fixture.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray ppm = "P3\n1 1\n255\n0 255 0\n";
    QCOMPARE(fixture.write(ppm), static_cast<qint64>(ppm.size()));
    fixture.close();

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    window.pauseButton()->click();
    QVERIFY(window.pauseButton()->isChecked());
    QVERIFY(window.playerSurface()->isPaused());

    QVERIFY(window.loadLocalMedia(fixture.fileName()));
    QVERIFY(!window.pauseButton()->isChecked());
    QVERIFY(!window.playerSurface()->isPaused());

    window.pauseButton()->click();
    QVERIFY(window.pauseButton()->isChecked());
    auto *stopButton = window.findChild<QToolButton *>(QStringLiteral("stopButton"));
    QVERIFY(stopButton != nullptr);
    stopButton->click();
    QVERIFY(!window.pauseButton()->isChecked());
    QVERIFY(!window.playerSurface()->isPaused());
}

void MainWindowTest::supportsNineRoomGridAndLimit()
{
    MainWindow window(fakeServicePath());
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QVERIFY(window.addRoom(QStringLiteral("63136")));
    QCOMPARE(window.roomCount(), 1);
    QCOMPARE(window.layoutId(), QStringLiteral("single"));

    for (int index = 1; index < 4; ++index) {
        QVERIFY(window.addRoom(QString::number(63136 + index)));
    }
    QCOMPARE(window.roomCount(), 4);
    QCOMPARE(window.layoutId(), QStringLiteral("grid-2x2"));

    for (int index = 4; index < 9; ++index) {
        QVERIFY(window.addRoom(QString::number(63136 + index)));
    }
    QCOMPARE(window.roomCount(), 9);
    QCOMPARE(window.layoutId(), QStringLiteral("grid-3x3"));
    QVERIFY(!window.addRoom(QStringLiteral("999999")));
    QCOMPARE(window.roomCount(), 9);
}

void MainWindowTest::removesRoomAndPreservesRemainingSurfaces()
{
    MainWindow window(fakeServicePath());
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QVERIFY(window.addRoom(QStringLiteral("63136")));
    QVERIFY(window.addRoom(QStringLiteral("63137")));
    QVERIFY(window.addRoom(QStringLiteral("63138")));
    auto *remainingSurface = window.surfaceForRoom(QStringLiteral("63138"));
    QVERIFY(remainingSurface != nullptr);

    QVERIFY(window.removeRoom(QStringLiteral("63137")));
    QCOMPARE(window.roomCount(), 2);
    QCOMPARE(window.roomIds(), QStringList({QStringLiteral("63136"), QStringLiteral("63138")}));
    QCOMPARE(window.surfaceForRoom(QStringLiteral("63138")), remainingSurface);
    QVERIFY(window.surfaceForRoom(QStringLiteral("63137")) == nullptr);
}

QTEST_MAIN(MainWindowTest)

#include "main_window_test.moc"
