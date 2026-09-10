#include "workspace/favorite_monitor.h"

#include <QSet>
#include <QDateTime>

FavoriteMonitor::FavoriteMonitor(StartSearch startSearch,
                                 CancelRequest cancelRequest,
                                 RoomRefreshTiming timing,
                                 QObject *parent)
    : QObject(parent)
    , scheduler_(std::move(startSearch), std::move(cancelRequest), std::move(timing), this)
    , policy_([] { return QDateTime::currentMSecsSinceEpoch(); })
{
    qRegisterMetaType<QVector<NotificationEvent>>();
}

void FavoriteMonitor::synchronize(const QVector<FavoriteRoomSpec> &rooms)
{
    QSet<QString> incomingIds;
    QVector<RoomSnapshot> schedulerRooms;
    schedulerRooms.reserve(rooms.size());
    for (const FavoriteRoomSpec &room : rooms) {
        if (room.roomId.isEmpty() || incomingIds.contains(room.roomId)) continue;
        incomingIds.insert(room.roomId);
        if (!states_.contains(room.roomId)) {
            states_.insert(room.roomId, State{room, false});
        } else {
            states_[room.roomId].room.metadata = room.metadata;
            states_[room.roomId].room.liveStatus = room.liveStatus;
        }

        RoomSnapshot snapshot;
        snapshot.roomId = room.roomId;
        snapshot.metadata = room.metadata;
        snapshot.liveStatus = room.liveStatus;
        snapshot.favorite = true;
        schedulerRooms.push_back(snapshot);
    }

    const QStringList existingIds = states_.keys();
    for (const QString &roomId : existingIds) {
        if (incomingIds.contains(roomId)) continue;
        states_.remove(roomId);
        policy_.forgetRoom(roomId);
    }

    scheduler_.synchronize(schedulerRooms);
    for (const QString &roomId : incomingIds) {
        if (!states_.value(roomId).hasBaseline) scheduler_.requestNow(roomId);
    }
}

void FavoriteMonitor::requestNow(const QString &roomId)
{
    scheduler_.requestNow(roomId);
}

void FavoriteMonitor::onSearchResponse(const ServiceResponse &response)
{
    const auto roomId = scheduler_.roomForRequest(response.requestId);
    if (!roomId.has_value()) return;
    const bool valid = response.ok && response.search && response.results.size() == 1
        && response.results.first().roomId == *roomId;
    const auto completed = scheduler_.takeCompletedRequest(response.requestId, valid);
    if (!completed.has_value() || !valid) return;
    processResult(*completed, response.results.first());
}

void FavoriteMonitor::onRequestFailed(quint64 requestId)
{
    scheduler_.takeCompletedRequest(requestId, false);
}

void FavoriteMonitor::stop()
{
    scheduler_.stop();
    states_.clear();
    policy_.resetBaseline();
}

void FavoriteMonitor::processResult(const QString &roomId, const RoomSearchResult &result)
{
    auto it = states_.find(roomId);
    if (it == states_.end()) return;

    it->room.metadata.roomId = roomId;
    it->room.metadata.anchorName = result.anchorName;
    it->room.metadata.title = result.title;
    it->room.metadata.category = result.category;
    it->room.metadata.viewerLabel = result.viewerLabel;
    it->room.metadata.avatarUrl = result.avatarUrl;
    it->room.liveStatus = result.online ? RoomLiveStatus::Online : RoomLiveStatus::Offline;

    RoomSnapshot snapshot;
    snapshot.roomId = roomId;
    snapshot.metadata = it->room.metadata;
    snapshot.liveStatus = it->room.liveStatus;
    snapshot.favorite = true;
    const QVector<NotificationEvent> events = policy_.update({snapshot});
    it->hasBaseline = true;
    emit roomUpdated(roomId, it->room.metadata, it->room.liveStatus);
    if (!events.isEmpty()) emit eventsReady(events);
}
