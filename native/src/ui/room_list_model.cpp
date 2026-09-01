#include "ui/room_list_model.h"

#include <QSet>

namespace {
QVector<int> snapshotRoles()
{
    return {
        RoomListModel::RoomIdRole,
        RoomListModel::AnchorNameRole,
        RoomListModel::TitleRole,
        RoomListModel::CategoryRole,
        RoomListModel::ViewerLabelRole,
        RoomListModel::AvatarUrlRole,
        RoomListModel::LiveStateRole,
        RoomListModel::PlaybackStateRole,
        RoomListModel::PrimaryRole,
        RoomListModel::FavoriteRole,
        RoomListModel::AudioFocusedRole,
        RoomListModel::RequestedQualityRole,
        RoomListModel::EffectiveQualityRole,
        RoomListModel::MutedRole,
        RoomListModel::AvailableQualitiesRole,
    };
}
} // namespace

RoomListModel::RoomListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int RoomListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : entries_.size();
}

QVariant RoomListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= entries_.size()) return {};

    const Entry &entry = entries_.at(index.row());
    const RoomSnapshot &snapshot = entry.snapshot;
    switch (role) {
    case RoomIdRole:
        return snapshot.roomId;
    case AnchorNameRole:
        return snapshot.metadata.anchorName;
    case TitleRole:
        return snapshot.metadata.title;
    case CategoryRole:
        return snapshot.metadata.category;
    case ViewerLabelRole:
        return snapshot.metadata.viewerLabel;
    case AvatarUrlRole:
        return snapshot.metadata.avatarUrl;
    case LiveStateRole:
        return liveState(snapshot);
    case PlaybackStateRole:
        return playbackState(snapshot);
    case PrimaryRole:
        return snapshot.isPrimary;
    case FavoriteRole:
        return snapshot.favorite;
    case AudioFocusedRole:
        return snapshot.audioFocused;
    case RequestedQualityRole:
        return qualityLabel(snapshot.requestedQuality);
    case EffectiveQualityRole:
        return qualityLabel(snapshot.effectiveQuality);
    case MutedRole:
        return snapshot.muted;
    case VolumeRole:
        return entry.settings.volume;
    case AvailableQualitiesRole:
        return snapshot.availableQualities;
    case DanmakuEnabledRole:
        return entry.settings.danmakuEnabled;
        case GroupIdRole:
            return entry.settings.groupId;
    case DanmakuStateRole:
        return entry.settings.danmakuState;
    case DanmakuErrorCodeRole:
        return entry.settings.danmakuErrorCode;
    case DanmakuRecentRateRole:
        return entry.settings.danmakuRecentRate;
    case DanmakuPeakRateRole:
        return entry.settings.danmakuPeakRate;
    case DanmakuFilteredRole:
        return entry.settings.danmakuFiltered;
    case DanmakuDuplicatesRole:
        return entry.settings.danmakuDuplicates;
    case DanmakuRateLimitedRole:
        return entry.settings.danmakuRateLimited;
    case DanmakuQueueOverflowRole:
        return entry.settings.danmakuQueueOverflow;
    case DanmakuUpstreamDroppedRole:
        return entry.settings.danmakuUpstreamDropped;
    default:
        return {};
    }
}

QHash<int, QByteArray> RoomListModel::roleNames() const
{
    return {
        {RoomIdRole, "roomId"},
        {AnchorNameRole, "anchorName"},
        {TitleRole, "title"},
        {CategoryRole, "category"},
        {ViewerLabelRole, "viewerLabel"},
        {AvatarUrlRole, "avatarUrl"},
        {LiveStateRole, "liveState"},
        {PlaybackStateRole, "playbackState"},
        {PrimaryRole, "primary"},
        {FavoriteRole, "favorite"},
        {AudioFocusedRole, "audioFocused"},
        {RequestedQualityRole, "requestedQuality"},
        {EffectiveQualityRole, "effectiveQuality"},
        {MutedRole, "muted"},
        {VolumeRole, "volume"},
        {AvailableQualitiesRole, "availableQualities"},
        {DanmakuEnabledRole, "danmakuEnabled"},
        {GroupIdRole, "groupId"},
        {DanmakuStateRole, "danmakuState"},
        {DanmakuErrorCodeRole, "danmakuErrorCode"},
        {DanmakuRecentRateRole, "danmakuRecentRate"},
        {DanmakuPeakRateRole, "danmakuPeakRate"},
        {DanmakuFilteredRole, "danmakuFiltered"},
        {DanmakuDuplicatesRole, "danmakuDuplicates"},
        {DanmakuRateLimitedRole, "danmakuRateLimited"},
        {DanmakuQueueOverflowRole, "danmakuQueueOverflow"},
        {DanmakuUpstreamDroppedRole, "danmakuUpstreamDropped"},
    };
}

void RoomListModel::applySnapshots(const RoomSnapshots &snapshots)
{
    QSet<QString> incomingIds;
    for (const RoomSnapshot &snapshot : snapshots) incomingIds.insert(snapshot.roomId);
    if (incomingIds.size() != snapshots.size()) {
        beginResetModel();
        entries_.clear();
        entries_.reserve(snapshots.size());
        for (const RoomSnapshot &snapshot : snapshots) entries_.append({snapshot, {}});
        endResetModel();
        return;
    }

    if (entries_.isEmpty() && !snapshots.isEmpty()) {
        beginInsertRows({}, 0, snapshots.size() - 1);
        entries_.reserve(snapshots.size());
        for (const RoomSnapshot &snapshot : snapshots) {
            entries_.append({snapshot, {}});
        }
        endInsertRows();
        return;
    }

    for (int row = entries_.size() - 1; row >= 0; --row) {
        if (incomingIds.contains(entries_.at(row).snapshot.roomId)) continue;
        beginRemoveRows({}, row, row);
        entries_.removeAt(row);
        endRemoveRows();
    }

    for (int row = 0; row < snapshots.size(); ++row) {
        int sourceRow = -1;
        for (int candidate = row; candidate < entries_.size(); ++candidate) {
            if (entries_.at(candidate).snapshot.roomId == snapshots.at(row).roomId) {
                sourceRow = candidate;
                break;
            }
        }

        if (sourceRow < 0) {
            beginInsertRows({}, row, row);
            entries_.insert(row, {snapshots.at(row), {}});
            endInsertRows();
        } else if (sourceRow != row) {
            beginMoveRows({}, sourceRow, sourceRow, {}, row);
            Entry moved = entries_.takeAt(sourceRow);
            entries_.insert(row, std::move(moved));
            endMoveRows();
        }
    }

    for (int row = 0; row < snapshots.size(); ++row) {
        Entry &entry = entries_[row];
        if (snapshotsEqual(entry.snapshot, snapshots.at(row))) continue;
        entry.snapshot = snapshots.at(row);
        emit dataChanged(index(row, 0), index(row, 0), snapshotRoles());
    }
}

void RoomListModel::applyPresentationSettings(
    const QHash<QString, RoomPresentationSettings> &settings)
{
    for (int row = 0; row < entries_.size(); ++row) {
        Entry &entry = entries_[row];
        const RoomPresentationSettings updated = settings.value(entry.snapshot.roomId);
        if (entry.settings == updated) continue;

        QVector<int> changedRoles;
        if (entry.settings.volume != updated.volume) changedRoles.append(VolumeRole);
        if (entry.settings.danmakuEnabled != updated.danmakuEnabled) {
            changedRoles.append(DanmakuEnabledRole);
        }
        if (entry.settings.groupId != updated.groupId) changedRoles.append(GroupIdRole);
        if (entry.settings.danmakuState != updated.danmakuState) {
            changedRoles.append(DanmakuStateRole);
        }
        if (entry.settings.danmakuErrorCode != updated.danmakuErrorCode) {
            changedRoles.append(DanmakuErrorCodeRole);
        }
        if (!qFuzzyCompare(entry.settings.danmakuRecentRate + 1,
                           updated.danmakuRecentRate + 1)) {
            changedRoles.append(DanmakuRecentRateRole);
        }
        if (!qFuzzyCompare(entry.settings.danmakuPeakRate + 1,
                           updated.danmakuPeakRate + 1)) {
            changedRoles.append(DanmakuPeakRateRole);
        }
        if (entry.settings.danmakuFiltered != updated.danmakuFiltered) {
            changedRoles.append(DanmakuFilteredRole);
        }
        if (entry.settings.danmakuDuplicates != updated.danmakuDuplicates) {
            changedRoles.append(DanmakuDuplicatesRole);
        }
        if (entry.settings.danmakuRateLimited != updated.danmakuRateLimited) {
            changedRoles.append(DanmakuRateLimitedRole);
        }
        if (entry.settings.danmakuQueueOverflow != updated.danmakuQueueOverflow) {
            changedRoles.append(DanmakuQueueOverflowRole);
        }
        if (entry.settings.danmakuUpstreamDropped != updated.danmakuUpstreamDropped) {
            changedRoles.append(DanmakuUpstreamDroppedRole);
        }
        entry.settings = updated;
        emit dataChanged(index(row, 0), index(row, 0), changedRoles);
    }
}

bool RoomListModel::snapshotsEqual(const RoomSnapshot &left, const RoomSnapshot &right) noexcept
{
    return left.roomId == right.roomId
        && left.isPrimary == right.isPrimary
        && left.state == right.state
        && left.requestedQuality == right.requestedQuality
        && left.effectiveQuality == right.effectiveQuality
        && left.metadata == right.metadata
        && left.liveStatus == right.liveStatus
        && left.playbackHealth == right.playbackHealth
        && left.favorite == right.favorite
        && left.audioFocused == right.audioFocused
        && left.muted == right.muted
        && left.volume == right.volume
        && left.availableQualities == right.availableQualities;
}

QString RoomListModel::liveState(const RoomSnapshot &snapshot)
{
    switch (snapshot.liveStatus) {
    case RoomLiveStatus::Online:
        return QStringLiteral("online");
    case RoomLiveStatus::Offline:
        return QStringLiteral("offline");
    case RoomLiveStatus::Unknown:
        return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

QString RoomListModel::playbackState(const RoomSnapshot &snapshot)
{
    if (snapshot.state == RoomSession::State::Error
        || snapshot.playbackHealth == RoomPlaybackHealth::Error) {
        return QStringLiteral("error");
    }
    if (snapshot.liveStatus == RoomLiveStatus::Offline) return QStringLiteral("offline");
    if (snapshot.state == RoomSession::State::Resolving) return QStringLiteral("loading");
    if (snapshot.playbackHealth == RoomPlaybackHealth::Playing) return QStringLiteral("playing");
    if (snapshot.state == RoomSession::State::Ready) return QStringLiteral("reconnecting");
    return QStringLiteral("idle");
}

QString RoomListModel::qualityLabel(StreamQuality quality)
{
    switch (quality) {
    case StreamQuality::Auto:
        return QStringLiteral("auto");
    case StreamQuality::Original:
        return QStringLiteral("original");
    case StreamQuality::Super:
        return QStringLiteral("super");
    case StreamQuality::High:
        return QStringLiteral("high");
    case StreamQuality::Standard:
        return QStringLiteral("standard");
    }
    return QStringLiteral("auto");
}
