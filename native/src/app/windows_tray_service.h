#pragma once

#include <QObject>
#include <QString>

#include <memory>

#include <QtGlobal>

class QWindow;
class SystemNotificationSink;

class WindowsTrayShellApi {
public:
    struct IconIdentity {
        quintptr window = 0;
        quint32 id = 0;
    };

    struct IconSpec {
        IconIdentity identity;
        quint32 callbackMessage = 0;
        quintptr iconHandle = 0;
        QString tooltip;
    };

    struct NotificationSpec {
        QString title;
        QString body;
        quint32 flags = 0;
        quint32 infoFlags = 0;
        quint32 timeoutMs = 0;
    };

    virtual ~WindowsTrayShellApi() = default;
    virtual bool add(const IconSpec &icon) = 0;
    virtual bool modify(const IconIdentity &identity,
                        const NotificationSpec &notification) = 0;
    virtual bool remove(const IconIdentity &identity) = 0;
};

class WindowsTrayService final : public QObject {
    Q_OBJECT

public:
    explicit WindowsTrayService(QObject *parent = nullptr,
                                std::unique_ptr<WindowsTrayShellApi> shellApi = {});
    ~WindowsTrayService() override;

    bool start(QWindow *window);
    void stop();
    bool isRunning() const noexcept;
    SystemNotificationSink *notificationSink() noexcept;

#ifdef DOUYU_TESTING
    void triggerShowForTest();
    void triggerQuitForTest();
#endif

signals:
    void showRequested();
    void quitRequested();

public:
    class Private;

private:
    bool showNotification(const QString &title, const QString &body);

    Private *d_ = nullptr;
};
