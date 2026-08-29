#include "ui/workspace_model.h"

#include <QVariantMap>

WorkspaceModel::WorkspaceModel(AppController *controller, QObject *parent)
    : QObject(parent)
    , controller_(controller)
{
}

bool WorkspaceModel::sidebarVisible() const noexcept
{
    return sidebarVisible_;
}

QString WorkspaceModel::layoutId() const
{
    return layoutId_;
}

QString WorkspaceModel::layoutMode() const
{
    return layoutMode_;
}

double WorkspaceModel::primaryRoomRatio() const noexcept
{
    return primaryRoomRatio_;
}

QString WorkspaceModel::primaryRoomId() const
{
    return primaryRoomId_;
}

QString WorkspaceModel::audioRoomId() const
{
    return audioRoomId_;
}

QString WorkspaceModel::audioMode() const
{
    return audioMode_;
}

bool WorkspaceModel::globalMuted() const noexcept
{
    return globalMuted_;
}

bool WorkspaceModel::danmakuEnabled() const noexcept
{
    return danmakuEnabled_;
}

int WorkspaceModel::maxRooms() const noexcept
{
    return 9;
}

QVariantList WorkspaceModel::groupItems() const
{
    QVariantList items;
    items.reserve(groups_.size());
    for (const NativeRoomGroup &group : groups_) {
        items.append(QVariantMap{
            {QStringLiteral("id"), group.id},
            {QStringLiteral("name"), group.name},
            {QStringLiteral("roomIds"), group.roomIds},
            {QStringLiteral("active"), group.id == activeGroupId_},
        });
    }
    return items;
}

QVariantList WorkspaceModel::presetItems() const
{
    QVariantList items;
    items.reserve(presets_.size());
    for (const NativeWorkspacePreset &preset : presets_) {
        items.append(QVariantMap{
            {QStringLiteral("id"), preset.id},
            {QStringLiteral("name"), preset.name},
            {QStringLiteral("layoutId"), preset.layoutId},
        });
    }
    return items;
}

QString WorkspaceModel::lastMessage() const
{
    return lastMessage_;
}

QString WorkspaceModel::lastMessageLevel() const
{
    return lastMessageLevel_;
}

int WorkspaceModel::lastMessageTimeoutMs() const noexcept
{
    return lastMessageTimeoutMs_;
}

void WorkspaceModel::setSidebarVisible(bool visible)
{
    if (sidebarVisible_ == visible) return;
    sidebarVisible_ = visible;
    emit sidebarVisibleChanged();
}

QString WorkspaceModel::commandMessage(RoomCommandResult result) const
{
    switch (result) {
    case RoomCommandResult::Accepted:
    case RoomCommandResult::Unchanged:
        return {};
    case RoomCommandResult::InvalidRoomId:
        return QStringLiteral("请输入有效房间号");
    case RoomCommandResult::DuplicateRoomId:
        return QStringLiteral("该房间已在列表中");
    case RoomCommandResult::RoomLimitReached:
        return QStringLiteral("最多添加 9 个房间");
    case RoomCommandResult::RoomNotFound:
        return QStringLiteral("未找到该房间");
    case RoomCommandResult::AlreadyPrimary:
        return QStringLiteral("该房间已是主画面");
    case RoomCommandResult::Unavailable:
        return QStringLiteral("当前操作不可用");
    }
    return QStringLiteral("当前操作不可用");
}

void WorkspaceModel::setAudioRoomId(QString roomId)
{
    if (audioRoomId_ == roomId) return;
    audioRoomId_ = std::move(roomId);
    emit audioRoomIdChanged();
}

void WorkspaceModel::setCoordinatorState(QString layoutId,
                                         QString primaryRoomId,
                                         QString audioRoomId)
{
    if (layoutId_ != layoutId) {
        layoutId_ = std::move(layoutId);
        emit layoutIdChanged();
    }
    if (primaryRoomId_ != primaryRoomId) {
        primaryRoomId_ = std::move(primaryRoomId);
        emit primaryRoomIdChanged();
    }
    setAudioRoomId(std::move(audioRoomId));
}

void WorkspaceModel::setAudioPolicy(QString audioMode, bool globalMuted)
{
    if (audioMode_ != audioMode) {
        audioMode_ = std::move(audioMode);
        emit audioModeChanged();
    }
    if (globalMuted_ != globalMuted) {
        globalMuted_ = globalMuted;
        emit globalMutedChanged();
    }
}

void WorkspaceModel::setLayoutPresentation(QString layoutMode, double primaryRoomRatio)
{
    if (layoutMode_ != layoutMode) {
        layoutMode_ = std::move(layoutMode);
        emit layoutIdChanged();
    }
    if (!qFuzzyCompare(primaryRoomRatio_ + 1.0, primaryRoomRatio + 1.0)) {
        primaryRoomRatio_ = primaryRoomRatio;
        emit primaryRoomRatioChanged();
    }
}

void WorkspaceModel::setLastMessage(QString message)
{
    setLastMessage(std::move(message), QStringLiteral("info"), 3200);
}

void WorkspaceModel::setLastMessage(QString message, QString level, int timeoutMs)
{
    const QString normalizedLevel = level == QStringLiteral("success")
        || level == QStringLiteral("error")
        ? std::move(level)
        : QStringLiteral("info");
    const int normalizedTimeout = qBound(0, timeoutMs, 60000);

    if (lastMessage_ != message) {
        lastMessage_ = std::move(message);
        emit lastMessageChanged();
    }
    if (lastMessageLevel_ != normalizedLevel) {
        lastMessageLevel_ = normalizedLevel;
        emit lastMessageLevelChanged();
    }
    if (lastMessageTimeoutMs_ != normalizedTimeout) {
        lastMessageTimeoutMs_ = normalizedTimeout;
        emit lastMessageTimeoutChanged();
    }
}

const QVector<NativeRoomGroup> &WorkspaceModel::groups() const noexcept
{
    return groups_;
}

const QVector<NativeWorkspacePreset> &WorkspaceModel::presets() const noexcept
{
    return presets_;
}

void WorkspaceModel::setWorkspaceData(QVector<NativeRoomGroup> groups,
                                      QVector<NativeWorkspacePreset> presets,
                                      QString activeGroupId)
{
    if (groups_ == groups && presets_ == presets && activeGroupId_ == activeGroupId) return;
    groups_ = std::move(groups);
    presets_ = std::move(presets);
    activeGroupId_ = std::move(activeGroupId);
    emit workspaceDataChanged();
}
