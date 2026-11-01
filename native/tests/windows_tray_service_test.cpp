#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <memory>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#endif

#include "app/windows_notification_service.h"
#include "app/windows_tray_service.h"

namespace {

struct ShellCall {
    WindowsTrayShellApi::IconIdentity identity;
    WindowsTrayShellApi::NotificationSpec notification;
};

class RecordingShellApi final : public WindowsTrayShellApi {
public:
    bool add(const IconSpec &icon) override
    {
        added.push_back(icon);
        return addResult;
    }

    bool modify(const IconIdentity &identity,
                const NotificationSpec &notification) override
    {
        modified.push_back({identity, notification});
        return modifyResult;
    }

    bool remove(const IconIdentity &identity) override
    {
        removed.push_back(identity);
        return removeResult;
    }

    bool addResult = true;
    bool modifyResult = true;
    bool removeResult = true;
    QVector<IconSpec> added;
    QVector<ShellCall> modified;
    QVector<IconIdentity> removed;
};

} // namespace

class WindowsTrayServiceTest final : public QObject {
    Q_OBJECT

private slots:
    void startsAndStopsIdempotently();
    void forwardsTestActions();
    void registersOneIconAndSharesIdentityWithNotifications();
    void registrationFailureLeavesNotificationsUnavailable();
    void notificationFailureDoesNotDisableTray();
};

void WindowsTrayServiceTest::startsAndStopsIdempotently()
{
    WindowsTrayService service;
    QVERIFY(!service.isRunning());
    service.stop();
    QVERIFY(!service.isRunning());

#ifdef Q_OS_WIN
    auto shell = std::make_unique<RecordingShellApi>();
    auto *recording = shell.get();
    WindowsTrayService isolated(nullptr, std::move(shell));
    QVERIFY(isolated.start(nullptr));
    QVERIFY(isolated.start(nullptr));
    QCOMPARE(recording->added.size(), 1);
    isolated.stop();
    isolated.stop();
    QCOMPARE(recording->removed.size(), 1);
    QVERIFY(!isolated.isRunning());
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

void WindowsTrayServiceTest::registersOneIconAndSharesIdentityWithNotifications()
{
#ifndef Q_OS_WIN
    QSKIP("Windows Shell identity test is Windows-only.");
#else
    auto shell = std::make_unique<RecordingShellApi>();
    auto *recording = shell.get();
    WindowsTrayService tray(nullptr, std::move(shell));
    QVERIFY(tray.notificationSink() != nullptr);
    QVERIFY(tray.start(nullptr));
    QCOMPARE(recording->added.size(), 1);
    QVERIFY(recording->added.front().identity.window != 0);
    QCOMPARE(recording->added.front().identity.id, quint32(1));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("notifications.ini")),
                       QSettings::IniFormat);
    {
        WindowsNotificationService notifications(&settings, tray.notificationSink());
        QVERIFY(notifications.available());

        NotificationEvent event;
        event.type = NotificationEventType::RoomOnline;
        event.roomId = QStringLiteral("63136");
        event.title = QStringLiteral("房间");
        event.body = QStringLiteral("已开播");
        QVERIFY(notifications.deliver(event));
    }

    QCOMPARE(recording->added.size(), 1);
    QCOMPARE(recording->modified.size(), 1);
    QCOMPARE(recording->modified.front().identity.window,
             recording->added.front().identity.window);
    QCOMPARE(recording->modified.front().identity.id,
             recording->added.front().identity.id);
    QCOMPARE(recording->modified.front().notification.title, QStringLiteral("房间"));
    QCOMPARE(recording->modified.front().notification.body, QStringLiteral("已开播"));
    QCOMPARE(recording->modified.front().notification.flags, quint32(NIF_INFO));
    QCOMPARE(recording->modified.front().notification.infoFlags, quint32(NIIF_INFO));
    QCOMPARE(recording->modified.front().notification.timeoutMs, quint32(5000));
    QCOMPARE(recording->removed.size(), 0);

    tray.stop();
    QCOMPARE(recording->removed.size(), 1);
    QCOMPARE(recording->removed.front().window,
             recording->added.front().identity.window);
    QCOMPARE(recording->removed.front().id,
             recording->added.front().identity.id);
#endif
}

void WindowsTrayServiceTest::registrationFailureLeavesNotificationsUnavailable()
{
#ifndef Q_OS_WIN
    QSKIP("Windows Shell registration test is Windows-only.");
#else
    auto shell = std::make_unique<RecordingShellApi>();
    auto *recording = shell.get();
    recording->addResult = false;
    WindowsTrayService tray(nullptr, std::move(shell));
    QVERIFY(!tray.start(nullptr));
    QVERIFY(!tray.isRunning());
    QVERIFY(tray.notificationSink() != nullptr);
    QVERIFY(!tray.notificationSink()->available());

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("notifications.ini")),
                       QSettings::IniFormat);
    WindowsNotificationService notifications(&settings, tray.notificationSink());
    NotificationEvent event;
    event.type = NotificationEventType::RoomOnline;
    event.roomId = QStringLiteral("63136");
    QVERIFY(!notifications.deliver(event));
    QCOMPARE(recording->modified.size(), 0);
    QCOMPARE(recording->removed.size(), 0);
#endif
}

void WindowsTrayServiceTest::notificationFailureDoesNotDisableTray()
{
#ifndef Q_OS_WIN
    QSKIP("Windows Shell notification test is Windows-only.");
#else
    auto shell = std::make_unique<RecordingShellApi>();
    auto *recording = shell.get();
    recording->modifyResult = false;
    WindowsTrayService tray(nullptr, std::move(shell));
    QVERIFY(tray.start(nullptr));

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("notifications.ini")),
                       QSettings::IniFormat);
    WindowsNotificationService notifications(&settings, tray.notificationSink());
    NotificationEvent event;
    event.type = NotificationEventType::RoomOnline;
    event.roomId = QStringLiteral("63136");
    event.title = QStringLiteral("房间");
    event.body = QStringLiteral("第一次提交失败");

    QVERIFY(!notifications.deliver(event));
    QVERIFY(notifications.available());
    QCOMPARE(recording->modified.size(), 1);

    recording->modifyResult = true;
    event.body = QStringLiteral("第二次提交成功");
    QVERIFY(notifications.deliver(event));
    QVERIFY(notifications.available());
    QCOMPARE(recording->modified.size(), 2);

    tray.stop();
    QCOMPARE(recording->removed.size(), 1);
#endif
}

QTEST_GUILESS_MAIN(WindowsTrayServiceTest)

#include "windows_tray_service_test.moc"
