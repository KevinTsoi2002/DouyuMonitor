#include "workspace/room_session.h"

#include "media/remote_playback_controller.h"
#include "service/streamget_process_client.h"
#include "ui/mpv_quick_item.h"

namespace {

QString qualityToken(StreamQuality quality)
{
    switch (quality) {
    case StreamQuality::Auto: return QStringLiteral("auto");
    case StreamQuality::Original: return QStringLiteral("original");
    case StreamQuality::Super: return QStringLiteral("super");
    case StreamQuality::High: return QStringLiteral("high");
    case StreamQuality::Standard: return QStringLiteral("standard");
    }
    return {};
}

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
                         QObject *parent,
                         RoomMetadata metadata)
    : QObject(parent)
    , roomId_(std::move(roomId))
    , userQuality_(userQuality)
    , effectiveQuality_(userQuality)
    , metadata_(std::move(metadata))
    , controller_(new RemotePlaybackController(client, this))
{
    metadata_.roomId = roomId_;
    qRegisterMetaType<RoomSession::State>();
    connect(controller_, &RemotePlaybackController::sourceReady,
            this, &RoomSession::onControllerSourceReady);
    connect(controller_, &RemotePlaybackController::variantsReady,
            this, &RoomSession::onControllerVariantsReady);
    connect(controller_, &RemotePlaybackController::failed,
            this, &RoomSession::onControllerFailed);
    connect(controller_, &RemotePlaybackController::stateChanged,
            this, &RoomSession::onControllerStateChanged);
}

RoomSession::~RoomSession()
{
    release();
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
    if (result.roomId != roomId_ || result.anchorName.isEmpty()) return;

    metadata_.roomId = result.roomId;
    metadata_.anchorName = result.anchorName;
    if (!result.title.isEmpty()) metadata_.title = result.title;
    if (!result.category.isEmpty()) metadata_.category = result.category;
    if (!result.viewerLabel.isEmpty()) metadata_.viewerLabel = result.viewerLabel;
    if (!result.avatarUrl.isEmpty()) {
        metadata_.avatarUrl = isSafeHttpUrl(result.avatarUrl) ? result.avatarUrl : QUrl();
    }
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

int RoomSession::volume() const noexcept
{
    return volume_;
}

bool RoomSession::setVolume(int volume)
{
    if (volume < 0 || volume > 100 || volume_ == volume) return false;
    if (player_ != nullptr && !player_->setVolume(volume)) return false;
    volume_ = volume;
    return true;
}

bool RoomSession::attachPlayer(MpvQuickItem *player)
{
    if (player == nullptr || player_ == player) return false;
    if (player_ != nullptr) detachPlayer(player_);

    player_ = player;
    connect(player_, &MpvQuickItem::playbackFailed,
            this, &RoomSession::onSurfacePlaybackFailed);

    if (!player_->setMuted(!audioFocused_) || !player_->setVolume(volume_)) {
        detachPlayer(player);
        return false;
    }
    if (pendingSource_.has_value()) return startPendingSource();
    if (!activeSource_.has_value()) return true;
    if (player_->loadSource(*activeSource_)) return true;

    onControllerFailed(QStringLiteral("PLAYER_FAILED"));
    return false;
}

void RoomSession::detachPlayer(MpvQuickItem *player)
{
    if (player == nullptr || player_ != player) return;
    QObject::disconnect(player, nullptr, this, nullptr);
    player_.clear();
}

MpvQuickItem *RoomSession::player() const noexcept
{
    return player_;
}

QVariantList RoomSession::availableQualities() const
{
    return availableQualities_;
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
    pendingSource_.reset();
    activeSource_.reset();
    setState(State::Idle);
}

void RoomSession::stop()
{
    if (controller_ != nullptr) controller_->stop();
    if (player_ != nullptr) player_->release();
    pendingSource_.reset();
    activeSource_.reset();
    setPlaybackHealth(RoomPlaybackHealth::Pending);
    setState(State::Idle);
}

void RoomSession::release()
{
    if (controller_ != nullptr) controller_->release();
    // The QML scene owns the player. Releasing its libmpv core from the
    // session can race the scene graph while the delegate is being removed.
    if (player_ != nullptr) detachPlayer(player_);
    pendingSource_.reset();
    activeSource_.reset();
    setState(State::Idle);
}

void RoomSession::onControllerSourceReady(MediaSource source)
{
    pendingSource_ = std::move(source);
    if (player_ != nullptr) {
        startPendingSource();
        return;
    }

    setLiveStatus(RoomLiveStatus::Online);
    setPlaybackHealth(RoomPlaybackHealth::Pending);
}

void RoomSession::onControllerVariantsReady(QVector<StreamVariant> variants)
{
    QVariantList options;
    options.reserve(variants.size());
    for (const StreamVariant &variant : variants) {
        options.push_back(QVariantMap{
            {QStringLiteral("id"), variant.id},
            {QStringLiteral("label"), variant.label},
            {QStringLiteral("quality"), qualityToken(variant.quality)},
        });
    }
    if (availableQualities_ == options) return;
    availableQualities_ = std::move(options);
    emit variantsChanged();
}

bool RoomSession::startPendingSource()
{
    if (player_ == nullptr || !pendingSource_.has_value()) return false;
    if (!player_->loadSource(*pendingSource_)) {
        onControllerFailed(QStringLiteral("PLAYER_FAILED"));
        return false;
    }
    activeSource_ = pendingSource_;
    pendingSource_.reset();
    setLiveStatus(RoomLiveStatus::Online);
    setPlaybackHealth(RoomPlaybackHealth::Playing);
    setState(State::Ready);
    emit sourceReady();
    return true;
}

void RoomSession::onControllerFailed(QString errorCode)
{
    if (errorCode == QStringLiteral("ROOM_OFFLINE")) {
        if (player_ != nullptr) player_->stop();
        pendingSource_.reset();
        activeSource_.reset();
        if (liveStatus_ == RoomLiveStatus::Unknown) {
            setLiveStatus(RoomLiveStatus::Offline);
        }
        setPlaybackHealth(RoomPlaybackHealth::Pending);
        setState(State::Idle);
        return;
    }
    pendingSource_.reset();
    activeSource_.reset();
    setPlaybackHealth(RoomPlaybackHealth::Error);
    setState(State::Error);
    emit failed(std::move(errorCode));
}

#ifdef DOUYU_TESTING
bool RoomSession::hasPendingSourceForTest() const noexcept
{
    return pendingSource_.has_value();
}
#endif

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
    if (liveStatus_ == status) return;
    liveStatus_ = status;
    emit liveStatusChanged(status);
}

void RoomSession::setPlaybackHealth(RoomPlaybackHealth health)
{
    playbackHealth_ = health;
}
