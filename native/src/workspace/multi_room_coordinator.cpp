#include "workspace/multi_room_coordinator.h"

#include <QRegularExpression>

#include "workspace/quality_policy.h"
#include "workspace/room_session.h"

namespace {

const QRegularExpression kRoomIdPattern(QStringLiteral(R"(^[0-9]{1,20}$)"));

} // namespace

MultiRoomCoordinator::MultiRoomCoordinator(StreamgetProcessClient *client,
                                           QWidget *surfaceParent,
                                           QObject *parent)
    : QObject(parent)
    , client_(client)
    , surfaceParent_(surfaceParent)
{
    qRegisterMetaType<StreamQuality>();
    qRegisterMetaType<RoomSnapshots>("RoomSnapshots");
}

MultiRoomCoordinator::~MultiRoomCoordinator()
{
    const auto sessions = sessions_.values();
    for (RoomSession *session : sessions) {
        if (session != nullptr) {
            QObject::disconnect(session, nullptr, this, nullptr);
            session->release();
            delete session;
        }
    }
    sessions_.clear();
    order_.clear();
}

bool MultiRoomCoordinator::addRoom(const QString &roomId, StreamQuality userQuality)
{
    return addRoomDetailed(roomId, userQuality) == RoomCommandResult::Accepted;
}

RoomCommandResult MultiRoomCoordinator::addRoomDetailed(const QString &roomId,
                                                         StreamQuality requestedQuality)
{
    if (!isValidRoomId(roomId)) return RoomCommandResult::InvalidRoomId;
    if (sessions_.contains(roomId)) return RoomCommandResult::DuplicateRoomId;
    if (order_.size() >= kMaxRooms) return RoomCommandResult::RoomLimitReached;
    if (client_ == nullptr || surfaceParent_ == nullptr) return RoomCommandResult::Unavailable;

    auto *session = new RoomSession(client_, roomId, requestedQuality, surfaceParent_, this);
    sessions_.insert(roomId, session);
    order_.append(roomId);
    connectSession(session);
    if (primaryRoomId_.isEmpty()) primaryRoomId_ = roomId;

    const QString previousLayout = layoutId_;
    layoutId_ = recommendedGridId(order_.size());
    recomputeQuality();
    if (session->state() == RoomSession::State::Idle) session->resolve();

    emit roomAdded(roomId);
    if (previousLayout != layoutId_) emit layoutChanged(layoutId_);
    publishSnapshots();
    return RoomCommandResult::Accepted;
}

bool MultiRoomCoordinator::removeRoom(const QString &roomId)
{
    return removeRoomDetailed(roomId) == RoomCommandResult::Accepted;
}

RoomCommandResult MultiRoomCoordinator::removeRoomDetailed(const QString &roomId)
{
    auto it = sessions_.find(roomId);
    if (it == sessions_.end()) return RoomCommandResult::RoomNotFound;

    RoomSession *session = it.value();
    QObject::disconnect(session, nullptr, this, nullptr);
    if (session != nullptr) session->release();
    sessions_.erase(it);
    order_.removeAll(roomId);

    if (primaryRoomId_ == roomId) {
        primaryRoomId_ = order_.isEmpty() ? QString() : order_.front();
    }
    if (audioRoomId_ == roomId) audioRoomId_.clear();

    const QString previousLayout = layoutId_;
    layoutId_ = recommendedGridId(order_.size());
    recomputeQuality();
    applyAudioFocus();
    delete session;

    emit roomRemoved(roomId);
    if (previousLayout != layoutId_) emit layoutChanged(layoutId_);
    publishSnapshots();
    return RoomCommandResult::Accepted;
}

bool MultiRoomCoordinator::setPrimaryRoom(const QString &roomId)
{
    return setPrimaryRoomDetailed(roomId) == RoomCommandResult::Accepted;
}

RoomCommandResult MultiRoomCoordinator::setPrimaryRoomDetailed(const QString &roomId)
{
    if (!sessions_.contains(roomId)) return RoomCommandResult::RoomNotFound;
    if (primaryRoomId_ == roomId) return RoomCommandResult::AlreadyPrimary;
    primaryRoomId_ = roomId;
    recomputeQuality();
    publishSnapshots();
    return RoomCommandResult::Accepted;
}

bool MultiRoomCoordinator::setAudioFocus(const QString &roomId)
{
    if (!sessions_.contains(roomId)) return false;
    audioRoomId_ = roomId;
    applyAudioFocus();
    publishSnapshots();
    return true;
}

bool MultiRoomCoordinator::setFavorite(const QString &roomId, bool favorite)
{
    RoomSession *session = sessionForRoom(roomId);
    if (session == nullptr || !session->setFavorite(favorite)) return false;
    publishSnapshots();
    return true;
}

QString MultiRoomCoordinator::audioRoomId() const
{
    return audioRoomId_;
}

RoomCommandResult MultiRoomCoordinator::setRequestedQuality(const QString &roomId,
                                                             StreamQuality requestedQuality)
{
    RoomSession *session = sessionForRoom(roomId);
    if (session == nullptr) return RoomCommandResult::RoomNotFound;
    if (!session->setRequestedQuality(requestedQuality)) return RoomCommandResult::Unchanged;
    recomputeQuality();
    publishSnapshots();
    return RoomCommandResult::Accepted;
}

int MultiRoomCoordinator::roomCount() const noexcept
{
    return order_.size();
}

QString MultiRoomCoordinator::primaryRoomId() const
{
    return primaryRoomId_;
}

QStringList MultiRoomCoordinator::roomIds() const
{
    return QStringList(order_.cbegin(), order_.cend());
}

QString MultiRoomCoordinator::layoutId() const
{
    return layoutId_;
}

RoomSnapshots MultiRoomCoordinator::roomSnapshots() const
{
    RoomSnapshots snapshots;
    snapshots.reserve(order_.size());
    for (const QString &roomId : order_) {
        const RoomSession *session = sessions_.value(roomId, nullptr);
        if (session == nullptr) continue;
        snapshots.push_back({roomId,
                             roomId == primaryRoomId_,
                             session->state(),
                             session->userQuality(),
                             session->effectiveQuality(),
                             session->metadata(),
                             session->liveStatus(),
                             session->playbackHealth(),
                             session->isFavorite(),
                             session->isAudioFocused(),
                             session->surface() == nullptr || session->surface()->isMuted()});
    }
    return snapshots;
}

StreamQuality MultiRoomCoordinator::userQuality(const QString &roomId) const noexcept
{
    const RoomSession *session = sessionForRoom(roomId);
    return session != nullptr ? session->userQuality() : StreamQuality::Auto;
}

StreamQuality MultiRoomCoordinator::effectiveQuality(const QString &roomId) const noexcept
{
    const RoomSession *session = sessionForRoom(roomId);
    return session != nullptr ? session->effectiveQuality() : StreamQuality::Auto;
}

RoomSession *MultiRoomCoordinator::sessionForRoom(const QString &roomId) const noexcept
{
    return sessions_.value(roomId, nullptr);
}

PlayerSurface *MultiRoomCoordinator::surfaceForRoom(const QString &roomId) const noexcept
{
    RoomSession *session = sessionForRoom(roomId);
    return session != nullptr ? session->surface() : nullptr;
}

void MultiRoomCoordinator::recomputeQuality()
{
    const int count = order_.size();
    for (const QString &roomId : order_) {
        RoomSession *session = sessions_.value(roomId, nullptr);
        if (session == nullptr) continue;
        const auto decision = resolveRoomQuality(count, roomId == primaryRoomId_,
                                                 session->userQuality());
        if (!session->setEffectiveQuality(decision.effectiveQuality)) continue;
        emit qualityChanged(roomId, decision.effectiveQuality);
        session->resolve();
    }
}

void MultiRoomCoordinator::applyAudioFocus()
{
    for (const QString &roomId : order_) {
        RoomSession *session = sessions_.value(roomId, nullptr);
        if (session == nullptr) continue;
        const bool focused = !audioRoomId_.isEmpty() && roomId == audioRoomId_;
        if (session->surface() != nullptr) session->surface()->setMuted(!focused);
        session->setAudioFocused(focused);
    }
}

void MultiRoomCoordinator::publishSnapshots()
{
    emit roomSnapshotsChanged(roomSnapshots());
}

void MultiRoomCoordinator::connectSession(RoomSession *session)
{
    if (session == nullptr) return;
    connect(session, &RoomSession::stateChanged, this,
            [this, session](RoomSession::State) {
                emit roomStateChanged(session->roomId());
                publishSnapshots();
            });
    connect(session, &RoomSession::failed, this,
            [this, session](const QString &errorCode) {
                emit failed(session->roomId(), errorCode);
            });
}

bool MultiRoomCoordinator::isValidRoomId(const QString &roomId)
{
    return kRoomIdPattern.match(roomId).hasMatch();
}
