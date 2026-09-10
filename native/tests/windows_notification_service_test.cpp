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
    void filtersFavoriteTitleChanges();
    void suppressesDuplicateEventsAcrossSources();
    void limitsEventsAcrossSources();
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

QTEST_GUILESS_MAIN(WindowsNotificationServiceTest)

#include "windows_notification_service_test.moc"
