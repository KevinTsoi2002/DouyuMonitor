#include "workspace/guild_room_resolver.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QTimer>

#include <algorithm>

namespace {

constexpr int kRequestSpacingMs = 1200;
constexpr int kLiveStatusSpacingMs = 100;
constexpr int kRetryDelaysMs[] = {3000, 15000, 75000};
// Live status is cached per member so that toggling the navigation panel does
// not enqueue another 50+ sequential status requests.
constexpr qint64 kLiveStatusFreshMs = 15 * 60 * 1000;

const QRegularExpression kRoomIdPattern(QStringLiteral(R"(^[0-9]{1,20}$)"));

int retryDelayForFailureCount(int failureCount)
{
    const int index = qBound(0, failureCount - 1,
                             static_cast<int>(std::size(kRetryDelaysMs)) - 1);
    return kRetryDelaysMs[index];
}

bool sameName(const QString &left, const QString &right)
{
    return GuildRoster::normalizedName(left)
               .compare(GuildRoster::normalizedName(right), Qt::CaseSensitive)
        == 0;
}

} // namespace

GuildRoomResolver::GuildRoomResolver(SearchTransport *transport, QObject *parent)
    : QObject(parent)
    , transport_(transport)
    , scheduleTimer_(new QTimer(this))
{
    scheduleTimer_->setSingleShot(true);
    connect(scheduleTimer_, &QTimer::timeout, this, &GuildRoomResolver::sendNextRequest);
}

GuildRoomResolver::~GuildRoomResolver()
{
    stop();
}

void GuildRoomResolver::setRoster(QVector<GuildMember> roster)
{
    stop();
    members_.clear();
    roomIds_.clear();
    statuses_.clear();
    avatarUrls_.clear();
    liveStates_.clear();
    failures_.clear();
    cache_.clear();
    cachedMemberIds_.clear();
    queue_.clear();
    queuedMembers_.clear();
    metadataQueue_.clear();
    queuedMetadataMembers_.clear();
    activeRequestIsMetadataRefresh_ = false;
    activeRequestStatusOnly_ = false;

    for (const GuildMember &member : roster) {
        if (member.id.isEmpty() || member.anchorName.isEmpty()) continue;
        members_.insert(member.id, member);
    }
    for (const QString &memberId : members_.keys()) {
        const GuildMember &member = members_.value(memberId);
        if (isValidRoomId(member.roomId)) {
            roomIds_.insert(memberId, member.roomId);
            statuses_.insert(memberId, QStringLiteral("resolved"));
        } else {
            statuses_.insert(memberId, QStringLiteral("unconfirmed"));
        }
    }
}

void GuildRoomResolver::setCache(QVector<GuildRoomCacheEntry> cache)
{
    stop();
    cache_.clear();
    roomIds_.clear();
    statuses_.clear();
    avatarUrls_.clear();
    liveStates_.clear();
    failures_.clear();
    cachedMemberIds_.clear();
    queue_.clear();
    queuedMembers_.clear();
    metadataQueue_.clear();
    queuedMetadataMembers_.clear();
    activeRequestIsMetadataRefresh_ = false;
    activeRequestStatusOnly_ = false;

    for (const QString &memberId : members_.keys()) {
        const GuildMember &member = members_.value(memberId);
        if (isValidRoomId(member.roomId)) {
            roomIds_.insert(memberId, member.roomId);
            statuses_.insert(memberId, QStringLiteral("resolved"));
        }
    }

    QSet<QString> seen;
    for (GuildRoomCacheEntry entry : cache) {
        entry.memberId = entry.memberId.trimmed();
        entry.roomId = entry.roomId.trimmed();
        entry.anchorName = entry.anchorName.trimmed();
        const auto member = members_.constFind(entry.memberId);
        if (member == members_.cend() || seen.contains(entry.memberId)
            || !isValidRoomId(entry.roomId) || entry.verifiedAtMs <= 0) {
            continue;
        }
        entry.anchorName = member->anchorName;
        seen.insert(entry.memberId);
        roomIds_.insert(entry.memberId, entry.roomId);
        statuses_.insert(entry.memberId, QStringLiteral("resolved"));
        if (!entry.avatarUrl.isEmpty()) {
            avatarUrls_.insert(entry.memberId, entry.avatarUrl.toString());
        }
        liveStates_.insert(entry.memberId, entry.liveState);
        cachedMemberIds_.insert(entry.memberId);
        cache_.push_back(std::move(entry));
    }
    for (const QString &memberId : members_.keys()) {
        if (!statuses_.contains(memberId)) {
            statuses_.insert(memberId, QStringLiteral("unconfirmed"));
        }
    }
    emit cacheChanged();
}

QVector<GuildRoomCacheEntry> GuildRoomResolver::cache() const
{
    return cache_;
}

void GuildRoomResolver::start()
{
    if (started_) return;
    started_ = true;
    if (transport_ == nullptr) return;

    QVector<QString> ids;
    ids.reserve(members_.size());
    for (auto it = members_.cbegin(); it != members_.cend(); ++it) {
        if (!roomIds_.contains(it.key())) ids.push_back(it.key());
    }
    std::sort(ids.begin(), ids.end());
    for (const QString &memberId : ids) {
        if (queuedMembers_.contains(memberId)) continue;
        queue_.push_back(memberId);
        queuedMembers_.insert(memberId);
        statuses_.insert(memberId, QStringLiteral("idle"));
    }
    refreshMetadata();
    if (activeMemberId_.isEmpty() && (!queue_.isEmpty() || !metadataQueue_.isEmpty())) {
        sendNextRequest();
    }
}

void GuildRoomResolver::refreshMetadata(bool forceFullMetadata)
{
    if (!started_) return;
    QStringList memberIds = roomIds_.keys();
    std::sort(memberIds.begin(), memberIds.end());
    for (const QString &memberId : memberIds) refreshMemberMetadata(memberId, forceFullMetadata);
}

void GuildRoomResolver::stop()
{
    started_ = false;
    if (scheduleTimer_ != nullptr) scheduleTimer_->stop();
    const quint64 requestId = activeRequestId_;
    activeRequestId_ = 0;
    activeMemberId_.clear();
    activeRequestIsMetadataRefresh_ = false;
    activeRequestStatusOnly_ = false;
    if (requestId != 0 && transport_ != nullptr) transport_->cancel(requestId);
}

QString GuildRoomResolver::roomIdFor(const QString &memberId) const
{
    return roomIds_.value(memberId);
}

QString GuildRoomResolver::avatarUrlFor(const QString &memberId) const
{
    return avatarUrls_.value(memberId);
}

QString GuildRoomResolver::liveStateFor(const QString &memberId) const
{
    return liveStates_.value(memberId, QStringLiteral("unknown"));
}

QString GuildRoomResolver::statusFor(const QString &memberId) const
{
    return statuses_.value(memberId);
}

QString GuildRoomResolver::setManualRoomId(const QString &memberId, const QString &roomId)
{
    const QString normalizedRoomId = roomId.trimmed();
    const auto member = members_.constFind(memberId);
    if (member == members_.cend()) return QStringLiteral("未找到该公会主播");
    if (!isValidRoomId(normalizedRoomId)) return QStringLiteral("请输入有效房间号");

    removeQueuedMember(memberId);
    if (activeMemberId_ == memberId && activeRequestId_ != 0) {
        const quint64 requestId = activeRequestId_;
        activeRequestId_ = 0;
        activeMemberId_.clear();
        if (transport_ != nullptr) transport_->cancel(requestId);
    }
    failures_.remove(memberId);
    roomIds_.insert(memberId, normalizedRoomId);
    avatarUrls_.remove(memberId);
    liveStates_.insert(memberId, QStringLiteral("unknown"));
    statuses_.insert(memberId, QStringLiteral("resolved"));
    if (started_) enqueueMetadataRefresh(memberId, true);
    storeCacheEntry(memberId, normalizedRoomId);
    emit memberChanged(memberId);
    emit cacheChanged();
    if (started_ && activeMemberId_.isEmpty() && !scheduleTimer_->isActive()
        && (!queue_.isEmpty() || !metadataQueue_.isEmpty())) {
        scheduleNextRequest(kRequestSpacingMs);
    }
    return {};
}

bool GuildRoomResolver::isExactMatch(const GuildMember &member, const RoomSearchResult &result) const
{
    return sameName(member.anchorName, result.anchorName) && isValidRoomId(result.roomId);
}

void GuildRoomResolver::applyMetadata(const QString &memberId, const RoomSearchResult &result)
{
    if (result.avatarUrl.isValid() && !result.avatarUrl.isEmpty()) {
        avatarUrls_.insert(memberId, result.avatarUrl.toString());
    } else {
        avatarUrls_.remove(memberId);
    }
    liveStates_.insert(memberId, result.online ? QStringLiteral("online")
                                                : QStringLiteral("offline"));
}

void GuildRoomResolver::applyLiveStatus(const QString &memberId,
                                        const ServiceResponse &response)
{
    if (!response.status) return;
    const QString liveState = response.isLive ? QStringLiteral("online")
                                               : QStringLiteral("offline");
    liveStates_.insert(memberId, liveState);
    updateCacheLiveState(memberId, liveState);
}

bool GuildRoomResolver::storeCacheEntry(const QString &memberId,
                                        const QString &roomId,
                                        const QUrl &avatarUrl,
                                        bool metadataChecked)
{
    GuildRoomCacheEntry entry;
    entry.memberId = memberId;
    entry.roomId = roomId;
    entry.anchorName = members_.value(memberId).anchorName;
    entry.avatarUrl = avatarUrl;
    entry.metadataCheckedAtMs = metadataChecked ? QDateTime::currentMSecsSinceEpoch() : 0;
    if (liveStates_.contains(memberId)
        && liveStates_.value(memberId) != QStringLiteral("unknown")) {
        entry.liveState = liveStates_.value(memberId);
        entry.liveCheckedAtMs = QDateTime::currentMSecsSinceEpoch();
    }
    entry.verifiedAtMs = QDateTime::currentMSecsSinceEpoch();
    const auto existing = std::find_if(cache_.begin(), cache_.end(),
                                       [&memberId](const GuildRoomCacheEntry &candidate) {
                                           return candidate.memberId == memberId;
                                       });
    if (existing == cache_.end()) {
        cache_.push_back(std::move(entry));
        cachedMemberIds_.insert(memberId);
        return true;
    }
    if (!liveStates_.contains(memberId)
        || liveStates_.value(memberId) == QStringLiteral("unknown")) {
        entry.liveState = existing->liveState;
        entry.liveCheckedAtMs = existing->liveCheckedAtMs;
    }
    *existing = std::move(entry);
    cachedMemberIds_.insert(memberId);
    return false;
}

void GuildRoomResolver::enqueueMetadataRefresh(const QString &memberId, bool forceFullMetadata)
{
    refreshMemberMetadata(memberId, forceFullMetadata);
}

void GuildRoomResolver::refreshMemberMetadata(const QString &memberId, bool forceFullMetadata)
{
    if (!started_ || !members_.contains(memberId) || !roomIds_.contains(memberId)
        || queuedMetadataMembers_.contains(memberId) || activeMemberId_ == memberId) {
        return;
    }
    const auto cacheIt = std::find_if(cache_.begin(), cache_.end(),
                                      [&memberId](const GuildRoomCacheEntry &entry) {
                                          return entry.memberId == memberId;
                                      });
    const bool hasCheckedMetadata = cacheIt != cache_.end()
        && cacheIt->metadataCheckedAtMs > 0;
    const bool liveStateIsFresh = cacheIt != cache_.end()
        && cacheIt->liveCheckedAtMs > 0
        && QDateTime::currentMSecsSinceEpoch() - cacheIt->liveCheckedAtMs < kLiveStatusFreshMs;
    if (liveStateIsFresh && !forceFullMetadata) return;
    metadataQueue_.push_back({
        memberId,
        hasCheckedMetadata && !forceFullMetadata,
    });
    queuedMetadataMembers_.insert(memberId);
    if (activeMemberId_.isEmpty() && !scheduleTimer_->isActive()) {
        scheduleNextRequest(0);
    }
}

bool GuildRoomResolver::handleResponse(const ServiceResponse &response)
{
    if (activeRequestId_ == 0 || response.requestId != activeRequestId_) return false;

    const QString memberId = activeMemberId_;
    const bool metadataRefresh = activeRequestIsMetadataRefresh_;
    const bool statusOnly = activeRequestStatusOnly_;
    activeRequestId_ = 0;
    activeMemberId_.clear();
    activeRequestIsMetadataRefresh_ = false;
    activeRequestStatusOnly_ = false;

    const GuildMember member = members_.value(memberId);
    if (!response.ok || (statusOnly ? !response.status : !response.search)) {
        if (metadataRefresh) {
            scheduleNextRequest(kRequestSpacingMs);
            return true;
        }
        scheduleRetry(memberId);
        return true;
    }

    if (metadataRefresh) {
        if (response.status) {
            applyLiveStatus(memberId, response);
            emit memberChanged(memberId);
            scheduleNextRequest(kLiveStatusSpacingMs);
            return true;
        } else {
            const QString expectedRoomId = roomIds_.value(memberId);
            for (const RoomSearchResult &result : response.results) {
                if (result.roomId != expectedRoomId) continue;
                applyMetadata(memberId, result);
                const QString liveState = liveStates_.value(memberId);
                const auto cacheIt = std::find_if(
                    cache_.begin(), cache_.end(),
                    [&memberId](const GuildRoomCacheEntry &entry) {
                        return entry.memberId == memberId;
                    });
                if (cacheIt != cache_.end()) {
                    cacheIt->avatarUrl = result.avatarUrl;
                    cacheIt->metadataCheckedAtMs = QDateTime::currentMSecsSinceEpoch();
                    if (!liveState.isEmpty()) {
                        cacheIt->liveState = liveState;
                        cacheIt->liveCheckedAtMs = QDateTime::currentMSecsSinceEpoch();
                    }
                } else {
                    storeCacheEntry(memberId, expectedRoomId, result.avatarUrl, true);
                }
                emit cacheChanged();
                emit memberChanged(memberId);
                break;
            }
        }
        scheduleNextRequest(kRequestSpacingMs);
        return true;
    }

    QSet<QString> matches;
    const RoomSearchResult *matchedResult = nullptr;
    for (const RoomSearchResult &result : response.results) {
        if (!isExactMatch(member, result)) continue;
        matches.insert(result.roomId);
        matchedResult = &result;
    }
    if (matches.size() == 1 && matchedResult != nullptr) {
        const QString roomId = *matches.cbegin();
        roomIds_.insert(memberId, roomId);
        statuses_.insert(memberId, QStringLiteral("resolved"));
        failures_.remove(memberId);
        applyMetadata(memberId, *matchedResult);
        updateCacheLiveState(memberId,
                             matchedResult->online ? QStringLiteral("online")
                                                   : QStringLiteral("offline"));
        storeCacheEntry(memberId, roomId, matchedResult->avatarUrl, true);
        emit memberChanged(memberId);
        emit cacheChanged();
    } else if (!matches.isEmpty()) {
        completeWithoutRoom(memberId, QStringLiteral("unconfirmed"));
    } else {
        completeWithoutRoom(memberId, QStringLiteral("error"));
    }

    scheduleNextRequest(kRequestSpacingMs);
    return true;
}

bool GuildRoomResolver::handleFailure(quint64 requestId, const QString &)
{
    if (activeRequestId_ == 0 || requestId != activeRequestId_) return false;
    const QString memberId = activeMemberId_;
    const bool metadataRefresh = activeRequestIsMetadataRefresh_;
    activeRequestId_ = 0;
    activeMemberId_.clear();
    activeRequestIsMetadataRefresh_ = false;
    activeRequestStatusOnly_ = false;
    if (metadataRefresh) {
        scheduleNextRequest(kRequestSpacingMs);
        return true;
    }
    scheduleRetry(memberId);
    return true;
}
int GuildRoomResolver::nextRetryDelayMsForTest() const noexcept
{
    return nextRetryDelayMs_;
}

bool GuildRoomResolver::isValidRoomId(const QString &roomId)
{
    return kRoomIdPattern.match(roomId).hasMatch();
}

void GuildRoomResolver::scheduleNextRequest(int delayMs)
{
    if (!started_ || transport_ == nullptr
        || (queue_.isEmpty() && metadataQueue_.isEmpty())) {
        return;
    }
    scheduleTimer_->start(qMax(0, delayMs));
}

void GuildRoomResolver::sendNextRequest()
{
    if (!started_ || transport_ == nullptr || !activeMemberId_.isEmpty()) return;

    if (!metadataQueue_.isEmpty()) {
        const MetadataRefreshTask task = metadataQueue_.takeFirst();
        const QString memberId = task.memberId;
        queuedMetadataMembers_.remove(memberId);
        activeRequestStatusOnly_ = false;
        const QString roomId = roomIds_.value(memberId);
        if (!members_.contains(memberId) || roomId.isEmpty()) {
            sendNextRequest();
            return;
        }
        activeMemberId_ = memberId;
        activeRequestIsMetadataRefresh_ = true;
        activeRequestStatusOnly_ = task.statusOnly;
        activeRequestId_ = task.statusOnly ? transport_->status(roomId)
                                           : transport_->search(roomId);
        if (activeRequestId_ == 0) {
            activeMemberId_.clear();
            activeRequestIsMetadataRefresh_ = false;
            activeRequestStatusOnly_ = false;
            scheduleNextRequest(0);
        }
        return;
    }

    while (!queue_.isEmpty()) {
        const QString memberId = queue_.takeFirst();
        queuedMembers_.remove(memberId);
        if (!members_.contains(memberId) || roomIds_.contains(memberId)) continue;
        activeMemberId_ = memberId;
        activeRequestIsMetadataRefresh_ = false;
        statuses_.insert(memberId, QStringLiteral("resolving"));
        emit memberChanged(memberId);
        activeRequestId_ = transport_->search(members_.value(memberId).searchName);
        if (activeRequestId_ == 0) {
            activeMemberId_.clear();
            scheduleRetry(memberId);
            return;
        }
        return;
    }
}

void GuildRoomResolver::completeWithoutRoom(const QString &memberId, const QString &status)
{
    roomIds_.remove(memberId);
    statuses_.insert(memberId, status);
    emit memberChanged(memberId);
}

void GuildRoomResolver::scheduleRetry(const QString &memberId)
{
    const int failures = failures_.value(memberId) + 1;
    failures_.insert(memberId, failures);
    nextRetryDelayMs_ = retryDelayForFailureCount(failures);
    statuses_.insert(memberId, QStringLiteral("retrying"));
    emit memberChanged(memberId);
    queue_.push_back(memberId);
    queuedMembers_.insert(memberId);
    scheduleNextRequest(nextRetryDelayMs_);
}

void GuildRoomResolver::removeQueuedMember(const QString &memberId)
{
    queue_.removeAll(memberId);
    queuedMembers_.remove(memberId);
}

void GuildRoomResolver::updateCacheLiveState(const QString &memberId, const QString &liveState)
{
    const auto cacheIt = std::find_if(cache_.begin(), cache_.end(),
                                      [&memberId](const GuildRoomCacheEntry &entry) {
                                          return entry.memberId == memberId;
                                      });
    if (cacheIt == cache_.end()) return;
    cacheIt->liveState = liveState;
    cacheIt->liveCheckedAtMs = QDateTime::currentMSecsSinceEpoch();
    emit cacheChanged();
}

