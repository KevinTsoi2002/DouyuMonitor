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
}

MultiRoomCoordinator::~MultiRoomCoordinator()
{
    const auto sessions = sessions_.values();
    for (RoomSession *session : sessions) {
        if (session != nullptr) {
            session->release();
            delete session;
        }
    }
    sessions_.clear();
    order_.clear();
}

bool MultiRoomCoordinator::addRoom(const QString &roomId, StreamQuality userQuality)
{
    if (!isValidRoomId(roomId) || sessions_.contains(roomId)
        || order_.size() >= kMaxRooms || client_ == nullptr || surfaceParent_ == nullptr) {
        return false;
    }

    auto *session = new RoomSession(client_, roomId, userQuality, surfaceParent_, this);
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
    return true;
}

bool MultiRoomCoordinator::removeRoom(const QString &roomId)
{
    auto it = sessions_.find(roomId);
    if (it == sessions_.end()) return false;

    RoomSession *session = it.value();
    if (session != nullptr) session->release();
    sessions_.erase(it);
    order_.removeAll(roomId);

    if (primaryRoomId_ == roomId) {
        primaryRoomId_ = order_.isEmpty() ? QString() : order_.front();
    }

    const QString previousLayout = layoutId_;
    layoutId_ = recommendedGridId(order_.size());
    recomputeQuality();
    delete session;

    emit roomRemoved(roomId);
    if (previousLayout != layoutId_) emit layoutChanged(layoutId_);
    return true;
}

bool MultiRoomCoordinator::setPrimaryRoom(const QString &roomId)
{
    if (!sessions_.contains(roomId) || primaryRoomId_ == roomId) return false;
    primaryRoomId_ = roomId;
    recomputeQuality();
    return true;
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

void MultiRoomCoordinator::connectSession(RoomSession *session)
{
    if (session == nullptr) return;
    connect(session, &RoomSession::stateChanged, this,
            [this, session](RoomSession::State) {
                emit roomStateChanged(session->roomId());
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
