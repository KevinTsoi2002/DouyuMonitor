#include "danmaku/danmaku_socket.h"

#include "danmaku/douyu_danmaku_protocol.h"

#include <QAuthenticator>
#include <QWebSocket>

QtDanmakuSocket::QtDanmakuSocket(QObject *parent)
    : DanmakuSocket(parent)
{
}

void QtDanmakuSocket::connectTo(const QUrl &url, quint64 attemptId)
{
    if (socket_) {
        socket_->close();
        socket_->deleteLater();
        socket_ = nullptr;
    }
    socket_ = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    QWebSocket *const connectionSocket = socket_;
    connect(connectionSocket, &QWebSocket::connected, this, [this, attemptId] {
        emit connected(attemptId);
    });
    connect(connectionSocket, &QWebSocket::binaryMessageReceived,
            this, [this, attemptId](const QByteArray &frame) {
                emit binaryFrameReceived(frame, attemptId);
            });
    connect(connectionSocket, &QWebSocket::errorOccurred, this,
            [this, attemptId](QAbstractSocket::SocketError) {
                emit networkFailure(attemptId);
            });
    connect(connectionSocket, &QWebSocket::disconnected, this, [this, attemptId, connectionSocket] {
        emit closed(static_cast<int>(connectionSocket->closeCode()),
                    connectionSocket->closeReason(), attemptId);
    });
    connect(connectionSocket, &QWebSocket::authenticationRequired, this,
            [this, attemptId, connectionSocket](QAuthenticator *) {
                emit authenticationRequested(attemptId);
                connectionSocket->close();
            });
    socket_->setMaxAllowedIncomingMessageSize(
        static_cast<quint64>(DouyuDanmakuProtocol::kMaxWebSocketMessageBytes));
    socket_->open(url);
}

void QtDanmakuSocket::sendBinary(const QByteArray &frame)
{
    if (socket_ && socket_->isValid()) socket_->sendBinaryMessage(frame);
}

void QtDanmakuSocket::close()
{
    if (socket_) socket_->close();
}
