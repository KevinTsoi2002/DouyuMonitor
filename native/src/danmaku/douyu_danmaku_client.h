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

    void start() override;
    void stop() override;
    void retry() override;

private:
    void connectSocketSignals();
    void beginConnection(bool retrying);
    void cancelTimers();
    void sendLoginAndJoin();
    void sendHeartbeat();
    void handleFrame(const QByteArray &frame);
    void handleSocketFailure();
    void handleClosed(int code, const QString &reason);
    void handleAuthenticationRequested();
    void handleHandshakeTimeout();
    void scheduleRetry(DanmakuErrorCode errorCode);
    void markConnected();
    void setStatus(DanmakuConnectionState state, DanmakuErrorCode errorCode = DanmakuErrorCode::None);
    bool isAuthenticationClose(int code, const QString &reason) const;

    QString roomId_;
    std::unique_ptr<DanmakuSocket> socket_;
    DanmakuTimerScheduler *scheduler_ = nullptr;
    DanmakuConnectionStatus status_;
    DanmakuConnectionState stateBeforeStop_ = DanmakuConnectionState::Idle;
    DouyuDanmakuProtocol::FrameDecoder decoder_;
    DanmakuTimerScheduler::TimerId handshakeTimerId_ = 0;
    DanmakuTimerScheduler::TimerId heartbeatTimerId_ = 0;
    DanmakuTimerScheduler::TimerId retryTimerId_ = 0;
    DanmakuTimerScheduler::TimerId stableTimerId_ = 0;
    int retryAttempt_ = 0;
    int endpointIndex_ = 0;
    bool running_ = false;
    bool platformBlocked_ = false;
    bool handshakeComplete_ = false;
};

using DanmakuClientFactory = std::function<std::unique_ptr<DanmakuClient>(
    const QString &roomId, QObject *parent)>;
