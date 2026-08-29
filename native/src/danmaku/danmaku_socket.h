#pragma once

#include <QUrl>

#include <QObject>

class DanmakuSocket : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;
    ~DanmakuSocket() override = default;

    virtual void connectTo(const QUrl &url) = 0;
    virtual void sendBinary(const QByteArray &frame) = 0;
    virtual void close() = 0;

signals:
    void connected();
    void binaryFrameReceived(const QByteArray &frame);
    void networkFailure();
    void closed(int code, const QString &reason);
    void authenticationRequested();
};

class QtDanmakuSocket final : public DanmakuSocket {
    Q_OBJECT

public:
    explicit QtDanmakuSocket(QObject *parent = nullptr);

    void connectTo(const QUrl &url) override;
    void sendBinary(const QByteArray &frame) override;
    void close() override;

private:
    class QWebSocket *socket_ = nullptr;
};
