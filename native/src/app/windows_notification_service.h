#pragma once

#include <QObject>
#include <QString>

#include <memory>

#include "workspace/notification_policy.h"

class QSettings;

struct NotificationPreferences {
    bool enabled = true;
    bool roomOnline = true;
    bool roomOffline = true;
    bool playbackFailed = true;
    bool playbackRecovered = true;
};

class SystemNotificationSink {
public:
    virtual ~SystemNotificationSink() = default;
    virtual bool available() const = 0;
    virtual void show(const QString &title, const QString &body) = 0;
};

class WindowsNotificationService final : public QObject {
    Q_OBJECT

public:
    explicit WindowsNotificationService(QSettings *settings,
                                        SystemNotificationSink *sink = nullptr,
                                        QObject *parent = nullptr);
    ~WindowsNotificationService() override;

    NotificationPreferences preferences() const noexcept;
    bool setPreferences(NotificationPreferences preferences);
    bool deliver(const NotificationEvent &event);
    QString statusText() const;

private:
    void loadPreferences();
    bool isEnabled(NotificationEventType type) const noexcept;

    QSettings *settings_ = nullptr;
    SystemNotificationSink *sink_ = nullptr;
    std::unique_ptr<SystemNotificationSink> ownedSink_;
    NotificationPreferences preferences_;
    QString statusText_;
};
