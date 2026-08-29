#include "danmaku/danmaku_socket.h"

#include <QAuthenticator>
#include <QWebSocket>

QtDanmakuSocket::QtDanmakuSocket(QObject *parent)
    : DanmakuSocket(parent)
    , socket_(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
{
    connect(socket_, &QWebSocket::connected, this, &DanmakuSocket::connected);
    connect(socket_, &QWebSocket::binaryMessageReceived,
            this, &DanmakuSocket::binaryFrameReceived);
    connect(socket_, &QWebSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) { emit networkFailure(); });
    connect(socket_, &QWebSocket::disconnected, this, [this] {
        emit closed(static_cast<int>(socket_->closeCode()), socket_->closeReason());
    });
    connect(socket_, &QWebSocket::authenticationRequired, this,
            [this](QAuthenticator *) {
                emit authenticationRequested();
                socket_->close();
            });
}

void QtDanmakuSocket::connectTo(const QUrl &url)
{
    socket_->open(url);
}

void QtDanmakuSocket::sendBinary(const QByteArray &frame)
{
    if (socket_->isValid()) socket_->sendBinaryMessage(frame);
}

void QtDanmakuSocket::close()
{
    socket_->close();
}
