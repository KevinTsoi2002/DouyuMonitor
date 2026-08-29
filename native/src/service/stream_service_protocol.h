#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QUrl>
#include <QVector>

#include <optional>

enum class StreamQuality {
    Auto,
    Original,
    Super,
    High,
    Standard,
};

enum class ServiceOperation {
    Ping,
    Resolve,
    Search,
    Cancel,
    Shutdown,
};

struct StreamVariant {
    QString id;
    QString label;
    StreamQuality quality = StreamQuality::Auto;
    QString container;
    QUrl playbackUrl;
};

struct RoomSearchResult {
    QString roomId;
    QString anchorName;
    QString title;
    QString category;
    bool online = false;
    QString viewerLabel;
    QUrl avatarUrl;
};

struct ServiceRequest {
    quint64 requestId = 0;
    ServiceOperation operation = ServiceOperation::Ping;
    QString roomId;
    QString query;
    StreamQuality quality = StreamQuality::Auto;
    quint64 targetRequestId = 0;
};

struct ServiceResponse {
    quint64 requestId = 0;
    bool ok = false;
    bool pong = false;
    bool shutdown = false;
    quint64 cancelledRequestId = 0;
    bool search = false;
    QVector<RoomSearchResult> results;
    QString roomId;
    bool isLive = false;
    QVector<StreamVariant> variants;
    QString errorCode;
    bool retryable = false;
    QString errorMessage;
};

Q_DECLARE_METATYPE(ServiceResponse)
Q_DECLARE_METATYPE(StreamVariant)
Q_DECLARE_METATYPE(QVector<StreamVariant>)

QByteArray encodeRequest(const ServiceRequest &request);
std::optional<ServiceRequest> decodeRequest(const QByteArray &line);
std::optional<ServiceResponse> decodeResponse(const QByteArray &line);
