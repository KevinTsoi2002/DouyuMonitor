#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantList>

#include <optional>

#include "media/remote_playback_controller.h"
#include "service/stream_service_protocol.h"
#include "workspace/native_workspace_types.h"

class StreamgetProcessClient;
class MpvQuickItem;

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
                QObject *parent = nullptr,
                RoomMetadata metadata = {});
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
    int volume() const noexcept;
    bool setVolume(int volume);
    bool attachPlayer(MpvQuickItem *player);
    void detachPlayer(MpvQuickItem *player);
    MpvQuickItem *player() const noexcept;
    QVariantList availableQualities() const;

    quint64 resolve();
    void cancel();
    void stop();
    void release();

#ifdef DOUYU_TESTING
    bool hasPendingSourceForTest() const noexcept;
#endif

signals:
    void sourceReady();
    void variantsChanged();
    void failed(QString errorCode);
    void stateChanged(RoomSession::State state);
    void liveStatusChanged(RoomLiveStatus status);
    void qualityChanged(StreamQuality quality);

private slots:
    void onControllerVariantsReady(QVector<StreamVariant> variants);
    void onControllerSourceReady(MediaSource source);
    void onControllerFailed(QString errorCode);
    void onControllerStateChanged(RemotePlaybackController::State state);
    void onSurfacePlaybackFailed();

private:
    bool startPendingSource();
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
    int volume_ = 100;
    RemotePlaybackController *controller_ = nullptr;
    QPointer<MpvQuickItem> player_;
    std::optional<MediaSource> pendingSource_;
    std::optional<MediaSource> activeSource_;
    QVariantList availableQualities_;
};

Q_DECLARE_METATYPE(RoomSession::State)
