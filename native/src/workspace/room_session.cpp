#include "workspace/room_session.h"

#include "media/remote_playback_controller.h"
#include "service/streamget_process_client.h"

namespace {

bool isSafeHttpUrl(const QUrl &url)
{
    return url.isValid()
        && (url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https"))
        && !url.host().isEmpty() && url.userName().isEmpty() && url.password().isEmpty();
}

} // namespace

RoomSession::RoomSession(StreamgetProcessClient *client,
                         QString roomId,
                         StreamQuality userQuality,
                         QWidget *surfaceParent,
                         QObject *parent)
    : QObject(parent)
    , roomId_(std::move(roomId))
    , userQuality_(userQuality)
    , effectiveQuality_(userQuality)
    , controller_(new RemotePlaybackController(client, this))
    , surface_(new PlayerSurface(nullptr))
{
    Q_UNUSED(surfaceParent)
    qRegisterMetaType<RoomSession::State>();
    connect(controller_, &RemotePlaybackController::sourceReady,
            this, &RoomSession::onControllerSourceReady);
    connect(controller_, &RemotePlaybackController::failed,
            this, &RoomSession::onControllerFailed);
    connect(controller_, &RemotePlaybackController::stateChanged,
            this, &RoomSession::onControllerStateChanged);
    connect(surface_, &PlayerSurface::playbackFailed,
            this, &RoomSession::onSurfacePlaybackFailed);
}

RoomSession::~RoomSession()
{
    release();
    if (surface_ != nullptr) surface_->setParent(nullptr);
    delete surface_;
    surface_ = nullptr;
}

QString RoomSession::roomId() const
{
    return roomId_;
}

const RoomMetadata &RoomSession::metadata() const noexcept
{
    return metadata_;
}

void RoomSession::applyMetadata(const RoomSearchResult &result)
{
    if (result.roomId != roomId_ || result.anchorName.isEmpty() || result.title.isEmpty()
        || result.category.isEmpty() || result.viewerLabel.isEmpty()) {
        return;
    }
    metadata_ = {result.roomId, result.anchorName, result.title, result.category,
                 result.viewerLabel, isSafeHttpUrl(result.avatarUrl) ? result.avatarUrl : QUrl()};
    setLiveStatus(result.online ? RoomLiveStatus::Online : RoomLiveStatus::Offline);
}

StreamQuality RoomSession::userQuality() const noexcept
{
    return userQuality_;
}

bool RoomSession::setRequestedQuality(StreamQuality quality)
{
    if (userQuality_ == quality) return false;
    userQuality_ = quality;
    return true;
}

StreamQuality RoomSession::effectiveQuality() const noexcept
{
    return effectiveQuality_;
}

bool RoomSession::setEffectiveQuality(StreamQuality quality)
{
    if (effectiveQuality_ == quality) return false;
    effectiveQuality_ = quality;
    emit qualityChanged(quality);
    return true;
}

RoomSession::State RoomSession::state() const noexcept
{
    return state_;
}

RoomLiveStatus RoomSession::liveStatus() const noexcept
{
    return liveStatus_;
}

RoomPlaybackHealth RoomSession::playbackHealth() const noexcept
{
    return playbackHealth_;
}

bool RoomSession::isFavorite() const noexcept
{
    return favorite_;
}

bool RoomSession::setFavorite(bool favorite)
{
    if (favorite_ == favorite) return false;
    favorite_ = favorite;
    return true;
}

bool RoomSession::isAudioFocused() const noexcept
{
    return audioFocused_;
}

bool RoomSession::setAudioFocused(bool focused)
{
    if (audioFocused_ == focused) return false;
    audioFocused_ = focused;
    return true;
}

PlayerSurface *RoomSession::surface() const noexcept
{
    return surface_;
}

quint64 RoomSession::resolve()
{
    if (controller_ == nullptr) return 0;
    return controller_->resolve(roomId_, effectiveQuality_);
}

void RoomSession::cancel()
{
    if (controller_ == nullptr) return;
    controller_->cancel();
    setState(State::Idle);
}

void RoomSession::stop()
{
    if (controller_ != nullptr) controller_->stop();
    if (surface_ != nullptr) surface_->stop();
    setState(State::Idle);
}

void RoomSession::release()
{
    if (controller_ != nullptr) controller_->release();
    if (surface_ != nullptr) surface_->release();
    setState(State::Idle);
}

void RoomSession::onControllerSourceReady(MediaSource source)
{
    if (surface_ == nullptr || !surface_->loadSource(source)) {
        onControllerFailed(QStringLiteral("PLAYER_FAILED"));
        return;
    }
    setLiveStatus(RoomLiveStatus::Online);
    setPlaybackHealth(RoomPlaybackHealth::Playing);
    setState(State::Ready);
    emit sourceReady();
}

void RoomSession::onControllerFailed(QString errorCode)
{
    if (errorCode == QStringLiteral("ROOM_OFFLINE")) {
        if (surface_ != nullptr) surface_->stop();
        setLiveStatus(RoomLiveStatus::Offline);
        setPlaybackHealth(RoomPlaybackHealth::Pending);
        setState(State::Idle);
        return;
    }
    setPlaybackHealth(RoomPlaybackHealth::Error);
    setState(State::Error);
    emit failed(std::move(errorCode));
}

void RoomSession::onControllerStateChanged(RemotePlaybackController::State state)
{
    if (state == RemotePlaybackController::State::Resolving) {
        setState(State::Resolving);
    } else if (state == RemotePlaybackController::State::Idle
               && state_ != State::Error) {
        setState(State::Idle);
    }
}

void RoomSession::onSurfacePlaybackFailed()
{
    if (state_ == State::Idle || liveStatus_ == RoomLiveStatus::Offline) return;
    setPlaybackHealth(RoomPlaybackHealth::Error);
    setState(State::Error);
    emit failed(QStringLiteral("PLAYER_FAILED"));
}

void RoomSession::setState(State state)
{
    if (state_ == state) return;
    state_ = state;
    emit stateChanged(state_);
}

void RoomSession::setLiveStatus(RoomLiveStatus status)
{
    liveStatus_ = status;
}

void RoomSession::setPlaybackHealth(RoomPlaybackHealth health)
{
    playbackHealth_ = health;
}
