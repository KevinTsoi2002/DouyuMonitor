#pragma once

#include <QObject>
#include <QString>

#include "media/remote_playback_controller.h"
#include "media/player_surface.h"
#include "service/stream_service_protocol.h"

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
    StreamQuality userQuality() const noexcept;
    StreamQuality effectiveQuality() const noexcept;
    bool setEffectiveQuality(StreamQuality quality);
    State state() const noexcept;
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

    QString roomId_;
    StreamQuality userQuality_ = StreamQuality::Auto;
    StreamQuality effectiveQuality_ = StreamQuality::Auto;
    State state_ = State::Idle;
    RemotePlaybackController *controller_ = nullptr;
    PlayerSurface *surface_ = nullptr;
};

Q_DECLARE_METATYPE(RoomSession::State)
