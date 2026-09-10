#pragma once

#include <QObject>

class QWindow;

class WindowsTrayService final : public QObject {
    Q_OBJECT

public:
    explicit WindowsTrayService(QObject *parent = nullptr);
    ~WindowsTrayService() override;

    bool start(QWindow *window);
    void stop();
    bool isRunning() const noexcept;

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
    Private *d_ = nullptr;
};
