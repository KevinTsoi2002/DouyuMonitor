#include <QFile>
#include <QImage>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>

#include <memory>

#include "ui/mpv_quick_item.h"

namespace {
QString makePpmFixture(QTemporaryDir &directory)
{
    const QString path = directory.filePath(QStringLiteral("frame.ppm"));
    QFile fixture(path);
    if (!fixture.open(QIODevice::WriteOnly | QIODevice::Truncate)) return {};
    const QByteArray ppm = "P3\n1 1\n255\n255 0 0\n";
    if (fixture.write(ppm) != ppm.size()) return {};
    fixture.close();
    return path;
}

QString makeY4mFixture(QTemporaryDir &directory)
{
    const QString path = directory.filePath(QStringLiteral("frame.y4m"));
    QFile fixture(path);
    if (!fixture.open(QIODevice::WriteOnly | QIODevice::Truncate)) return {};

    if (fixture.write("YUV4MPEG2 W2 H2 F25:1 Ip A1:1 C444\n") < 0) return {};
    const QByteArray redFrame("RRRRZZZZ\xF0\xF0\xF0\xF0", 12);
    for (int frame = 0; frame < 150; ++frame) {
        if (fixture.write("FRAME\n") < 0 || fixture.write(redFrame) != redFrame.size()) return {};
    }
    fixture.close();
    return path;
}
} // namespace

class MpvQuickItemTest final : public QObject {
    Q_OBJECT

private slots:
    void suspendsRenderingWithoutChangingPlaybackControls();
    void rendersOneLocalFrameAndReleasesCleanly();
    void doesNotRestoreMediaStateAfterReleaseDuringLoad();
    void rendersLocalFramePixelsIntoTheQuickFramebuffer();
    void keepsRenderingAfterItemResize();
    void ignoresRetiredLocalLoadFailureAfterSourceSwitch();
    void destroysMpvCoreAfterRenderContextRelease();
    void doesNotPlaceRemoteAddressInFailureText();
    void appliesValidatedVolume();
    void usesWakeupDrivenEventDraining();
};

void MpvQuickItemTest::suspendsRenderingWithoutChangingPlaybackControls()
{
    MpvQuickItem item;
    QVERIFY(!item.renderingSuspended());
    QVERIFY(item.setMuted(true));
    QVERIFY(item.setVolume(37));
    item.suspendRendering();
    QVERIFY(item.renderingSuspended());
    QCOMPARE(item.volume(), 37);
    QVERIFY(item.isMuted());
    item.suspendRendering();
    QVERIFY(item.renderingSuspended());
    item.resumeRendering();
    QVERIFY(!item.renderingSuspended());
    item.resumeRendering();
    QVERIFY(!item.renderingSuspended());
}

void MpvQuickItemTest::rendersOneLocalFrameAndReleasesCleanly()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fixture = makePpmFixture(directory);
    QVERIFY(!fixture.isEmpty());

    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QQuickWindow window;
    window.resize(320, 240);
    auto *item = new MpvQuickItem(window.contentItem());
    item->setWidth(1);
    item->setHeight(1);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QTRY_VERIFY_WITH_TIMEOUT(item->isRenderContextReady(), 10000);
    QVERIFY(item->loadLocalMedia(fixture));
    QTRY_VERIFY_WITH_TIMEOUT(item->isFirstFrameRendered(), 10000);
    item->release();
    QCOMPARE(item->playbackState(), MpvQuickItem::PlaybackState::Idle);
    QVERIFY(!item->isMediaLoaded());
    QVERIFY(!item->isFirstFrameRendered());
}

void MpvQuickItemTest::doesNotRestoreMediaStateAfterReleaseDuringLoad()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fixture = makeY4mFixture(directory);
    QVERIFY(!fixture.isEmpty());

    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QQuickWindow window;
    window.resize(320, 240);
    auto *item = new MpvQuickItem(window.contentItem());
    item->setWidth(320);
    item->setHeight(240);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTRY_VERIFY_WITH_TIMEOUT(item->isRenderContextReady(), 10000);

    QVERIFY(item->loadLocalMedia(fixture));
    QThread::msleep(250);
    item->release();
    QTest::qWait(500);

    QCOMPARE(item->playbackState(), MpvQuickItem::PlaybackState::Idle);
    QVERIFY(!item->isMediaLoaded());
    QVERIFY(!item->isFirstFrameRendered());
}

void MpvQuickItemTest::rendersLocalFramePixelsIntoTheQuickFramebuffer()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fixture = makeY4mFixture(directory);
    QVERIFY(!fixture.isEmpty());

    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QQuickWindow window;
    window.resize(320, 240);
    auto *item = new MpvQuickItem(window.contentItem());
    item->setWidth(320);
    item->setHeight(240);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QTRY_VERIFY_WITH_TIMEOUT(item->isRenderContextReady(), 10000);
    QVERIFY(item->loadLocalMedia(fixture));
    QTRY_VERIFY_WITH_TIMEOUT(item->isFirstFrameRendered(), 10000);

    QTRY_VERIFY_WITH_TIMEOUT([&window] {
        window.update();
        const QImage frame = window.grabWindow();
        if (frame.isNull()) return false;
        const QColor centerPixel = frame.pixelColor(frame.width() / 2, frame.height() / 2);
        return centerPixel.red() > 200 && centerPixel.green() < 80 && centerPixel.blue() < 80;
    }(), 5000);
}

void MpvQuickItemTest::keepsRenderingAfterItemResize()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fixture = makeY4mFixture(directory);
    QVERIFY(!fixture.isEmpty());

    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QQuickWindow window;
    window.resize(320, 240);
    auto *item = new MpvQuickItem(window.contentItem());
    item->setWidth(320);
    item->setHeight(240);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QTRY_VERIFY_WITH_TIMEOUT(item->isRenderContextReady(), 10000);
    QVERIFY(item->loadLocalMedia(fixture));
    QTRY_VERIFY_WITH_TIMEOUT(item->isFirstFrameRendered(), 10000);

    item->setWidth(160);
    item->setHeight(120);
    item->setX(80);
    item->setY(60);
    QTest::qWait(100);
    item->setWidth(0);
    item->setHeight(0);
    QTest::qWait(100);
    item->setWidth(320);
    item->setHeight(240);
    item->setX(0);
    item->setY(0);

    QTRY_VERIFY_WITH_TIMEOUT([&window] {
        window.update();
        const QImage frame = window.grabWindow();
        if (frame.isNull()) return false;
        const QColor centerPixel = frame.pixelColor(frame.width() / 2, frame.height() / 2);
        return centerPixel.red() > 200 && centerPixel.green() < 80 && centerPixel.blue() < 80;
    }(), 5000);
}

void MpvQuickItemTest::ignoresRetiredLocalLoadFailureAfterSourceSwitch()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString validFixture = makeY4mFixture(directory);
    QVERIFY(!validFixture.isEmpty());

    const QString invalidFixture = directory.filePath(QStringLiteral("broken.y4m"));
    QFile brokenFixture(invalidFixture);
    QVERIFY(brokenFixture.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QVERIFY(brokenFixture.write("not a valid local media fixture\n") > 0);
    brokenFixture.close();

    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QQuickWindow window;
    window.resize(320, 240);
    auto *item = new MpvQuickItem(window.contentItem());
    item->setWidth(320);
    item->setHeight(240);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTRY_VERIFY_WITH_TIMEOUT(item->isRenderContextReady(), 10000);

    QSignalSpy failures(item, &MpvQuickItem::playbackFailed);
    QVERIFY(failures.isValid());
    QVERIFY(item->loadLocalMedia(invalidFixture));
    QVERIFY(item->loadLocalMedia(validFixture));
    QTRY_VERIFY_WITH_TIMEOUT(item->isFirstFrameRendered(), 10000);
    QTest::qWait(250);

    QVERIFY(item->playbackState() != MpvQuickItem::PlaybackState::Error);
    QCOMPARE(failures.count(), 0);
}

void MpvQuickItemTest::destroysMpvCoreAfterRenderContextRelease()
{
    MpvQuickItem::resetTeardownObservationForTest();
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    auto window = std::make_unique<QQuickWindow>();
    window->resize(320, 240);
    auto *item = new MpvQuickItem(window->contentItem());
    item->setWidth(320);
    item->setHeight(240);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.get()));
    QTRY_VERIFY_WITH_TIMEOUT(item->isRenderContextReady(), 10000);

    delete item;
    window->close();
    window.reset();

    QTRY_VERIFY_WITH_TIMEOUT(
        MpvQuickItem::wasLastCoreTeardownAfterRenderContextReleaseForTest(), 10000);
}

void MpvQuickItemTest::doesNotPlaceRemoteAddressInFailureText()
{
    MpvQuickItem item;
    QVERIFY(!item.safeErrorLabel().contains(QStringLiteral("://")));
    QVERIFY(!item.safeErrorLabel().contains(QStringLiteral("token"), Qt::CaseInsensitive));
}

void MpvQuickItemTest::appliesValidatedVolume()
{
    MpvQuickItem item;

    QCOMPARE(item.volume(), 100);
    QVERIFY(item.setVolume(37));
    QCOMPARE(item.volume(), 37);
    QVERIFY(!item.setVolume(-1));
    QCOMPARE(item.volume(), 37);
    QVERIFY(!item.setVolume(101));
    QCOMPARE(item.volume(), 37);
}

void MpvQuickItemTest::usesWakeupDrivenEventDraining()
{
    MpvQuickItem item;

    QVERIFY(item.usesWakeupCallbackForTest());
    QVERIFY(!item.usesTimerPollingForTest());
    QTRY_COMPARE_WITH_TIMEOUT(item.pendingEventDrainCountForTest(), 0, 1000);
}

QTEST_MAIN(MpvQuickItemTest)

#include "mpv_quick_item_test.moc"
