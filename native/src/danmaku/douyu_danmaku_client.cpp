#include "danmaku/douyu_danmaku_client.h"

#include "danmaku/douyu_danmaku_protocol.h"

#include <QUrl>

#include <array>

namespace {

constexpr std::array<int, 6> kRetryDelaysMs = {1000, 2000, 4000, 8000, 15000, 15000};
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
    status_.roomId = roomId_;
    connectSocketSignals();
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
    connect(socket_.get(), &DanmakuSocket::connected, this, [this] {
        if (!running_) return;
        sendLoginAndJoin();
        handshakeTimerId_ = scheduler_->once(kHandshakeTimeoutMs,
                                             [this] { handleHandshakeTimeout(); });
        heartbeatTimerId_ = scheduler_->repeating(kHeartbeatIntervalMs,
                                                  [this] { sendHeartbeat(); });
    });
    connect(socket_.get(), &DanmakuSocket::binaryFrameReceived,
            this, [this](const QByteArray &frame) { handleFrame(frame); });
    connect(socket_.get(), &DanmakuSocket::networkFailure,
            this, [this] { handleSocketFailure(); });
    connect(socket_.get(), &DanmakuSocket::closed,
            this, [this](int code, const QString &reason) { handleClosed(code, reason); });
    connect(socket_.get(), &DanmakuSocket::authenticationRequested,
            this, [this] { handleAuthenticationRequested(); });
}

void DouyuDanmakuClient::beginConnection(bool retrying)
{
    if (!running_) return;
    decoder_.clear();
    handshakeComplete_ = false;
    setStatus(retrying ? DanmakuConnectionState::Reconnecting
                       : DanmakuConnectionState::Connecting);
    socket_->connectTo(QUrl(kEndpoints.at(endpointIndex_)));
}

void DouyuDanmakuClient::cancelTimers()
{
    if (!scheduler_) return;
    for (const auto id : {handshakeTimerId_, heartbeatTimerId_, retryTimerId_, stableTimerId_}) {
        if (id != 0) scheduler_->cancel(id);
    }
    handshakeTimerId_ = heartbeatTimerId_ = retryTimerId_ = stableTimerId_ = 0;
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

void DouyuDanmakuClient::handleFrame(const QByteArray &frame)
{
    if (!running_) return;
    const auto result = decoder_.push(frame);
    if (result.error != DouyuDanmakuProtocol::FrameError::None) {
        scheduleRetry(DanmakuErrorCode::ProtocolChanged);
        return;
    }
    for (const QString &raw : result.frames) {
        const QMap<QString, QString> fields = DouyuDanmakuProtocol::parseStt(raw);
        const QString type = fields.value(QStringLiteral("type"));
        if (type == QLatin1String("loginres") && fields.contains(QStringLiteral("error"))) {
            handleAuthenticationRequested();
            return;
        }
        if (type == QLatin1String("loginres") || type == QLatin1String("setmsggroup")) {
            markConnected();
        }
        if (type == QLatin1String("chatmsg")
            && fields.value(QStringLiteral("rid")) == roomId_) {
            markConnected();
            emit chatReceived(roomId_, fields);
        }
    }
}

void DouyuDanmakuClient::handleSocketFailure()
{
    if (!running_) return;
    scheduleRetry(DanmakuErrorCode::NetworkUnavailable);
}

void DouyuDanmakuClient::handleClosed(int code, const QString &reason)
{
    if (!running_ || platformBlocked_) return;
    if (isAuthenticationClose(code, reason)) {
        handleAuthenticationRequested();
        return;
    }
    if (!handshakeComplete_) scheduleRetry(DanmakuErrorCode::NetworkUnavailable);
}

void DouyuDanmakuClient::handleAuthenticationRequested()
{
    if (!running_) return;
    cancelTimers();
    running_ = true;
    platformBlocked_ = true;
    setStatus(DanmakuConnectionState::PlatformBlocked, DanmakuErrorCode::AuthRequired);
    socket_->close();
}

void DouyuDanmakuClient::handleHandshakeTimeout()
{
    if (!running_ || handshakeComplete_) return;
    scheduleRetry(DanmakuErrorCode::HandshakeTimeout);
}

void DouyuDanmakuClient::scheduleRetry(DanmakuErrorCode errorCode)
{
    if (!running_ || platformBlocked_ || retryTimerId_ != 0) return;
    cancelTimers();
    if (retryAttempt_ >= static_cast<int>(kRetryDelaysMs.size())) {
        setStatus(DanmakuConnectionState::Failed, DanmakuErrorCode::RetryExhausted);
        return;
    }
    setStatus(DanmakuConnectionState::Reconnecting, errorCode);
    const int delay = kRetryDelaysMs.at(retryAttempt_++);
    endpointIndex_ = (endpointIndex_ + 1) % static_cast<int>(kEndpoints.size());
    retryTimerId_ = scheduler_->once(delay, [this] {
        retryTimerId_ = 0;
        beginConnection(true);
    });
}

void DouyuDanmakuClient::markConnected()
{
    if (!running_ || handshakeComplete_) return;
    handshakeComplete_ = true;
    retryAttempt_ = 0;
    if (handshakeTimerId_ != 0) scheduler_->cancel(handshakeTimerId_);
    handshakeTimerId_ = 0;
    setStatus(DanmakuConnectionState::Connected);
    stableTimerId_ = scheduler_->once(kStableConnectionMs, [this] { retryAttempt_ = 0; });
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
