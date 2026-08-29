#pragma once

#include <QObject>

class MonitoringModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int onlineCount READ onlineCount NOTIFY countsChanged)
    Q_PROPERTY(int offlineCount READ offlineCount NOTIFY countsChanged)
    Q_PROPERTY(int errorCount READ errorCount NOTIFY countsChanged)
    Q_PROPERTY(QString notificationStatus READ notificationStatus NOTIFY statusChanged)
    Q_PROPERTY(QString recoveryStatus READ recoveryStatus NOTIFY statusChanged)

public:
    enum class NotificationStatus {
        Disabled,
        Enabled,
        Unavailable,
    };
    Q_ENUM(NotificationStatus)

    enum class RecoveryStatus {
        Healthy,
        Recovering,
        Error,
    };
    Q_ENUM(RecoveryStatus)

    explicit MonitoringModel(QObject *parent = nullptr);

    int onlineCount() const noexcept;
    int offlineCount() const noexcept;
    int errorCount() const noexcept;
    QString notificationStatus() const;
    QString recoveryStatus() const;

    void setCounts(int online, int offline, int errors);
    void setStatus(NotificationStatus notification, RecoveryStatus recovery);

signals:
    void countsChanged();
    void statusChanged();

private:
    int onlineCount_ = 0;
    int offlineCount_ = 0;
    int errorCount_ = 0;
    NotificationStatus notificationStatus_ = NotificationStatus::Disabled;
    RecoveryStatus recoveryStatus_ = RecoveryStatus::Healthy;
};
