#pragma once

#include "danmaku/danmaku_socket.h"
#include "danmaku/danmaku_timer_scheduler.h"
#include "danmaku/danmaku_types.h"
#include "danmaku/douyu_danmaku_protocol.h"

#include <QMap>

#include <functional>
#include <memory>

class DanmakuClient : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;
    ~DanmakuClient() override = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void retry() = 0;

signals:
    void statusChanged(const DanmakuConnectionStatus &status);
    void chatReceived(const QString &roomId, const QMap<QString, QString> &rawChat);
};

class DouyuDanmakuClient final : public DanmakuClient {
    Q_OBJECT

public:
    DouyuDanmakuClient(const QString &roomId,
                       std::unique_ptr<DanmakuSocket> socket,
                       DanmakuTimerScheduler *scheduler,
                       QObject *parent = nullptr);
    ~DouyuDanmakuClient() override;

    void start() override;
    void stop() override;
    void retry() override;

private:
    void connectSocketSignals();
    void beginConnection(bool retrying);
    void cancelTimers();
    void sendLoginAndJoin();
    void sendHeartbeat();
    void handleFrame(const QByteArray &frame, quint64 attemptId);
    void continueFrameDispatch(quint64 attemptId);
    bool processDecodedFrames(const QStringList &frames, quint64 attemptId);
    void scheduleFrameDispatch(quint64 attemptId);
    void handleSocketFailure(quint64 attemptId);
    void handleClosed(int code, const QString &reason, quint64 attemptId);
    void handleAuthenticationRequested(quint64 attemptId);
    void handleConnectionTimeout(quint64 attemptId);
    void handleHandshakeTimeout(quint64 attemptId);
    void scheduleRetry(DanmakuErrorCode errorCode, quint64 attemptId);
    void markConnected(quint64 attemptId);
    bool isCurrentAttempt(quint64 attemptId) const;
    void setStatus(DanmakuConnectionState state, DanmakuErrorCode errorCode = DanmakuErrorCode::None);
    bool isAuthenticationClose(int code, const QString &reason) const;

    QString roomId_;
    std::unique_ptr<DanmakuSocket> socket_;
    DanmakuTimerScheduler *scheduler_ = nullptr;
    DanmakuConnectionStatus status_;
    DanmakuConnectionState stateBeforeStop_ = DanmakuConnectionState::Idle;
    DouyuDanmakuProtocol::FrameDecoder decoder_;
    DanmakuTimerScheduler::TimerId connectionTimerId_ = 0;
    DanmakuTimerScheduler::TimerId handshakeTimerId_ = 0;
    DanmakuTimerScheduler::TimerId heartbeatTimerId_ = 0;
    DanmakuTimerScheduler::TimerId retryTimerId_ = 0;
    DanmakuTimerScheduler::TimerId stableTimerId_ = 0;
    DanmakuTimerScheduler::TimerId frameDispatchTimerId_ = 0;
    int retryAttempt_ = 0;
    int endpointIndex_ = 0;
    bool running_ = false;
    bool platformBlocked_ = false;
    bool handshakeComplete_ = false;
    quint64 nextAttemptId_ = 0;
    quint64 activeAttemptId_ = 0;
};

using DanmakuClientFactory = std::function<std::unique_ptr<DanmakuClient>(
    const QString &roomId, QObject *parent)>;
