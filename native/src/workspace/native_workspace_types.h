#pragma once

#include <QUrl>
#include <QVector>
#include <QString>
#include <QStringList>

#include "service/stream_service_protocol.h"

enum class RoomLiveStatus {
    Unknown,
    Online,
    Offline,
};

enum class RoomPlaybackHealth {
    Pending,
    Playing,
    Error,
};

struct RoomMetadata {
    QString roomId;
    QString anchorName;
    QString title;
    QString category;
    QString viewerLabel;
    QUrl avatarUrl;

    bool operator==(const RoomMetadata &) const = default;
};

struct NativeRoomRecord {
    QString roomId;
    RoomMetadata metadata;
    StreamQuality requestedQuality = StreamQuality::Auto;
    bool favorite = false;
    qint64 lastOpenedAtMs = 0;

    bool operator==(const NativeRoomRecord &) const = default;
};

struct NativeRoomGroup {
    QString id;
    QString name;
    QStringList roomIds;

    bool operator==(const NativeRoomGroup &) const = default;
};

struct NativeWorkspaceSnapshot {
    int version = 1;
    QVector<NativeRoomRecord> library;
    QVector<NativeRoomGroup> groups;
    QStringList activeRoomIds;
    QString activeGroupId;
    QString primaryRoomId;
    QString audioRoomId;

    bool operator==(const NativeWorkspaceSnapshot &) const = default;
};

Q_DECLARE_METATYPE(RoomLiveStatus)
Q_DECLARE_METATYPE(RoomPlaybackHealth)
