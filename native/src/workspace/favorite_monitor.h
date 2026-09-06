#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QVector>

#include "service/stream_service_protocol.h"
#include "workspace/room_status_scheduler.h"
#include "workspace/room_workspace_types.h"
#include "workspace/notification_policy.h"

struct FavoriteRoomSpec {
    QString roomId;
    RoomMetadata metadata;
    RoomLiveStatus liveStatus = RoomLiveStatus::Unknown;
};

class FavoriteMonitor final : public QObject {
    Q_OBJECT

public:
    using StartSearch = RoomStatusScheduler::StartSearch;
    using CancelRequest = RoomStatusScheduler::CancelRequest;

    explicit FavoriteMonitor(StartSearch startSearch,
                             CancelRequest cancelRequest,
                             RoomRefreshTiming timing = {},
                             QObject *parent = nullptr);

    void synchronize(const QVector<FavoriteRoomSpec> &rooms);
    void requestNow(const QString &roomId);
    void onSearchResponse(const ServiceResponse &response);
    void onRequestFailed(quint64 requestId);
    void stop();

signals:
    void eventsReady(QVector<NotificationEvent> events);
    void roomUpdated(QString roomId, RoomMetadata metadata, RoomLiveStatus liveStatus);

private:
    struct State {
        FavoriteRoomSpec room;
        bool hasBaseline = false;
    };

    static RoomSnapshots schedulerSnapshots(const QVector<FavoriteRoomSpec> &rooms);
    void processResult(const QString &roomId, const RoomSearchResult &result);

    RoomStatusScheduler scheduler_;
    QHash<QString, State> states_;
    NotificationPolicy policy_;
};
