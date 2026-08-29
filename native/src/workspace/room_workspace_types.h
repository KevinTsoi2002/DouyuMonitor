#pragma once

#include <QMetaType>
#include <QString>
#include <QVector>
#include <QVariantList>

#include "service/stream_service_protocol.h"
#include "workspace/native_workspace_types.h"
#include "workspace/room_session.h"

enum class RoomCommandResult {
    Accepted,
    InvalidRoomId,
    DuplicateRoomId,
    RoomLimitReached,
    RoomNotFound,
    AlreadyPrimary,
    Unchanged,
    Unavailable,
};

struct RoomSnapshot {
    QString roomId;
    bool isPrimary = false;
    RoomSession::State state = RoomSession::State::Idle;
    StreamQuality requestedQuality = StreamQuality::Auto;
    StreamQuality effectiveQuality = StreamQuality::Auto;
    RoomMetadata metadata;
    RoomLiveStatus liveStatus = RoomLiveStatus::Unknown;
    RoomPlaybackHealth playbackHealth = RoomPlaybackHealth::Pending;
    bool favorite = false;
    bool audioFocused = false;
    bool muted = true;
    int volume = 100;
    QVariantList availableQualities;
};

using RoomSnapshots = QVector<RoomSnapshot>;

Q_DECLARE_METATYPE(RoomCommandResult)
Q_DECLARE_METATYPE(RoomSnapshot)
Q_DECLARE_METATYPE(RoomSnapshots)
