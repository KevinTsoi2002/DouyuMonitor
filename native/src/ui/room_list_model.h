#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QVector>

#include "workspace/room_workspace_types.h"

struct RoomPresentationSettings {
    int volume = 100;
    bool danmakuEnabled = false;
    QString groupId;
    QStringList groupIds;
    QString danmakuState = QStringLiteral("idle");
    QString danmakuErrorCode = QStringLiteral("NONE");
    qreal danmakuRecentRate = 0;
    qreal danmakuPeakRate = 0;
    int danmakuFiltered = 0;
    int danmakuDuplicates = 0;
    int danmakuRateLimited = 0;
    int danmakuQueueOverflow = 0;
    int danmakuUpstreamDropped = 0;

    bool operator==(const RoomPresentationSettings &) const = default;
};

class RoomListModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role : int {
        RoomIdRole = Qt::UserRole + 1,
        AnchorNameRole,
        TitleRole,
        CategoryRole,
        ViewerLabelRole,
        AvatarUrlRole,
        LiveStateRole,
        PlaybackStateRole,
        PrimaryRole,
        SecondaryPrimaryRole,
        FavoriteRole,
        AudioFocusedRole,
        RequestedQualityRole,
        EffectiveQualityRole,
        MutedRole,
        VolumeRole,
        AvailableQualitiesRole,
        DanmakuEnabledRole,
        GroupIdRole,
        GroupIdsRole,
        DanmakuStateRole,
        DanmakuErrorCodeRole,
        DanmakuRecentRateRole,
        DanmakuPeakRateRole,
        DanmakuFilteredRole,
        DanmakuDuplicatesRole,
        DanmakuRateLimitedRole,
        DanmakuQueueOverflowRole,
        DanmakuUpstreamDroppedRole,
    };
    Q_ENUM(Role)

    explicit RoomListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void applySnapshots(const RoomSnapshots &snapshots);
    void applyPresentationSettings(const QHash<QString, RoomPresentationSettings> &settings);

private:
    struct Entry {
        RoomSnapshot snapshot;
        RoomPresentationSettings settings;
    };

    static bool snapshotsEqual(const RoomSnapshot &left, const RoomSnapshot &right) noexcept;
    static QString liveState(const RoomSnapshot &snapshot);
    static QString playbackState(const RoomSnapshot &snapshot);
    static QString qualityLabel(StreamQuality quality);

    QVector<Entry> entries_;
};
