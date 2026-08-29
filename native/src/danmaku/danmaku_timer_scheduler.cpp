#include "danmaku/danmaku_timer_scheduler.h"

#include <QHash>
#include <QTimer>

struct QtDanmakuTimerScheduler::Entry {
    QTimer *timer = nullptr;
    Callback callback;
};

QtDanmakuTimerScheduler::QtDanmakuTimerScheduler(QObject *parent)
    : QObject(parent)
{
}

DanmakuTimerScheduler::TimerId QtDanmakuTimerScheduler::once(int delayMs, Callback callback)
{
    return schedule(delayMs, 0, std::move(callback));
}

DanmakuTimerScheduler::TimerId QtDanmakuTimerScheduler::repeating(int intervalMs,
                                                                   Callback callback)
{
    return schedule(intervalMs, intervalMs, std::move(callback));
}

void QtDanmakuTimerScheduler::cancel(TimerId id)
{
    auto it = entries_.find(id);
    if (it == entries_.end()) return;
    Entry *entry = it.value();
    entries_.erase(it);
    entry->timer->stop();
    entry->timer->deleteLater();
    delete entry;
}

DanmakuTimerScheduler::TimerId QtDanmakuTimerScheduler::schedule(int delayMs,
                                                                  int intervalMs,
                                                                  Callback callback)
{
    const TimerId id = nextId_++;
    auto *entry = new Entry;
    entry->timer = new QTimer(this);
    entry->callback = std::move(callback);
    entry->timer->setSingleShot(intervalMs == 0);
    entry->timer->setInterval(intervalMs == 0 ? delayMs : intervalMs);
    entries_.insert(id, entry);
    connect(entry->timer, &QTimer::timeout, this, [this, id] {
        auto it = entries_.find(id);
        if (it == entries_.end()) return;
        Entry *entry = it.value();
        if (entry->timer->isSingleShot()) {
            const auto callback = entry->callback;
            entries_.erase(it);
            entry->timer->deleteLater();
            delete entry;
            callback();
            return;
        }
        entry->callback();
    });
    entry->timer->start();
    return id;
}
