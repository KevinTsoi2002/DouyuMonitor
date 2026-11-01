#include "app/windows_notification_service.h"

#include <QDateTime>
#include <QSet>
#include <QSettings>
#include <QStringList>

namespace {

QString key(NotificationEventType type)
{
    switch (type) {
    case NotificationEventType::RoomOnline:
        return QStringLiteral("roomOnline");
    case NotificationEventType::RoomOffline:
        return QStringLiteral("roomOffline");
    case NotificationEventType::PlaybackFailed:
        return QStringLiteral("playbackFailed");
    case NotificationEventType::PlaybackRecovered:
        return QStringLiteral("playbackRecovered");
    case NotificationEventType::FavoriteTitleChanged:
        return QStringLiteral("favoriteTitleChanged");
    }
    return {};
}

} // namespace

WindowsNotificationService::WindowsNotificationService(QSettings *settings,
                                                       SystemNotificationSink *sink,
                                                       QObject *parent)
    : QObject(parent)
    , settings_(settings)
    , sink_(sink)
{
    loadPreferences();
}

WindowsNotificationService::~WindowsNotificationService() = default;

NotificationPreferences WindowsNotificationService::preferences() const noexcept
{
    return preferences_;
}

bool WindowsNotificationService::available() const noexcept
{
    return sink_ != nullptr && sink_->available();
}

bool WindowsNotificationService::setPreferences(NotificationPreferences preferences)
{
    if (settings_ == nullptr) {
        preferences_ = preferences;
        statusText_.clear();
        return true;
    }

    const NotificationPreferences previous = preferences_;
    const QStringList keys = {
        QStringLiteral("notifications/enabled"),
        QStringLiteral("notifications/roomOnline"),
        QStringLiteral("notifications/roomOffline"),
        QStringLiteral("notifications/playbackFailed"),
        QStringLiteral("notifications/playbackRecovered"),
        QStringLiteral("notifications/favoriteTitleChanged"),
    };
    QHash<QString, QVariant> previousValues;
    QSet<QString> previousKeys;
    for (const QString &key : keys) {
        if (settings_->contains(key)) {
            previousKeys.insert(key);
            previousValues.insert(key, settings_->value(key));
        }
    }

    settings_->setValue(keys.at(0), preferences.enabled);
    settings_->setValue(keys.at(1), preferences.roomOnline);
    settings_->setValue(keys.at(2), preferences.roomOffline);
    settings_->setValue(keys.at(3), preferences.playbackFailed);
    settings_->setValue(keys.at(4), preferences.playbackRecovered);
    settings_->setValue(keys.at(5), preferences.favoriteTitleChanged);
    settings_->sync();
    if (settings_->status() != QSettings::NoError) {
        for (const QString &key : keys) {
            if (previousKeys.contains(key)) settings_->setValue(key, previousValues.value(key));
            else settings_->remove(key);
        }
        preferences_ = previous;
        statusText_ = QStringLiteral("通知设置保存失败");
        return false;
    }
    preferences_ = preferences;
    statusText_.clear();
    return true;
}

bool WindowsNotificationService::deliver(const NotificationEvent &event)
{
    if (!preferences_.enabled || !isEnabled(event.type) || sink_ == nullptr
        || !sink_->available()) {
        return false;
    }

    constexpr qint64 dedupeWindowMs = 5 * 60 * 1000;
    constexpr qint64 rateWindowMs = 60 * 1000;
    constexpr int maxEventsPerRateWindow = 6;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto it = deliveredEventsMs_.begin(); it != deliveredEventsMs_.end();) {
        if (it.value() <= now - dedupeWindowMs) it = deliveredEventsMs_.erase(it);
        else ++it;
    }
    while (!deliveredAtMs_.isEmpty() && deliveredAtMs_.front() <= now - rateWindowMs) {
        deliveredAtMs_.dequeue();
    }
    if (deliveredAtMs_.size() >= maxEventsPerRateWindow) return false;
    const QString key = eventKey(event);
    const auto previous = deliveredEventsMs_.constFind(key);
    if (previous != deliveredEventsMs_.cend() && now - previous.value() < dedupeWindowMs) {
        return false;
    }
    if (!sink_->show(event.title, event.body)) return false;
    deliveredEventsMs_.insert(key, now);
    deliveredAtMs_.enqueue(now);
    return true;
}

QString WindowsNotificationService::eventKey(const NotificationEvent &event)
{
    return event.roomId + QLatin1Char(':')
        + QString::number(static_cast<int>(event.type));
}

QString WindowsNotificationService::statusText() const
{
    return statusText_;
}

void WindowsNotificationService::loadPreferences()
{
    if (settings_ == nullptr) return;
    preferences_.enabled =
        settings_->value(QStringLiteral("notifications/enabled"), true).toBool();
    preferences_.roomOnline =
        settings_->value(QStringLiteral("notifications/roomOnline"), true).toBool();
    preferences_.roomOffline =
        settings_->value(QStringLiteral("notifications/roomOffline"), true).toBool();
    preferences_.playbackFailed =
        settings_->value(QStringLiteral("notifications/playbackFailed"), true).toBool();
    preferences_.playbackRecovered =
        settings_->value(QStringLiteral("notifications/playbackRecovered"), true).toBool();
    preferences_.favoriteTitleChanged =
        settings_->value(QStringLiteral("notifications/favoriteTitleChanged"), true).toBool();
}

bool WindowsNotificationService::isEnabled(NotificationEventType type) const noexcept
{
    switch (type) {
    case NotificationEventType::RoomOnline:
        return preferences_.roomOnline;
    case NotificationEventType::RoomOffline:
        return preferences_.roomOffline;
    case NotificationEventType::PlaybackFailed:
        return preferences_.playbackFailed;
    case NotificationEventType::PlaybackRecovered:
        return preferences_.playbackRecovered;
    case NotificationEventType::FavoriteTitleChanged:
        return preferences_.favoriteTitleChanged;
    }
    return false;
}
