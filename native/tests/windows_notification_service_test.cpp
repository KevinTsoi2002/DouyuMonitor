#include <QSettings>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "app/windows_notification_service.h"

class FakeNotificationSink final : public SystemNotificationSink {
public:
    bool available() const override { return available_; }
    void show(const QString &title, const QString &body) override
    {
        titles.push_back(title);
        bodies.push_back(body);
    }

    bool available_ = true;
    QStringList titles;
    QStringList bodies;
};

class WindowsNotificationServiceTest final : public QObject {
    Q_OBJECT

private slots:
    void filtersEventsByPreferences();
    void persistsPreferences();
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

QTEST_GUILESS_MAIN(WindowsNotificationServiceTest)

#include "windows_notification_service_test.moc"
