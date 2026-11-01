#include "danmaku/douyu_danmaku_client.h"

#include "danmaku/douyu_danmaku_protocol.h"

#include <QUrl>

#include <array>

namespace {

constexpr std::array<int, 6> kRetryDelaysMs = {1000, 2000, 4000, 8000, 15000, 15000};
constexpr int kConnectionTimeoutMs = 10'000;
constexpr int kHandshakeTimeoutMs = 10'000;
constexpr int kHeartbeatIntervalMs = 45'000;
constexpr int kStableConnectionMs = 60'000;

const std::array<QString, 6> kEndpoints = {
    QStringLiteral("wss://danmuproxy.douyu.com:8501/"),
    QStringLiteral("wss://danmuproxy.douyu.com:8502/"),
    QStringLiteral("wss://danmuproxy.douyu.com:8503/"),
    QStringLiteral("wss://danmuproxy.douyu.com:8504/"),
    QStringLiteral("wss://danmuproxy.douyu.com:8505/"),
    QStringLiteral("wss://danmuproxy.douyu.com:8506/"),
};

} // namespace

DouyuDanmakuClient::DouyuDanmakuClient(const QString &roomId,
                                       std::unique_ptr<DanmakuSocket> socket,
                                       DanmakuTimerScheduler *scheduler,
                                       QObject *parent)
    : DanmakuClient(parent)
    , roomId_(roomId)
    , socket_(std::move(socket))
    , scheduler_(scheduler)
{
    if (auto *schedulerObject = dynamic_cast<QObject *>(scheduler_))
        schedulerObject->setParent(this);
    status_.roomId = roomId_;
    connectSocketSignals();
}

DouyuDanmakuClient::~DouyuDanmakuClient()
{
    running_ = false;
    cancelTimers();
    if (socket_) socket_->close();
}

void DouyuDanmakuClient::start()
{
    if (running_ || !scheduler_ || !socket_) return;
    running_ = true;
    retryAttempt_ = 0;
    endpointIndex_ = 0;
    platformBlocked_ = false;
    beginConnection(false);
}

void DouyuDanmakuClient::stop()
{
    if (!running_) return;
    running_ = false;
    stateBeforeStop_ = status_.state;
    cancelTimers();
    socket_->close();
    decoder_.clear();
    handshakeComplete_ = false;
    setStatus(DanmakuConnectionState::Idle);
}

void DouyuDanmakuClient::retry()
{
    if (!scheduler_ || !socket_) return;
    cancelTimers();
    running_ = true;
    retryAttempt_ = 0;
    platformBlocked_ = false;
    handshakeComplete_ = false;
    decoder_.clear();
    beginConnection(true);
}

void DouyuDanmakuClient::connectSocketSignals()
{
    connect(socket_.get(), &DanmakuSocket::connected, this, [this](quint64 attemptId) {
        if (!isCurrentAttempt(attemptId) || handshakeComplete_ || handshakeTimerId_ != 0) return;
        if (connectionTimerId_ != 0) scheduler_->cancel(connectionTimerId_);
        connectionTimerId_ = 0;
        sendLoginAndJoin();
        handshakeTimerId_ = scheduler_->once(kHandshakeTimeoutMs,
                                             [this, attemptId] { handleHandshakeTimeout(attemptId); });
        heartbeatTimerId_ = scheduler_->repeating(kHeartbeatIntervalMs,
                                                  [this, attemptId] {
                                                      if (isCurrentAttempt(attemptId)) sendHeartbeat();
                                                  });
    });
    connect(socket_.get(), &DanmakuSocket::binaryFrameReceived,
            this, [this](const QByteArray &frame, quint64 attemptId) {
                handleFrame(frame, attemptId);
            });
    connect(socket_.get(), &DanmakuSocket::networkFailure,
            this, [this](quint64 attemptId) { handleSocketFailure(attemptId); });
    connect(socket_.get(), &DanmakuSocket::closed,
            this, [this](int code, const QString &reason, quint64 attemptId) {
                handleClosed(code, reason, attemptId);
            });
    connect(socket_.get(), &DanmakuSocket::authenticationRequested,
            this, [this](quint64 attemptId) { handleAuthenticationRequested(attemptId); });
}

void DouyuDanmakuClient::beginConnection(bool retrying)
{
    if (!running_) return;
    decoder_.clear();
    handshakeComplete_ = false;
    activeAttemptId_ = ++nextAttemptId_;
    const quint64 attemptId = activeAttemptId_;
    setStatus(retrying ? DanmakuConnectionState::Reconnecting
                       : DanmakuConnectionState::Connecting);
    connectionTimerId_ = scheduler_->once(kConnectionTimeoutMs, [this, attemptId] {
        connectionTimerId_ = 0;
        handleConnectionTimeout(attemptId);
    });
    socket_->connectTo(QUrl(kEndpoints.at(endpointIndex_)), attemptId);
}

void DouyuDanmakuClient::cancelTimers()
{
    if (!scheduler_) return;
    for (const auto id : {connectionTimerId_, handshakeTimerId_, heartbeatTimerId_, retryTimerId_,
                          stableTimerId_, frameDispatchTimerId_}) {
        if (id != 0) scheduler_->cancel(id);
    }
    connectionTimerId_ = handshakeTimerId_ = heartbeatTimerId_ = retryTimerId_ = stableTimerId_ =
        frameDispatchTimerId_ = 0;
}

void DouyuDanmakuClient::sendLoginAndJoin()
{
    const auto login = DouyuDanmakuProtocol::serializeStt({
        {QStringLiteral("type"), QStringLiteral("loginreq")},
        {QStringLiteral("roomid"), roomId_},
    });
    const auto join = DouyuDanmakuProtocol::serializeStt({
        {QStringLiteral("type"), QStringLiteral("joingroup")},
        {QStringLiteral("rid"), roomId_},
        {QStringLiteral("gid"), QStringLiteral("-9999")},
    });
    socket_->sendBinary(DouyuDanmakuProtocol::encodeFrame(login));
    socket_->sendBinary(DouyuDanmakuProtocol::encodeFrame(join));
}

void DouyuDanmakuClient::sendHeartbeat()
{
    if (!running_) return;
    socket_->sendBinary(DouyuDanmakuProtocol::encodeFrame(
        DouyuDanmakuProtocol::serializeStt({{QStringLiteral("type"), QStringLiteral("mrkl")}})));
}

bool DouyuDanmakuClient::isCurrentAttempt(quint64 attemptId) const
{
    return running_ && attemptId != 0 && attemptId == activeAttemptId_;
}

void DouyuDanmakuClient::handleFrame(const QByteArray &frame, quint64 attemptId)
{
    if (!isCurrentAttempt(attemptId)) return;
    const auto result = decoder_.push(frame);
    if (result.error != DouyuDanmakuProtocol::FrameError::None) {
        scheduleRetry(DanmakuErrorCode::ProtocolChanged, attemptId);
        return;
    }
    if (!processDecodedFrames(result.frames, attemptId)) return;
    if (result.hasMoreFrames) scheduleFrameDispatch(attemptId);
}

void DouyuDanmakuClient::continueFrameDispatch(quint64 attemptId)
{
    frameDispatchTimerId_ = 0;
    if (!isCurrentAttempt(attemptId)) return;
    const auto result = decoder_.push({});
    if (result.error != DouyuDanmakuProtocol::FrameError::None) {
        scheduleRetry(DanmakuErrorCode::ProtocolChanged, attemptId);
        return;
    }
    if (!processDecodedFrames(result.frames, attemptId)) return;
    if (result.hasMoreFrames) scheduleFrameDispatch(attemptId);
}

bool DouyuDanmakuClient::processDecodedFrames(const QStringList &frames, quint64 attemptId)
{
    for (const QString &raw : frames) {
        if (!isCurrentAttempt(attemptId)) return false;
        const QMap<QString, QString> fields = DouyuDanmakuProtocol::parseStt(raw);
        const QString type = fields.value(QStringLiteral("type"));
        if (type == QLatin1String("loginres") && fields.contains(QStringLiteral("error"))) {
            handleAuthenticationRequested(attemptId);
            return false;
        }
        if (type == QLatin1String("loginres") || type == QLatin1String("setmsggroup")) {
            markConnected(attemptId);
        }
        if (type == QLatin1String("chatmsg")
            && fields.value(QStringLiteral("rid")) == roomId_) {
            markConnected(attemptId);
            emit chatReceived(roomId_, fields);
        }
    }
    return true;
}

void DouyuDanmakuClient::scheduleFrameDispatch(quint64 attemptId)
{
    if (!isCurrentAttempt(attemptId) || frameDispatchTimerId_ != 0)
        return;
    frameDispatchTimerId_ = scheduler_->once(0, [this, attemptId] {
        continueFrameDispatch(attemptId);
    });
}

void DouyuDanmakuClient::handleSocketFailure(quint64 attemptId)
{
    if (!isCurrentAttempt(attemptId)) return;
    scheduleRetry(DanmakuErrorCode::NetworkUnavailable, attemptId);
}

void DouyuDanmakuClient::handleClosed(int code, const QString &reason, quint64 attemptId)
{
    if (!isCurrentAttempt(attemptId) || platformBlocked_) return;
    if (isAuthenticationClose(code, reason)) {
        handleAuthenticationRequested(attemptId);
        return;
    }
    scheduleRetry(DanmakuErrorCode::NetworkUnavailable, attemptId);
}

void DouyuDanmakuClient::handleAuthenticationRequested(quint64 attemptId)
{
    if (!isCurrentAttempt(attemptId)) return;
    cancelTimers();
    decoder_.clear();
    activeAttemptId_ = ++nextAttemptId_;
    running_ = true;
    platformBlocked_ = true;
    setStatus(DanmakuConnectionState::PlatformBlocked, DanmakuErrorCode::AuthRequired);
    socket_->close();
}

void DouyuDanmakuClient::handleHandshakeTimeout(quint64 attemptId)
{
    if (!isCurrentAttempt(attemptId) || handshakeComplete_) return;
    scheduleRetry(DanmakuErrorCode::HandshakeTimeout, attemptId);
}

void DouyuDanmakuClient::handleConnectionTimeout(quint64 attemptId)
{
    if (!isCurrentAttempt(attemptId) || handshakeComplete_) return;
    scheduleRetry(DanmakuErrorCode::NetworkUnavailable, attemptId);
}

void DouyuDanmakuClient::scheduleRetry(DanmakuErrorCode errorCode, quint64 attemptId)
{
    if (!isCurrentAttempt(attemptId) || platformBlocked_ || retryTimerId_ != 0) return;
    cancelTimers();
    decoder_.clear();
    activeAttemptId_ = ++nextAttemptId_;
    socket_->close();
    if (retryAttempt_ >= static_cast<int>(kRetryDelaysMs.size())) {
        setStatus(DanmakuConnectionState::Failed, DanmakuErrorCode::RetryExhausted);
        return;
    }
    setStatus(DanmakuConnectionState::Reconnecting, errorCode);
    const int delay = kRetryDelaysMs.at(retryAttempt_++);
    endpointIndex_ = (endpointIndex_ + 1) % static_cast<int>(kEndpoints.size());
    const quint64 retryToken = activeAttemptId_;
    retryTimerId_ = scheduler_->once(delay, [this, retryToken] {
        if (!running_ || activeAttemptId_ != retryToken) return;
        retryTimerId_ = 0;
        beginConnection(true);
    });
}

void DouyuDanmakuClient::markConnected(quint64 attemptId)
{
    if (!isCurrentAttempt(attemptId) || handshakeComplete_) return;
    handshakeComplete_ = true;
    if (handshakeTimerId_ != 0) scheduler_->cancel(handshakeTimerId_);
    if (retryTimerId_ != 0) scheduler_->cancel(retryTimerId_);
    handshakeTimerId_ = 0;
    retryTimerId_ = 0;
    setStatus(DanmakuConnectionState::Connected);
    const quint64 stableAttemptId = attemptId;
    stableTimerId_ = scheduler_->once(kStableConnectionMs, [this, stableAttemptId] {
        if (!isCurrentAttempt(stableAttemptId) || !handshakeComplete_
            || status_.state != DanmakuConnectionState::Connected) {
            return;
        }
        retryAttempt_ = 0;
        stableTimerId_ = 0;
    });
}

void DouyuDanmakuClient::setStatus(DanmakuConnectionState state, DanmakuErrorCode errorCode)
{
    if (!running_ && state != DanmakuConnectionState::Idle) return;
    status_.state = state;
    status_.attempt = retryAttempt_;
    status_.errorCode = errorCode;
    emit statusChanged(status_);
}

bool DouyuDanmakuClient::isAuthenticationClose(int code, const QString &reason) const
{
    if (code != 1008) return false;
    const QString lower = reason.toCaseFolded();
    return lower.contains(QStringLiteral("auth")) || lower.contains(QStringLiteral("login"));
}
