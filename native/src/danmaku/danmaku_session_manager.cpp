#include "danmaku/danmaku_session_manager.h"

#include <QDateTime>

namespace {

constexpr int kMaxSessions = 9;
constexpr int kMaxSeenIds = 200;
constexpr int kMaxPendingMessages = 100;

} // namespace

DanmakuSessionManager::DanmakuSessionManager(DanmakuClientFactory factory, QObject *parent)
    : QObject(parent)
    , factory_(std::move(factory))
{
}

void DanmakuSessionManager::synchronize(const QVector<DanmakuRoomEligibility> &rooms)
{
    QSet<QString> eligibleIds;
    for (const DanmakuRoomEligibility &room : rooms) {
        statuses_[room.roomId] = statuses_.value(
            room.roomId, DanmakuConnectionStatus{room.roomId});
        if (isEligible(room)) eligibleIds.insert(room.roomId);
    }

    const auto existingIds = sessions_.keys();
    for (const QString &roomId : existingIds) {
        if (!eligibleIds.contains(roomId)) removeSession(roomId);
    }

    int sessionCount = sessions_.size();
    for (const DanmakuRoomEligibility &room : rooms) {
        if (!isEligible(room)) {
            statuses_[room.roomId] = DanmakuConnectionStatus{room.roomId};
            continue;
        }
        auto existing = sessions_.find(room.roomId);
        if (existing != sessions_.end()) {
            if (!sameGovernance(existing.value()->governance, room.governance)) {
                existing.value()->governance = DanmakuGovernance::validatedGovernanceSettings(
                    room.governance);
                existing.value()->runtime = DanmakuGovernance::createRuntime();
                existing.value()->queue.clear();
            }
            continue;
        }
        if (sessionCount >= kMaxSessions || !factory_) {
            statuses_[room.roomId] = DanmakuConnectionStatus{room.roomId};
            continue;
        }

        auto client = factory_(room.roomId, this);
        if (!client) {
            statuses_[room.roomId] = DanmakuConnectionStatus{room.roomId};
            continue;
        }
        auto session = std::make_shared<Session>();
        session->governance = DanmakuGovernance::validatedGovernanceSettings(room.governance);
        session->status.roomId = room.roomId;
        session->client = std::move(client);
        auto it = sessions_.insert(room.roomId, session);
        if (it == sessions_.end()) continue;
        ++sessionCount;
        attachSessionSignals(room.roomId, *it.value());
        it.value()->client->start();
    }
}

void DanmakuSessionManager::retry(const QString &roomId)
{
    auto it = sessions_.find(roomId);
    if (it == sessions_.end()) return;
    it.value()->client->retry();
}

std::optional<DanmakuMessage> DanmakuSessionManager::takeNextMessage(const QString &roomId)
{
    auto it = sessions_.find(roomId);
    if (it == sessions_.end() || it.value()->queue.isEmpty()) return std::nullopt;
    return it.value()->queue.dequeue();
}

int DanmakuSessionManager::pendingCount(const QString &roomId) const
{
    const auto it = sessions_.constFind(roomId);
    return it == sessions_.cend() ? 0 : it.value()->queue.size();
}

int DanmakuSessionManager::activeSessionCount() const
{
    return sessions_.size();
}

DanmakuConnectionStatus DanmakuSessionManager::statusForRoom(const QString &roomId) const
{
    return statuses_.value(roomId, DanmakuConnectionStatus{roomId});
}

DanmakuGovernanceStats DanmakuSessionManager::statsForRoom(const QString &roomId) const
{
    const auto it = sessions_.constFind(roomId);
    return it == sessions_.cend() ? DanmakuGovernanceStats{} : it.value()->runtime.stats;
}

void DanmakuSessionManager::clearStats(const QString &roomId)
{
    auto it = sessions_.find(roomId);
    if (it == sessions_.end()) return;
    it.value()->runtime = DanmakuGovernance::createRuntime();
    emit roomStateChanged(roomId);
}

void DanmakuSessionManager::stopAll()
{
    const auto ids = sessions_.keys();
    for (const QString &roomId : ids) {
        auto it = sessions_.find(roomId);
        if (it == sessions_.end()) continue;
        it.value()->client->stop();
        it.value()->queue.clear();
        statuses_[roomId] = DanmakuConnectionStatus{roomId};
        emit roomStateChanged(roomId);
    }
    sessions_.clear();
}

void DanmakuSessionManager::clearRoom(const QString &roomId)
{
    removeSession(roomId);
}

bool DanmakuSessionManager::sameGovernance(const DanmakuGovernanceSettings &left,
                                            const DanmakuGovernanceSettings &right)
{
    return left.enabled == right.enabled
        && left.keywordBlacklist == right.keywordBlacklist
        && left.duplicateWindowSeconds == right.duplicateWindowSeconds
        && left.peakProtectionEnabled == right.peakProtectionEnabled;
}

bool DanmakuSessionManager::isEligible(const DanmakuRoomEligibility &room)
{
    return room.active && room.roomEnabled && room.globalEnabled && room.live;
}

void DanmakuSessionManager::attachSessionSignals(const QString &roomId, Session &session)
{
    connect(session.client.get(), &DanmakuClient::statusChanged, this,
            [this, roomId](const DanmakuConnectionStatus &status) {
                auto it = sessions_.find(roomId);
                if (it == sessions_.end()) return;
                it.value()->status = status;
                statuses_[roomId] = status;
                emit roomStateChanged(roomId);
            });
    connect(session.client.get(), &DanmakuClient::chatReceived, this,
            [this](const QString &incomingRoomId, const QMap<QString, QString> &rawChat) {
                handleChat(incomingRoomId, rawChat);
            });
}

void DanmakuSessionManager::handleChat(const QString &roomId,
                                       const QMap<QString, QString> &rawChat)
{
    auto it = sessions_.find(roomId);
    if (it == sessions_.end()) return;
    const QString id = rawChat.value(QStringLiteral("cid"),
                                     rawChat.value(QStringLiteral("msgid"),
                                                   rawChat.value(QStringLiteral("id"))));
    const auto message = DanmakuGovernance::sanitizeMessage(
        roomId, id, rawChat.value(QStringLiteral("nn")), rawChat.value(QStringLiteral("txt")),
        QDateTime::currentDateTimeUtc());
    if (!message.has_value() || id.isEmpty() || it.value()->seenIds.contains(id)) return;

    it.value()->seenIds.insert(id);
    it.value()->seenOrder.push_back(id);
    if (it.value()->seenOrder.size() > kMaxSeenIds) {
        const QString oldest = it.value()->seenOrder.takeFirst();
        it.value()->seenIds.remove(oldest);
    }

    const bool wasEmpty = it.value()->queue.isEmpty();
    const QVector<DanmakuMessage> accepted = DanmakuGovernance::apply(
        {*message}, it.value()->governance, it.value()->runtime,
        QDateTime::currentDateTimeUtc());
    for (const DanmakuMessage &acceptedMessage : accepted) {
        it.value()->queue.enqueue(acceptedMessage);
    }
    while (it.value()->queue.size() > kMaxPendingMessages) {
        it.value()->queue.dequeue();
        ++it.value()->runtime.stats.queueOverflow;
    }
    if (wasEmpty && !it.value()->queue.isEmpty()) emit messageAvailable(roomId);
}

void DanmakuSessionManager::removeSession(const QString &roomId)
{
    auto it = sessions_.find(roomId);
    if (it == sessions_.end()) return;
    it.value()->client->stop();
    sessions_.erase(it);
    statuses_[roomId] = DanmakuConnectionStatus{roomId};
    emit roomStateChanged(roomId);
}
