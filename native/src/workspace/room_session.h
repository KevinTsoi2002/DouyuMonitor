#pragma once

#include <QObject>
#include <QString>

#include "media/remote_playback_controller.h"
#include "media/player_surface.h"
#include "service/stream_service_protocol.h"
#include "workspace/native_workspace_types.h"

class StreamgetProcessClient;

class RoomSession final : public QObject {
    Q_OBJECT

public:
    enum class State {
        Idle,
        Resolving,
        Ready,
        Error,
    };

    RoomSession(StreamgetProcessClient *client,
                QString roomId,
                StreamQuality userQuality,
                QWidget *surfaceParent,
                QObject *parent = nullptr);
    ~RoomSession() override;

    QString roomId() const;
    const RoomMetadata &metadata() const noexcept;
    void applyMetadata(const RoomSearchResult &result);
    StreamQuality userQuality() const noexcept;
    bool setRequestedQuality(StreamQuality quality);
    StreamQuality effectiveQuality() const noexcept;
    bool setEffectiveQuality(StreamQuality quality);
    State state() const noexcept;
    RoomLiveStatus liveStatus() const noexcept;
    RoomPlaybackHealth playbackHealth() const noexcept;
    bool isFavorite() const noexcept;
    bool setFavorite(bool favorite);
    bool isAudioFocused() const noexcept;
    bool setAudioFocused(bool focused);
    PlayerSurface *surface() const noexcept;

    quint64 resolve();
    void cancel();
    void stop();
    void release();

signals:
    void sourceReady();
    void failed(QString errorCode);
    void stateChanged(RoomSession::State state);
    void qualityChanged(StreamQuality quality);

private slots:
    void onControllerSourceReady(MediaSource source);
    void onControllerFailed(QString errorCode);
    void onControllerStateChanged(RemotePlaybackController::State state);

private:
    void setState(State state);
    void setLiveStatus(RoomLiveStatus status);
    void setPlaybackHealth(RoomPlaybackHealth health);

    QString roomId_;
    RoomMetadata metadata_;
    StreamQuality userQuality_ = StreamQuality::Auto;
    StreamQuality effectiveQuality_ = StreamQuality::Auto;
    State state_ = State::Idle;
    RoomLiveStatus liveStatus_ = RoomLiveStatus::Unknown;
    RoomPlaybackHealth playbackHealth_ = RoomPlaybackHealth::Pending;
    bool favorite_ = false;
    bool audioFocused_ = false;
    RemotePlaybackController *controller_ = nullptr;
    PlayerSurface *surface_ = nullptr;
};

Q_DECLARE_METATYPE(RoomSession::State)
