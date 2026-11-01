#include <QSettings>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "app/windows_notification_service.h"

class FakeNotificationSink final : public SystemNotificationSink {
public:
    bool available() const override { return available_; }
    bool show(const QString &title, const QString &body) override
    {
        titles.push_back(title);
        bodies.push_back(body);
        return showResult_;
    }

    bool available_ = true;
    bool showResult_ = true;
    QStringList titles;
    QStringList bodies;
};

class WindowsNotificationServiceTest final : public QObject {
    Q_OBJECT

private slots:
    void filtersEventsByPreferences();
    void persistsPreferences();
    void rollsBackPreferencesWhenSyncFails();
    void failedUpdateDoesNotAffectFilteringOrNextUpdate();
    void filtersFavoriteTitleChanges();
    void suppressesDuplicateEventsAcrossSources();
    void limitsEventsAcrossSources();
    void reportsSingleDeliveryFailureWithoutDisablingBackend();
};

void WindowsNotificationServiceTest::filtersEventsByPreferences()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("notifications.ini")),
                       QSettings::IniFormat);
    FakeNotificationSink sink;
    WindowsNotificationService service(&settings, &sink);

    NotificationEvent online;
    online.type = NotificationEventType::RoomOnline;
    online.title = QStringLiteral("房间");
    online.body = QStringLiteral("已开播");
    QVERIFY(service.deliver(online));
    QCOMPARE(sink.bodies.size(), 1);
    QCOMPARE(sink.titles.front(), QStringLiteral("房间"));
    QCOMPARE(sink.bodies.front(), QStringLiteral("已开播"));

    auto preferences = service.preferences();
    preferences.roomOnline = false;
    QVERIFY(service.setPreferences(preferences));
    QVERIFY(!service.deliver(online));
    QCOMPARE(sink.bodies.size(), 1);

    sink.available_ = false;
    preferences.roomOnline = true;
    QVERIFY(service.setPreferences(preferences));
    QVERIFY(!service.deliver(online));
}

void WindowsNotificationServiceTest::persistsPreferences()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("notifications.ini")),
                       QSettings::IniFormat);
    FakeNotificationSink sink;
    WindowsNotificationService service(&settings, &sink);

    auto preferences = service.preferences();
    preferences.enabled = false;
    preferences.roomOffline = false;
    QVERIFY(service.setPreferences(preferences));

    WindowsNotificationService restored(&settings, &sink);
    QCOMPARE(restored.preferences().enabled, false);
    QCOMPARE(restored.preferences().roomOffline, false);
}

void WindowsNotificationServiceTest::rollsBackPreferencesWhenSyncFails()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString settingsPath = directory.filePath(QStringLiteral("notifications.ini"));
    const QString backupPath = directory.filePath(QStringLiteral("notifications.saved"));
    {
        QSettings initial(settingsPath, QSettings::IniFormat);
        initial.setValue(QStringLiteral("notifications/enabled"), true);
        initial.setValue(QStringLiteral("notifications/roomOnline"), true);
        initial.sync();
        QCOMPARE(initial.status(), QSettings::NoError);
    }

    QSettings settings(settingsPath, QSettings::IniFormat);
    FakeNotificationSink sink;
    WindowsNotificationService service(&settings, &sink);
    const NotificationPreferences previous = service.preferences();
    QVERIFY(previous.enabled);
    QVERIFY(previous.roomOnline);

    QVERIFY(QFile::rename(settingsPath, backupPath));
    QVERIFY(QDir().mkdir(settingsPath));

    NotificationPreferences next = previous;
    next.enabled = false;
    next.roomOnline = false;
    QVERIFY(!service.setPreferences(next));
    QCOMPARE(service.preferences().enabled, previous.enabled);
    QCOMPARE(service.preferences().roomOnline, previous.roomOnline);
    QCOMPARE(settings.value(QStringLiteral("notifications/enabled")).toBool(), true);
    QCOMPARE(settings.value(QStringLiteral("notifications/roomOnline")).toBool(), true);

    QSettings restored(backupPath, QSettings::IniFormat);
    QCOMPARE(restored.value(QStringLiteral("notifications/enabled")).toBool(), true);
    QCOMPARE(restored.value(QStringLiteral("notifications/roomOnline")).toBool(), true);

    QVERIFY(QDir(settingsPath).removeRecursively());
    QVERIFY(QFile::rename(backupPath, settingsPath));
    settings.sync();
    QSettings afterRecovery(settingsPath, QSettings::IniFormat);
    QCOMPARE(afterRecovery.value(QStringLiteral("notifications/enabled")).toBool(), true);
    QCOMPARE(afterRecovery.value(QStringLiteral("notifications/roomOnline")).toBool(), true);
}

void WindowsNotificationServiceTest::failedUpdateDoesNotAffectFilteringOrNextUpdate()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString settingsPath = directory.filePath(QStringLiteral("notifications.ini"));
    const QString backupPath = directory.filePath(QStringLiteral("notifications.saved"));
    QSettings initial(settingsPath, QSettings::IniFormat);
    initial.setValue(QStringLiteral("notifications/roomOnline"), true);
    initial.sync();
    QCOMPARE(initial.status(), QSettings::NoError);

    QSettings settings(settingsPath, QSettings::IniFormat);
    FakeNotificationSink sink;
    WindowsNotificationService service(&settings, &sink);
    NotificationEvent event;
    event.type = NotificationEventType::RoomOnline;
    event.roomId = QStringLiteral("63136");
    event.title = QStringLiteral("房间");
    event.body = QStringLiteral("已开播");

    QVERIFY(QFile::rename(settingsPath, backupPath));
    QVERIFY(QDir().mkdir(settingsPath));
    NotificationPreferences failed = service.preferences();
    failed.roomOnline = false;
    QVERIFY(!service.setPreferences(failed));
    QVERIFY(service.deliver(event));
    QCOMPARE(sink.bodies.size(), 1);

    QVERIFY(QDir(settingsPath).removeRecursively());
    QVERIFY(QFile::rename(backupPath, settingsPath));
    QSettings recoveredSettings(settingsPath, QSettings::IniFormat);
    WindowsNotificationService recovered(&recoveredSettings, &sink);
    NotificationPreferences committed = recovered.preferences();
    committed.roomOnline = false;
    QVERIFY(recovered.setPreferences(committed));
    QVERIFY(!recovered.deliver(event));

    QSettings restored(settingsPath, QSettings::IniFormat);
    QCOMPARE(restored.value(QStringLiteral("notifications/roomOnline")).toBool(), false);
}

void WindowsNotificationServiceTest::filtersFavoriteTitleChanges()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("notifications.ini")),
                       QSettings::IniFormat);
    FakeNotificationSink sink;
    WindowsNotificationService service(&settings, &sink);

    NotificationEvent event;
    event.type = NotificationEventType::FavoriteTitleChanged;
    event.title = QStringLiteral("主播");
    event.body = QStringLiteral("标题已更新");
    QVERIFY(service.deliver(event));
    QCOMPARE(sink.bodies.size(), 1);

    auto preferences = service.preferences();
    preferences.favoriteTitleChanged = false;
    QVERIFY(service.setPreferences(preferences));
    QVERIFY(!service.deliver(event));
    QCOMPARE(sink.bodies.size(), 1);
}

void WindowsNotificationServiceTest::suppressesDuplicateEventsAcrossSources()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("notifications.ini")),
                       QSettings::IniFormat);
    FakeNotificationSink sink;
    WindowsNotificationService service(&settings, &sink);

    NotificationEvent event;
    event.type = NotificationEventType::RoomOnline;
    event.roomId = QStringLiteral("63136");
    event.title = QStringLiteral("主播");
    event.body = QStringLiteral("主播 已开播");

    QVERIFY(service.deliver(event));
    QVERIFY(!service.deliver(event));
    QCOMPARE(sink.bodies.size(), 1);
}

void WindowsNotificationServiceTest::limitsEventsAcrossSources()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("notifications.ini")),
                       QSettings::IniFormat);
    FakeNotificationSink sink;
    WindowsNotificationService service(&settings, &sink);

    for (int index = 0; index < 6; ++index) {
        NotificationEvent event;
        event.type = NotificationEventType::RoomOnline;
        event.roomId = QString::number(63136 + index);
        event.title = QStringLiteral("主播");
        event.body = QStringLiteral("主播 已开播");
        QVERIFY(service.deliver(event));
    }

    NotificationEvent seventh;
    seventh.type = NotificationEventType::RoomOnline;
    seventh.roomId = QStringLiteral("999999");
    seventh.title = QStringLiteral("主播");
    seventh.body = QStringLiteral("主播 已开播");
    QVERIFY(!service.deliver(seventh));
    QCOMPARE(sink.bodies.size(), 6);
}

void WindowsNotificationServiceTest::reportsSingleDeliveryFailureWithoutDisablingBackend()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("notifications.ini")),
                       QSettings::IniFormat);
    FakeNotificationSink sink;
    WindowsNotificationService service(&settings, &sink);

    NotificationEvent event;
    event.type = NotificationEventType::RoomOnline;
    event.roomId = QStringLiteral("63136");
    event.title = QStringLiteral("主播");
    event.body = QStringLiteral("已开播");

    sink.showResult_ = false;
    QVERIFY(!service.deliver(event));
    QVERIFY(service.available());
    QCOMPARE(sink.bodies.size(), 1);

    sink.showResult_ = true;
    QVERIFY(service.deliver(event));
    QVERIFY(service.available());
    QCOMPARE(sink.bodies.size(), 2);
}

QTEST_GUILESS_MAIN(WindowsNotificationServiceTest)

#include "windows_notification_service_test.moc"
