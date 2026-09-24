#include "workspace/guild_room_resolver.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QTimer>

#include <algorithm>

namespace {

constexpr int kRequestSpacingMs = 1200;
constexpr int kRetryDelaysMs[] = {3000, 15000, 75000};

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
    failures_.clear();
    cache_.clear();
    queue_.clear();
    queuedMembers_.clear();

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
    failures_.clear();
    queue_.clear();
    queuedMembers_.clear();

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
    if (activeMemberId_.isEmpty() && !queue_.isEmpty()) sendNextRequest();
}

void GuildRoomResolver::stop()
{
    started_ = false;
    if (scheduleTimer_ != nullptr) scheduleTimer_->stop();
    const quint64 requestId = activeRequestId_;
    activeRequestId_ = 0;
    activeMemberId_.clear();
    if (requestId != 0 && transport_ != nullptr) transport_->cancel(requestId);
}

QString GuildRoomResolver::roomIdFor(const QString &memberId) const
{
    return roomIds_.value(memberId);
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
    statuses_.insert(memberId, QStringLiteral("resolved"));
    GuildRoomCacheEntry entry;
    entry.memberId = memberId;
    entry.roomId = normalizedRoomId;
    entry.anchorName = member->anchorName;
    entry.verifiedAtMs = QDateTime::currentMSecsSinceEpoch();
    const auto existing = std::find_if(cache_.begin(), cache_.end(),
                                       [&memberId](const GuildRoomCacheEntry &candidate) {
                                           return candidate.memberId == memberId;
                                       });
    if (existing == cache_.end()) cache_.push_back(std::move(entry));
    else *existing = std::move(entry);
    emit memberChanged(memberId);
    emit cacheChanged();
    if (started_ && activeMemberId_.isEmpty() && !scheduleTimer_->isActive()
        && !queue_.isEmpty()) {
        scheduleNextRequest(kRequestSpacingMs);
    }
    return {};
}

bool GuildRoomResolver::handleResponse(const ServiceResponse &response)
{
    if (activeRequestId_ == 0 || response.requestId != activeRequestId_) return false;

    const QString memberId = activeMemberId_;
    activeRequestId_ = 0;
    activeMemberId_.clear();

    const GuildMember member = members_.value(memberId);
    if (!response.ok || !response.search) {
        scheduleRetry(memberId);
        return true;
    }

    QSet<QString> matches;
    for (const RoomSearchResult &result : response.results) {
        if (sameName(member.anchorName, result.anchorName)
            && isValidRoomId(result.roomId)) {
            matches.insert(result.roomId);
        }
    }
    if (matches.size() == 1) {
        const QString roomId = *matches.cbegin();
        roomIds_.insert(memberId, roomId);
        statuses_.insert(memberId, QStringLiteral("resolved"));
        failures_.remove(memberId);
        GuildRoomCacheEntry entry;
        entry.memberId = memberId;
        entry.roomId = roomId;
        entry.anchorName = member.anchorName;
        entry.verifiedAtMs = QDateTime::currentMSecsSinceEpoch();
        const auto existing = std::find_if(cache_.begin(), cache_.end(),
                                           [&memberId](const GuildRoomCacheEntry &candidate) {
                                               return candidate.memberId == memberId;
                                           });
        if (existing == cache_.end()) cache_.push_back(std::move(entry));
        else *existing = std::move(entry);
        emit memberChanged(memberId);
        emit cacheChanged();
    } else {
        completeWithoutRoom(memberId,
                            matches.size() > 1 ? QStringLiteral("unconfirmed")
                                               : QStringLiteral("error"));
    }

    scheduleNextRequest(kRequestSpacingMs);
    return true;
}

bool GuildRoomResolver::handleFailure(quint64 requestId, const QString &)
{
    if (activeRequestId_ == 0 || requestId != activeRequestId_) return false;
    const QString memberId = activeMemberId_;
    activeRequestId_ = 0;
    activeMemberId_.clear();
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
    if (!started_ || transport_ == nullptr || queue_.isEmpty()) return;
    scheduleTimer_->start(qMax(0, delayMs));
}

void GuildRoomResolver::sendNextRequest()
{
    if (!started_ || transport_ == nullptr || !activeMemberId_.isEmpty()) return;
    while (!queue_.isEmpty()) {
        const QString memberId = queue_.takeFirst();
        queuedMembers_.remove(memberId);
        if (!members_.contains(memberId) || roomIds_.contains(memberId)) continue;
        activeMemberId_ = memberId;
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

