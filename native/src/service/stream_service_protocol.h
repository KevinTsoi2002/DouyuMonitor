#pragma once

#include <QByteArray>
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
    QString roomId;
    bool isLive = false;
    QVector<StreamVariant> variants;
    QString errorCode;
    bool retryable = false;
    QString errorMessage;
};

QByteArray encodeRequest(const ServiceRequest &request);
std::optional<ServiceRequest> decodeRequest(const QByteArray &line);
std::optional<ServiceResponse> decodeResponse(const QByteArray &line);
