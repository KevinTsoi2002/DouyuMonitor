#pragma once

#include <QUrl>
#include <QMap>
#include <QVector>
#include <QString>
#include <QStringList>

#include "danmaku/danmaku_types.h"
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
    int volume = 100;
    bool danmakuEnabled = false;

    bool operator==(const NativeRoomRecord &) const = default;
};

struct NativeRoomGroup {
    QString id;
    QString name;
    QStringList roomIds;

    bool operator==(const NativeRoomGroup &) const = default;
};

struct NativeDanmakuConfiguration {
    bool globalEnabled = true;
    DanmakuDisplaySettings display;
    DanmakuGovernanceSettings governance;
    QMap<QString, DanmakuGovernanceOverride> roomOverrides;

    bool operator==(const NativeDanmakuConfiguration &) const = default;
};

struct NativeWorkspacePreset {
    QString id;
    QString name;
    QString layoutId = QStringLiteral("auto");
    QString activeGroupId;
    QString primaryRoomId;
    QString audioRoomId;
    QStringList roomIds;
    bool sidebarVisible = true;
    double primaryRoomRatio = 0.6;
    QString audioMode = QStringLiteral("single");
    bool globalMuted = false;
    NativeDanmakuConfiguration danmaku;

    bool operator==(const NativeWorkspacePreset &) const = default;
};

struct NativeWorkspaceSnapshot {
    int version = 3;
    QVector<NativeRoomRecord> library;
    QVector<NativeRoomGroup> groups;
    QVector<NativeWorkspacePreset> presets;
    NativeDanmakuConfiguration danmaku;
    QStringList activeRoomIds;
    QString activeGroupId;
    QString primaryRoomId;
    QString audioRoomId;
    QString layoutId = QStringLiteral("auto");
    double primaryRoomRatio = 0.6;
    bool sidebarVisible = true;
    QString audioMode = QStringLiteral("single");
    bool globalMuted = false;

    bool operator==(const NativeWorkspaceSnapshot &) const = default;
};

Q_DECLARE_METATYPE(RoomLiveStatus)
Q_DECLARE_METATYPE(RoomPlaybackHealth)
