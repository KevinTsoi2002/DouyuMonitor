#include "workspace/room_status_scheduler.h"

#include <QSet>
#include <QTimer>

#include <algorithm>

RoomStatusScheduler::RoomStatusScheduler(StartSearch startSearch,
                                         CancelRequest cancelRequest,
                                         RoomRefreshTiming timing,
                                         QObject *parent)
    : QObject(parent)
    , startSearch_(std::move(startSearch))
    , cancelRequest_(std::move(cancelRequest))
    , timing_(std::move(timing))
{
}

RoomStatusScheduler::~RoomStatusScheduler()
{
    stop();
}

void RoomStatusScheduler::synchronize(const RoomSnapshots &rooms)
{
    QSet<QString> incomingIds;
    for (const RoomSnapshot &room : rooms) {
        if (room.roomId.isEmpty() || incomingIds.contains(room.roomId)) continue;
        incomingIds.insert(room.roomId);

        auto it = entries_.find(room.roomId);
        if (it == entries_.end()) {
            Entry entry;
            entry.liveStatus = room.liveStatus;
            auto inserted = entries_.insert(room.roomId, entry);
            scheduleBase(room.roomId, inserted.value());
            continue;
        }

        const bool statusChanged = it->liveStatus != room.liveStatus;
        it->liveStatus = room.liveStatus;
        if (statusChanged && it->requestId == 0) {
            it->failureCount = 0;
            scheduleBase(room.roomId, it.value());
        }
    }

    const QStringList existingIds = entries_.keys();
    for (const QString &roomId : existingIds) {
        if (!incomingIds.contains(roomId)) removeRoom(roomId);
    }
}

void RoomStatusScheduler::requestNow(const QString &roomId)
{
    auto it = entries_.find(roomId);
    if (it == entries_.end() || it->requestId != 0) return;

    if (it->timer != nullptr) it->timer->stop();
    ++it->generation;

    const quint64 requestId = startSearch_ ? startSearch_(roomId) : 0;
    it = entries_.find(roomId);
    if (it == entries_.end()) return;

    if (requestId == 0) {
        ++it->failureCount;
        scheduleRetry(roomId, it.value());
        return;
    }

    it->requestId = requestId;
    requestRooms_.insert(requestId, roomId);
}

std::optional<QString> RoomStatusScheduler::roomForRequest(quint64 requestId) const
{
    const auto it = requestRooms_.constFind(requestId);
    if (it == requestRooms_.cend()) return std::nullopt;
    return it.value();
}

std::optional<QString> RoomStatusScheduler::takeCompletedRequest(quint64 requestId,
                                                                  bool succeeded)
{
    const auto requestIt = requestRooms_.find(requestId);
    if (requestIt == requestRooms_.end()) return std::nullopt;

    const QString roomId = requestIt.value();
    requestRooms_.erase(requestIt);
    auto entryIt = entries_.find(roomId);
    if (entryIt == entries_.end() || entryIt->requestId != requestId) return std::nullopt;

    entryIt->requestId = 0;
    if (succeeded) {
        entryIt->failureCount = 0;
        scheduleBase(roomId, entryIt.value());
    } else {
        ++entryIt->failureCount;
        scheduleRetry(roomId, entryIt.value());
    }
    return roomId;
}

void RoomStatusScheduler::stop()
{
    const QStringList roomIds = entries_.keys();
    for (const QString &roomId : roomIds) removeRoom(roomId);
    requestRooms_.clear();
}

void RoomStatusScheduler::scheduleBase(const QString &roomId, Entry &entry)
{
    schedule(roomId, entry, baseIntervalMs(entry.liveStatus));
}

void RoomStatusScheduler::scheduleRetry(const QString &roomId, Entry &entry)
{
    if (timing_.retryDelaysMs.isEmpty()) {
        scheduleBase(roomId, entry);
        return;
    }

    const int lastIndex = static_cast<int>(timing_.retryDelaysMs.size()) - 1;
    const int index = std::clamp(entry.failureCount - 1, 0, lastIndex);
    schedule(roomId, entry, timing_.retryDelaysMs.at(index));
}

void RoomStatusScheduler::schedule(const QString &roomId, Entry &entry, int delayMs)
{
    if (entry.timer == nullptr) {
        entry.timer = new QTimer(this);
        entry.timer->setSingleShot(true);
    }

    if (entry.timer->isActive()) entry.timer->stop();
    const quint64 generation = ++entry.generation;
    disconnect(entry.timer, nullptr, this, nullptr);
    connect(entry.timer, &QTimer::timeout, this, [this, roomId, generation] {
        const auto it = entries_.find(roomId);
        if (it == entries_.end() || it->generation != generation) return;
        requestNow(roomId);
    });
    entry.timer->start(std::max(1, jitteredDelayMs(roomId, delayMs)));
}

void RoomStatusScheduler::removeRoom(const QString &roomId)
{
    auto it = entries_.find(roomId);
    if (it == entries_.end()) return;

    const quint64 requestId = it->requestId;
    it->requestId = 0;
    if (requestId != 0) {
        requestRooms_.remove(requestId);
        if (cancelRequest_) cancelRequest_(requestId);
    }
    if (it->timer != nullptr) {
        it->timer->stop();
        it->timer->deleteLater();
        it->timer = nullptr;
    }
    entries_.erase(it);
}

int RoomStatusScheduler::baseIntervalMs(RoomLiveStatus status) const noexcept
{
    return status == RoomLiveStatus::Offline
        ? timing_.offlineIntervalMs
        : timing_.onlineIntervalMs;
}

int RoomStatusScheduler::jitteredDelayMs(const QString &roomId, int delayMs) const noexcept
{
    if (delayMs <= 0 || timing_.jitterPercent <= 0) return delayMs;
    const int range = delayMs * timing_.jitterPercent / 100;
    if (range <= 0) return delayMs;
    const uint hash = qHash(roomId);
    const int offset = static_cast<int>(hash % static_cast<uint>(range * 2 + 1)) - range;
    return delayMs + offset;
}
