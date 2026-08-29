#include "ui/monitoring_model.h"

#include <algorithm>

MonitoringModel::MonitoringModel(QObject *parent)
    : QObject(parent)
{
}

int MonitoringModel::onlineCount() const noexcept
{
    return onlineCount_;
}

int MonitoringModel::offlineCount() const noexcept
{
    return offlineCount_;
}

int MonitoringModel::errorCount() const noexcept
{
    return errorCount_;
}

QString MonitoringModel::notificationStatus() const
{
    switch (notificationStatus_) {
    case NotificationStatus::Disabled:
        return QStringLiteral("disabled");
    case NotificationStatus::Enabled:
        return QStringLiteral("enabled");
    case NotificationStatus::Unavailable:
        return QStringLiteral("unavailable");
    }
    return QStringLiteral("unavailable");
}

QString MonitoringModel::recoveryStatus() const
{
    switch (recoveryStatus_) {
    case RecoveryStatus::Healthy:
        return QStringLiteral("healthy");
    case RecoveryStatus::Recovering:
        return QStringLiteral("recovering");
    case RecoveryStatus::Error:
        return QStringLiteral("error");
    }
    return QStringLiteral("error");
}

void MonitoringModel::setCounts(int online, int offline, int errors)
{
    online = std::max(online, 0);
    offline = std::max(offline, 0);
    errors = std::max(errors, 0);
    if (onlineCount_ == online && offlineCount_ == offline && errorCount_ == errors) return;
    onlineCount_ = online;
    offlineCount_ = offline;
    errorCount_ = errors;
    emit countsChanged();
}

void MonitoringModel::setStatus(NotificationStatus notification, RecoveryStatus recovery)
{
    if (notificationStatus_ == notification && recoveryStatus_ == recovery) return;
    notificationStatus_ = notification;
    recoveryStatus_ = recovery;
    emit statusChanged();
}
