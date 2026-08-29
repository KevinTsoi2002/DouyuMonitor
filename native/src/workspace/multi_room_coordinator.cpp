#include "workspace/multi_room_coordinator.h"

#include <QRegularExpression>
#include <QSet>

#include "workspace/quality_policy.h"
#include "workspace/room_session.h"
#include "service/streamget_process_client.h"
#include "ui/mpv_quick_item.h"

namespace {

const QRegularExpression kRoomIdPattern(QStringLiteral(R"(^[0-9]{1,20}$)"));

bool isSupportedLayout(const QString &layoutId)
{
    return layoutId == QStringLiteral("auto") || layoutId == QStringLiteral("single")
        || layoutId == QStringLiteral("grid-2x2") || layoutId == QStringLiteral("grid-3x2")
        || layoutId == QStringLiteral("grid-3x3") || layoutId == QStringLiteral("primary-two")
        || layoutId == QStringLiteral("split-horizontal")
        || layoutId == QStringLiteral("split-vertical");
}

double normalizedRatio(double ratio)
{
    constexpr double kRatios[] = {0.5, 0.6, 0.67};
    double closest = kRatios[0];
    for (double candidate : kRatios) {
        if (qAbs(candidate - ratio) < qAbs(closest - ratio)) closest = candidate;
    }
    return closest;
}

bool isSupportedRatio(double ratio)
{
    return qFuzzyCompare(ratio, 0.5) || qFuzzyCompare(ratio, 0.6)
        || qFuzzyCompare(ratio, 0.67);
}

} // namespace

MultiRoomCoordinator::MultiRoomCoordinator(StreamgetProcessClient *client,
                                           QObject *parent)
    : MultiRoomCoordinator(client, RoomRefreshTiming{}, parent)
{
}

MultiRoomCoordinator::MultiRoomCoordinator(StreamgetProcessClient *client,
                                           RoomRefreshTiming timing,
                                           QObject *parent)
    : QObject(parent)
    , client_(client)
    , scheduler_([this](const QString &roomId) {
                    return client_ != nullptr ? client_->search(roomId) : 0;
                },
                [this](quint64 requestId) {
                    if (client_ != nullptr) client_->cancel(requestId);
                },
                std::move(timing),
                this)
{
    qRegisterMetaType<StreamQuality>();
    qRegisterMetaType<RoomSnapshots>("RoomSnapshots");
    if (client_ != nullptr) {
        connect(client_, &StreamgetProcessClient::responseReceived,
                this, &MultiRoomCoordinator::onResponse);
        connect(client_, &StreamgetProcessClient::requestFailed,
                this, &MultiRoomCoordinator::onRequestFailed);
    }
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
                                                         StreamQuality requestedQuality,
                                                         RoomMetadata metadata)
{
    if (!isValidRoomId(roomId)) return RoomCommandResult::InvalidRoomId;
    if (sessions_.contains(roomId)) return RoomCommandResult::DuplicateRoomId;
    if (order_.size() >= kMaxRooms) return RoomCommandResult::RoomLimitReached;
    if (client_ == nullptr) return RoomCommandResult::Unavailable;

    RoomSession *session = new RoomSession(client_, roomId, requestedQuality, this,
                                           std::move(metadata));
    sessions_.insert(roomId, session);
    order_.append(roomId);
    connectSession(session);
    if (primaryRoomId_.isEmpty()) primaryRoomId_ = roomId;
    if (audioRoomId_.isEmpty() && audioMode_ == QStringLiteral("single")) {
        audioRoomId_ = roomId;
    }

    const QString previousLayout = layoutId_;
    if (layoutMode_ == QStringLiteral("auto")) layoutId_ = recommendedGridId(order_.size());
    recomputeQuality();
    applyAudioFocus();
    if (session->state() == RoomSession::State::Idle) session->resolve();

    emit roomAdded(roomId);
    if (previousLayout != layoutId_) emit layoutChanged(layoutId_);
    scheduler_.synchronize(roomSnapshots());
    scheduler_.requestNow(roomId);
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
    if (audioRoomId_ == roomId) {
        audioRoomId_ = order_.isEmpty() ? QString() : order_.front();
    }

    const QString previousLayout = layoutId_;
    if (layoutMode_ == QStringLiteral("auto")) layoutId_ = recommendedGridId(order_.size());
    recomputeQuality();
    applyAudioFocus();
    delete session;

    emit roomRemoved(roomId);
    if (previousLayout != layoutId_) emit layoutChanged(layoutId_);
    scheduler_.synchronize(roomSnapshots());
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

QString MultiRoomCoordinator::audioMode() const
{
    return audioMode_;
}

bool MultiRoomCoordinator::globalMuted() const noexcept
{
    return globalMuted_;
}

bool MultiRoomCoordinator::setAudioMode(const QString &mode)
{
    const QString normalized = mode.trimmed().toLower();
    if (normalized != QStringLiteral("single") && normalized != QStringLiteral("multi")) {
        return false;
    }
    if (audioMode_ == normalized) return false;
    audioMode_ = normalized;
    applyAudioFocus();
    publishSnapshots();
    return true;
}

bool MultiRoomCoordinator::setGlobalMuted(bool muted)
{
    if (globalMuted_ == muted) return false;
    globalMuted_ = muted;
    applyAudioFocus();
    publishSnapshots();
    return true;
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

RoomCommandResult MultiRoomCoordinator::moveRoomDetailed(const QString &roomId, int delta)
{
    const int fromIndex = order_.indexOf(roomId);
    if (fromIndex < 0) return RoomCommandResult::RoomNotFound;

    const int toIndex = qBound(0, fromIndex + delta, order_.size() - 1);
    if (toIndex == fromIndex) return RoomCommandResult::Unchanged;
    order_.move(fromIndex, toIndex);
    publishSnapshots();
    return RoomCommandResult::Accepted;
}

RoomCommandResult MultiRoomCoordinator::retryRoomDetailed(const QString &roomId)
{
    RoomSession *session = sessionForRoom(roomId);
    if (session == nullptr) return RoomCommandResult::RoomNotFound;
    return session->resolve() > 0 ? RoomCommandResult::Accepted : RoomCommandResult::Unavailable;
}

RoomCommandResult MultiRoomCoordinator::setVolume(const QString &roomId, int volume)
{
    RoomSession *session = sessionForRoom(roomId);
    if (session == nullptr) return RoomCommandResult::RoomNotFound;
    if (!session->setVolume(volume)) return RoomCommandResult::Unchanged;
    publishSnapshots();
    return RoomCommandResult::Accepted;
}

RoomCommandResult MultiRoomCoordinator::replaceRooms(const QVector<CoordinatorRoomSpec> &rooms)
{
    if (rooms.size() > kMaxRooms) return RoomCommandResult::RoomLimitReached;
    if (client_ == nullptr && !rooms.isEmpty()) return RoomCommandResult::Unavailable;

    QSet<QString> seen;
    for (const CoordinatorRoomSpec &spec : rooms) {
        if (!isValidRoomId(spec.roomId)) return RoomCommandResult::InvalidRoomId;
        if (seen.contains(spec.roomId)) return RoomCommandResult::DuplicateRoomId;
        seen.insert(spec.roomId);
    }

    const auto existingSessions = sessions_.values();
    for (RoomSession *session : existingSessions) {
        if (session == nullptr) continue;
        QObject::disconnect(session, nullptr, this, nullptr);
        session->release();
        delete session;
    }
    sessions_.clear();
    order_.clear();
    primaryRoomId_.clear();
    audioRoomId_.clear();

    for (const CoordinatorRoomSpec &spec : rooms) {
        auto *session = new RoomSession(client_, spec.roomId, spec.requestedQuality, this,
                                        spec.metadata);
        session->setFavorite(spec.favorite);
        session->setVolume(spec.volume);
        sessions_.insert(spec.roomId, session);
        order_.append(spec.roomId);
        connectSession(session);
        if (primaryRoomId_.isEmpty()) primaryRoomId_ = spec.roomId;
    }

    if (layoutMode_ == QStringLiteral("auto")) layoutId_ = recommendedGridId(order_.size());
    recomputeQuality();
    applyAudioFocus();
    scheduler_.synchronize(roomSnapshots());
    for (const QString &roomId : order_) {
        RoomSession *session = sessions_.value(roomId, nullptr);
        if (session != nullptr && session->state() == RoomSession::State::Idle) {
            session->resolve();
        }
        scheduler_.requestNow(roomId);
    }
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

QString MultiRoomCoordinator::layoutMode() const
{
    return layoutMode_;
}

double MultiRoomCoordinator::primaryRoomRatio() const noexcept
{
    return primaryRoomRatio_;
}

bool MultiRoomCoordinator::setLayout(const QString &layoutId)
{
    const QString normalized = layoutId.trimmed().toLower();
    if (!isSupportedLayout(normalized)) return false;

    const QString previousLayout = layoutId_;
    const QString previousMode = layoutMode_;
    layoutMode_ = normalized;
    if (normalized == QStringLiteral("auto")) {
        layoutId_ = recommendedGridId(order_.size());
    } else {
        layoutId_ = normalized;
    }
    if (previousLayout != layoutId_) emit layoutChanged(layoutId_);
    return previousLayout != layoutId_ || previousMode != layoutMode_;
}

bool MultiRoomCoordinator::setPrimaryRoomRatio(double ratio)
{
    if (!isSupportedRatio(ratio)) return false;
    const double normalized = normalizedRatio(ratio);
    if (qFuzzyCompare(primaryRoomRatio_, normalized)) return false;
    primaryRoomRatio_ = normalized;
    publishSnapshots();
    return true;
}

RoomSnapshots MultiRoomCoordinator::roomSnapshots() const
{
    RoomSnapshots snapshots;
    snapshots.reserve(order_.size());
    for (const QString &roomId : order_) {
        const RoomSession *session = sessions_.value(roomId, nullptr);
        if (session == nullptr) continue;
        const MpvQuickItem *quickPlayer = session->player();
        const bool audible = !globalMuted_
            && (audioMode_ == QStringLiteral("multi")
                || (!audioRoomId_.isEmpty() && roomId == audioRoomId_));
        const bool muted = quickPlayer != nullptr ? quickPlayer->isMuted() : !audible;
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
                             muted,
                             session->volume(),
                             session->availableQualities()});
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

bool MultiRoomCoordinator::attachPlayer(const QString &roomId, MpvQuickItem *player)
{
    RoomSession *session = sessionForRoom(roomId);
    if (session == nullptr || !session->attachPlayer(player)) return false;
    publishSnapshots();
    return true;
}

void MultiRoomCoordinator::detachPlayer(const QString &roomId, MpvQuickItem *player)
{
    RoomSession *session = sessionForRoom(roomId);
    if (session == nullptr) return;
    session->detachPlayer(player);
    publishSnapshots();
}

MpvQuickItem *MultiRoomCoordinator::playerForRoom(const QString &roomId) const noexcept
{
    RoomSession *session = sessionForRoom(roomId);
    return session != nullptr ? session->player() : nullptr;
}

void MultiRoomCoordinator::refreshRoomStatusNow(const QString &roomId)
{
    if (!sessions_.contains(roomId)) return;
    scheduler_.requestNow(roomId);
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
        const bool audible = !globalMuted_
            && (audioMode_ == QStringLiteral("multi")
                || (!audioRoomId_.isEmpty() && roomId == audioRoomId_));
        if (session->player() != nullptr) session->player()->setMuted(!audible);
        const bool focused = !audioRoomId_.isEmpty() && roomId == audioRoomId_;
        session->setAudioFocused(focused);
    }
}

void MultiRoomCoordinator::publishSnapshots()
{
    emit roomSnapshotsChanged(roomSnapshots());
}

void MultiRoomCoordinator::onResponse(ServiceResponse response)
{
    const auto requestedRoomId = scheduler_.roomForRequest(response.requestId);
    if (!requestedRoomId.has_value()) return;

    const bool validSearch = response.ok
        && response.search
        && response.results.size() == 1
        && response.results.first().roomId == *requestedRoomId;
    const auto completedRoomId = scheduler_.takeCompletedRequest(response.requestId, validSearch);
    if (!completedRoomId.has_value() || !validSearch) return;

    RoomSession *session = sessionForRoom(*completedRoomId);
    if (session == nullptr) return;

    const RoomLiveStatus previousStatus = session->liveStatus();
    session->applyMetadata(response.results.first());
    const RoomLiveStatus currentStatus = session->liveStatus();
    emit roomStatusRefreshed(*completedRoomId, currentStatus == RoomLiveStatus::Online);
    if (currentStatus == RoomLiveStatus::Online && previousStatus != RoomLiveStatus::Online
        && session->state() == RoomSession::State::Idle) {
        session->resolve();
    } else if (currentStatus == RoomLiveStatus::Offline
               && previousStatus != RoomLiveStatus::Offline) {
        session->stop();
    }
    scheduler_.synchronize(roomSnapshots());
    publishSnapshots();
}

void MultiRoomCoordinator::onRequestFailed(quint64 requestId, QString)
{
    scheduler_.takeCompletedRequest(requestId, false);
}

void MultiRoomCoordinator::connectSession(RoomSession *session)
{
    if (session == nullptr) return;
    connect(session, &RoomSession::stateChanged, this,
            [this, session](RoomSession::State) {
                emit roomStateChanged(session->roomId());
                publishSnapshots();
            });
    connect(session, &RoomSession::liveStatusChanged, this,
            [this, session](RoomLiveStatus) {
                emit roomStateChanged(session->roomId());
                publishSnapshots();
            });
    connect(session, &RoomSession::variantsChanged, this,
            [this, session]() {
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
