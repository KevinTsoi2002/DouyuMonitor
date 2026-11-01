#pragma once

#include <QUrl>

#include <QObject>

class DanmakuSocket : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;
    ~DanmakuSocket() override = default;

    virtual void connectTo(const QUrl &url, quint64 attemptId) = 0;
    virtual void sendBinary(const QByteArray &frame) = 0;
    virtual void close() = 0;

signals:
    void connected(quint64 attemptId);
    void binaryFrameReceived(const QByteArray &frame, quint64 attemptId);
    void networkFailure(quint64 attemptId);
    void closed(int code, const QString &reason, quint64 attemptId);
    void authenticationRequested(quint64 attemptId);
};

class QtDanmakuSocket final : public DanmakuSocket {
    Q_OBJECT

public:
    explicit QtDanmakuSocket(QObject *parent = nullptr);

    void connectTo(const QUrl &url, quint64 attemptId) override;
    void sendBinary(const QByteArray &frame) override;
    void close() override;

private:
    class QWebSocket *socket_ = nullptr;
};
