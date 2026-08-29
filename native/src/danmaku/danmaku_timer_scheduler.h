#pragma once

#include <functional>

#include <QHash>
#include <QObject>

class DanmakuTimerScheduler {
public:
    using TimerId = quint64;
    using Callback = std::function<void()>;

    virtual ~DanmakuTimerScheduler() = default;
    virtual TimerId once(int delayMs, Callback callback) = 0;
    virtual TimerId repeating(int intervalMs, Callback callback) = 0;
    virtual void cancel(TimerId id) = 0;
};

class QtDanmakuTimerScheduler final : public QObject, public DanmakuTimerScheduler {
    Q_OBJECT

public:
    explicit QtDanmakuTimerScheduler(QObject *parent = nullptr);

    TimerId once(int delayMs, Callback callback) override;
    TimerId repeating(int intervalMs, Callback callback) override;
    void cancel(TimerId id) override;

private:
    struct Entry;
    TimerId schedule(int delayMs, int intervalMs, Callback callback);

    TimerId nextId_ = 1;
    QHash<TimerId, Entry *> entries_;
};
