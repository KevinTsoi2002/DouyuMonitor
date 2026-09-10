#include <QSignalSpy>
#include <QtTest/QtTest>

#include "app/windows_tray_service.h"

class WindowsTrayServiceTest final : public QObject {
    Q_OBJECT

private slots:
    void startsAndStopsIdempotently();
    void forwardsTestActions();
};

void WindowsTrayServiceTest::startsAndStopsIdempotently()
{
    WindowsTrayService service;
    QVERIFY(!service.isRunning());
    service.stop();
    QVERIFY(!service.isRunning());

#ifdef Q_OS_WIN
    QSKIP("Tray integration requires an interactive Windows shell; action hooks are tested separately.");
#else
    QVERIFY(!service.start(nullptr));
    QVERIFY(!service.isRunning());
#endif
}

void WindowsTrayServiceTest::forwardsTestActions()
{
    WindowsTrayService service;
    QSignalSpy showSpy(&service, &WindowsTrayService::showRequested);
    QSignalSpy quitSpy(&service, &WindowsTrayService::quitRequested);

#ifdef DOUYU_TESTING
    service.triggerShowForTest();
    service.triggerQuitForTest();
    QCOMPARE(showSpy.count(), 1);
    QCOMPARE(quitSpy.count(), 1);
#else
    QSKIP("Test hooks are disabled in this build.");
#endif
}

QTEST_GUILESS_MAIN(WindowsTrayServiceTest)

#include "windows_tray_service_test.moc"
