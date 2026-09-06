#include "workspace/notification_policy.h"

#include <QStringBuilder>

#include <optional>

namespace {

constexpr qint64 kDedupeWindowMs = 5 * 60 * 1000;
constexpr qint64 kRateWindowMs = 60 * 1000;
constexpr int kMaxEventsPerRateWindow = 6;

QString typeKey(NotificationEventType type)
{
    switch (type) {
    case NotificationEventType::RoomOnline:
        return QStringLiteral("online");
    case NotificationEventType::RoomOffline:
        return QStringLiteral("offline");
    case NotificationEventType::PlaybackFailed:
        return QStringLiteral("playback-failed");
    case NotificationEventType::PlaybackRecovered:
        return QStringLiteral("playback-recovered");
    case NotificationEventType::FavoriteTitleChanged:
        return QStringLiteral("favorite-title-changed");
    }
    return QStringLiteral("unknown");
}

} // namespace

NotificationPolicy::NotificationPolicy(std::function<qint64()> nowMs)
    : nowMs_(std::move(nowMs))
{
}

QVector<NotificationEvent> NotificationPolicy::update(const RoomSnapshots &snapshots)
{
    const qint64 now = nowMs_ ? nowMs_() : 0;
    while (!emittedAtMs_.isEmpty() && emittedAtMs_.front() <= now - kRateWindowMs) {
        emittedAtMs_.dequeue();
    }

    QVector<NotificationEvent> events;
    for (const RoomSnapshot &snapshot : snapshots) {
        if (snapshot.roomId.isEmpty()) continue;

        const auto previous = states_.constFind(snapshot.roomId);
        if (previous == states_.cend()) {
            states_.insert(snapshot.roomId,
                           {snapshot.liveStatus,
                            snapshot.playbackHealth,
                            snapshot.metadata.anchorName,
                            snapshot.metadata.title});
            continue;
        }

        const State old = previous.value();
        std::optional<NotificationEventType> eventType;
        if (old.liveStatus != snapshot.liveStatus) {
            if (snapshot.liveStatus == RoomLiveStatus::Online
                && old.liveStatus == RoomLiveStatus::Offline) {
                eventType = NotificationEventType::RoomOnline;
            } else if (snapshot.liveStatus == RoomLiveStatus::Offline
                       && old.liveStatus == RoomLiveStatus::Online) {
                eventType = NotificationEventType::RoomOffline;
            }
        }

        if (!eventType.has_value()
            && snapshot.liveStatus == RoomLiveStatus::Online
            && old.liveStatus == RoomLiveStatus::Online) {
            if (old.playbackHealth != RoomPlaybackHealth::Error
                && snapshot.playbackHealth == RoomPlaybackHealth::Error) {
                eventType = NotificationEventType::PlaybackFailed;
            } else if (old.playbackHealth == RoomPlaybackHealth::Error
                       && snapshot.playbackHealth == RoomPlaybackHealth::Playing) {
                eventType = NotificationEventType::PlaybackRecovered;
            }
        }

        if (!eventType.has_value()
            && snapshot.favorite
            && old.liveStatus == RoomLiveStatus::Online
            && snapshot.liveStatus == RoomLiveStatus::Online
            && !old.title.isEmpty()
            && !snapshot.metadata.title.isEmpty()
            && old.title != snapshot.metadata.title) {
            eventType = NotificationEventType::FavoriteTitleChanged;
        }

        if (eventType.has_value() && canEmit(snapshot.roomId, *eventType, now)) {
            events.push_back(makeEvent(*eventType, snapshot, old.title));
            emittedAtMs_.enqueue(now);
        }

        states_[snapshot.roomId] = {snapshot.liveStatus,
                                    snapshot.playbackHealth,
                                    snapshot.metadata.anchorName,
                                    snapshot.metadata.title};
    }
    return events;
}

void NotificationPolicy::resetBaseline()
{
    states_.clear();
    lastEmittedMs_.clear();
    emittedAtMs_.clear();
}

void NotificationPolicy::forgetRoom(const QString &roomId)
{
    states_.remove(roomId);
    const QString prefix = roomId + QLatin1Char(':');
    for (auto it = lastEmittedMs_.begin(); it != lastEmittedMs_.end();) {
        if (it.key().startsWith(prefix)) it = lastEmittedMs_.erase(it);
        else ++it;
    }
}

bool NotificationPolicy::canEmit(const QString &roomId,
                                 NotificationEventType type,
                                 qint64 nowMs)
{
    if (emittedAtMs_.size() >= kMaxEventsPerRateWindow) return false;
    const QString key = eventKey(roomId, type);
    const auto previous = lastEmittedMs_.constFind(key);
    if (previous != lastEmittedMs_.cend() && nowMs - previous.value() < kDedupeWindowMs) {
        return false;
    }
    lastEmittedMs_[key] = nowMs;
    return true;
}

QString NotificationPolicy::eventKey(const QString &roomId, NotificationEventType type)
{
    return roomId % QLatin1Char(':') % typeKey(type);
}

NotificationEvent NotificationPolicy::makeEvent(NotificationEventType type,
                                                 const RoomSnapshot &snapshot,
                                                 const QString &previousTitle)
{
    const QString name = snapshot.metadata.anchorName.isEmpty()
        ? snapshot.roomId
        : snapshot.metadata.anchorName;
    const QString title = snapshot.metadata.title.isEmpty()
        ? QStringLiteral("斗鱼房间状态")
        : snapshot.metadata.title;

    NotificationEvent event;
    event.type = type;
    event.roomId = snapshot.roomId;
    event.anchorName = name;
    event.title = title;
    event.previousTitle = previousTitle;
    switch (type) {
    case NotificationEventType::RoomOnline:
        event.body = name + QStringLiteral(" 已开播");
        break;
    case NotificationEventType::RoomOffline:
        event.body = name + QStringLiteral(" 已下播");
        break;
    case NotificationEventType::PlaybackFailed:
        event.body = name + QStringLiteral(" 播放异常");
        break;
    case NotificationEventType::PlaybackRecovered:
        event.body = name + QStringLiteral(" 播放已恢复");
        break;
    case NotificationEventType::FavoriteTitleChanged:
        event.body = name + QStringLiteral(" 的直播间标题已更新\n原标题：")
            + previousTitle + QStringLiteral("\n新标题：") + snapshot.metadata.title;
        break;
    }
    return event;
}
