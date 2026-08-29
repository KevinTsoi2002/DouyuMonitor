#pragma once

#include <QHash>
#include <QQueue>
#include <QString>
#include <QVector>

#include <functional>

#include "workspace/room_workspace_types.h"

enum class NotificationEventType {
    RoomOnline,
    RoomOffline,
    PlaybackFailed,
    PlaybackRecovered,
};

struct NotificationEvent {
    NotificationEventType type = NotificationEventType::RoomOnline;
    QString roomId;
    QString anchorName;
    QString title;
    QString body;
};

class NotificationPolicy final {
public:
    explicit NotificationPolicy(std::function<qint64()> nowMs);

    QVector<NotificationEvent> update(const RoomSnapshots &snapshots);
    void resetBaseline();

private:
    struct State {
        RoomLiveStatus liveStatus = RoomLiveStatus::Unknown;
        RoomPlaybackHealth playbackHealth = RoomPlaybackHealth::Pending;
        QString anchorName;
        QString title;
    };

    bool canEmit(const QString &roomId, NotificationEventType type, qint64 nowMs);
    static QString eventKey(const QString &roomId, NotificationEventType type);
    static NotificationEvent makeEvent(NotificationEventType type,
                                       const RoomSnapshot &snapshot);

    std::function<qint64()> nowMs_;
    QHash<QString, State> states_;
    QHash<QString, qint64> lastEmittedMs_;
    QQueue<qint64> emittedAtMs_;
};
