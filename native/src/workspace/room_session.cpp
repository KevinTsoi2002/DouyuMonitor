#include "workspace/room_session.h"

#include "media/remote_playback_controller.h"
#include "service/streamget_process_client.h"

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

StreamQuality RoomSession::userQuality() const noexcept
{
    return userQuality_;
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
    setState(State::Ready);
    emit sourceReady();
}

void RoomSession::onControllerFailed(QString errorCode)
{
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

void RoomSession::setState(State state)
{
    if (state_ == state) return;
    state_ = state;
    emit stateChanged(state_);
}
