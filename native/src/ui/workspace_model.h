#pragma once

#include <QObject>
#include <QVariantList>
#include <QVector>

#include "workspace/native_workspace_types.h"
#include "workspace/room_workspace_types.h"

class AppController;

class WorkspaceModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool sidebarVisible READ sidebarVisible NOTIFY sidebarVisibleChanged)
    Q_PROPERTY(QString layoutId READ layoutId NOTIFY layoutIdChanged)
    Q_PROPERTY(QString layoutMode READ layoutMode NOTIFY layoutIdChanged)
    Q_PROPERTY(double primaryRoomRatio READ primaryRoomRatio NOTIFY primaryRoomRatioChanged)
    Q_PROPERTY(QString primaryRoomId READ primaryRoomId NOTIFY primaryRoomIdChanged)
    Q_PROPERTY(QString secondaryPrimaryRoomId READ secondaryPrimaryRoomId NOTIFY secondaryPrimaryRoomIdChanged)
    Q_PROPERTY(QString audioRoomId READ audioRoomId NOTIFY audioRoomIdChanged)
    Q_PROPERTY(QString audioMode READ audioMode NOTIFY audioModeChanged)
    Q_PROPERTY(bool globalMuted READ globalMuted NOTIFY globalMutedChanged)
    Q_PROPERTY(bool danmakuEnabled READ danmakuEnabled NOTIFY danmakuEnabledChanged)
    Q_PROPERTY(int maxRooms READ maxRooms CONSTANT)
    Q_PROPERTY(QVariantList groups READ groupItems NOTIFY workspaceDataChanged)
    Q_PROPERTY(QVariantList presets READ presetItems NOTIFY workspaceDataChanged)
    Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY lastMessageChanged)
    Q_PROPERTY(QString lastMessageLevel READ lastMessageLevel NOTIFY lastMessageLevelChanged)
    Q_PROPERTY(int lastMessageTimeoutMs READ lastMessageTimeoutMs NOTIFY lastMessageTimeoutChanged)

public:
    explicit WorkspaceModel(AppController *controller, QObject *parent = nullptr);

    bool sidebarVisible() const noexcept;
    QString layoutId() const;
    QString layoutMode() const;
    double primaryRoomRatio() const noexcept;
    QString primaryRoomId() const;
    QString secondaryPrimaryRoomId() const;
    QString audioRoomId() const;
    QString audioMode() const;
    bool globalMuted() const noexcept;
    bool danmakuEnabled() const noexcept;
    int maxRooms() const noexcept;
    QVariantList groupItems() const;
    QVariantList presetItems() const;
    QString lastMessage() const;
    QString lastMessageLevel() const;
    int lastMessageTimeoutMs() const noexcept;

    QString commandMessage(RoomCommandResult result) const;
    void setAudioRoomId(QString roomId);
    void setSidebarVisible(bool visible);
    void setCoordinatorState(QString layoutId, QString primaryRoomId, QString secondaryPrimaryRoomId, QString audioRoomId);
    void setAudioPolicy(QString audioMode, bool globalMuted);
    void setLayoutPresentation(QString layoutMode, double primaryRoomRatio);
    Q_INVOKABLE void setLastMessage(QString message);
    Q_INVOKABLE void setLastMessage(QString message, QString level, int timeoutMs);
    const QVector<NativeRoomGroup> &groups() const noexcept;
    const QVector<NativeWorkspacePreset> &presets() const noexcept;
    void setWorkspaceData(QVector<NativeRoomGroup> groups,
                          QVector<NativeWorkspacePreset> presets,
                          QString activeGroupId);

signals:
    void sidebarVisibleChanged();
    void layoutIdChanged();
    void primaryRoomRatioChanged();
    void primaryRoomIdChanged();
    void secondaryPrimaryRoomIdChanged();
    void audioRoomIdChanged();
    void audioModeChanged();
    void globalMutedChanged();
    void danmakuEnabledChanged();
    void workspaceDataChanged();
    void lastMessageChanged();
    void lastMessageLevelChanged();
    void lastMessageTimeoutChanged();

private:
    AppController *controller_ = nullptr;
    bool sidebarVisible_ = true;
    QString layoutId_ = QStringLiteral("auto");
    QString layoutMode_ = QStringLiteral("auto");
    double primaryRoomRatio_ = 0.6;
    QString primaryRoomId_;
    QString secondaryPrimaryRoomId_;
    QString audioRoomId_;
    QString audioMode_ = QStringLiteral("single");
    bool globalMuted_ = false;
    bool danmakuEnabled_ = false;
    QVector<NativeRoomGroup> groups_;
    QVector<NativeWorkspacePreset> presets_;
    QString activeGroupId_;
    QString lastMessage_;
    QString lastMessageLevel_ = QStringLiteral("info");
    int lastMessageTimeoutMs_ = 3200;
};
