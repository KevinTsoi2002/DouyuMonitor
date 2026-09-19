#include "service/stream_service_protocol.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace {

const QRegularExpression kRoomIdPattern(QStringLiteral(R"(^[0-9]{1,20}$)"));
const QStringList kAllowedHosts = {
    QStringLiteral(".douyucdn.cn"),
    QStringLiteral(".douyucdn2.cn"),
    QStringLiteral(".edgesrv.com"),
};
const QStringList kErrorCodes = {
    QStringLiteral("INVALID_INPUT"),
    QStringLiteral("ROOM_OFFLINE"),
    QStringLiteral("STREAMGET_UNAVAILABLE"),
    QStringLiteral("UNSAFE_STREAM_URL"),
    QStringLiteral("TIMEOUT"),
    QStringLiteral("INVALID_RESPONSE"),
    QStringLiteral("SERVICE_FAILED"),
};

bool isPositiveRequestId(const QJsonValue &value)
{
    return value.isDouble() && value.toDouble() > 0
        && value.toDouble() == static_cast<quint64>(value.toDouble());
}

bool isValidRoomId(const QString &roomId)
{
    return kRoomIdPattern.match(roomId).hasMatch();
}

QString qualityToString(StreamQuality quality)
{
    switch (quality) {
    case StreamQuality::Auto:
        return QStringLiteral("auto");
    case StreamQuality::Original:
        return QStringLiteral("original");
    case StreamQuality::Super:
        return QStringLiteral("super");
    case StreamQuality::High:
        return QStringLiteral("high");
    case StreamQuality::Standard:
        return QStringLiteral("standard");
    }
    return {};
}

std::optional<StreamQuality> qualityFromString(const QString &value)
{
    if (value == QStringLiteral("auto")) return StreamQuality::Auto;
    if (value == QStringLiteral("original")) return StreamQuality::Original;
    if (value == QStringLiteral("super")) return StreamQuality::Super;
    if (value == QStringLiteral("high")) return StreamQuality::High;
    if (value == QStringLiteral("standard")) return StreamQuality::Standard;
    return std::nullopt;
}

QString operationToString(ServiceOperation operation)
{
    switch (operation) {
    case ServiceOperation::Ping:
        return QStringLiteral("ping");
    case ServiceOperation::Resolve:
        return QStringLiteral("resolve");
    case ServiceOperation::Search:
        return QStringLiteral("search");
    case ServiceOperation::Cancel:
        return QStringLiteral("cancel");
    case ServiceOperation::Shutdown:
        return QStringLiteral("shutdown");
    }
    return {};
}

std::optional<ServiceOperation> operationFromString(const QString &value)
{
    if (value == QStringLiteral("ping")) return ServiceOperation::Ping;
    if (value == QStringLiteral("resolve")) return ServiceOperation::Resolve;
    if (value == QStringLiteral("search")) return ServiceOperation::Search;
    if (value == QStringLiteral("cancel")) return ServiceOperation::Cancel;
    if (value == QStringLiteral("shutdown")) return ServiceOperation::Shutdown;
    return std::nullopt;
}

bool isAllowedPlaybackUrl(const QUrl &url)
{
    if (!url.isValid() || (url.scheme() != QStringLiteral("http")
                           && url.scheme() != QStringLiteral("https"))
        || !url.userName().isEmpty() || !url.password().isEmpty()) {
        return false;
    }

    const QString host = url.host().toLower();
    for (const QString &suffix : kAllowedHosts) {
        if (host.endsWith(suffix)) return true;
    }
    return false;
}

bool isSafeHttpUrl(const QUrl &url)
{
    return url.isValid() && (url.scheme() == QStringLiteral("http")
                             || url.scheme() == QStringLiteral("https"))
        && !url.host().isEmpty() && url.userName().isEmpty() && url.password().isEmpty();
}

} // namespace

QByteArray encodeRequest(const ServiceRequest &request)
{
    QJsonObject object;
    object.insert(QStringLiteral("requestId"), static_cast<qint64>(request.requestId));
    object.insert(QStringLiteral("op"), operationToString(request.operation));

    switch (request.operation) {
    case ServiceOperation::Resolve:
        object.insert(QStringLiteral("roomId"), request.roomId);
        object.insert(QStringLiteral("quality"), qualityToString(request.quality));
        if (request.qualityRate >= 0) {
            object.insert(QStringLiteral("qualityRate"), request.qualityRate);
        }
        break;
    case ServiceOperation::Search:
        object.insert(QStringLiteral("query"), request.query);
        break;
    case ServiceOperation::Cancel:
        object.insert(QStringLiteral("targetRequestId"), static_cast<qint64>(request.targetRequestId));
        break;
    case ServiceOperation::Ping:
    case ServiceOperation::Shutdown:
        break;
    }

    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

std::optional<ServiceRequest> decodeRequest(const QByteArray &line)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return std::nullopt;
    }

    const QJsonObject object = document.object();
    if (!isPositiveRequestId(object.value(QStringLiteral("requestId")))) {
        return std::nullopt;
    }
    const auto operation = operationFromString(object.value(QStringLiteral("op")).toString());
    if (!operation.has_value()) return std::nullopt;

    ServiceRequest request;
    request.requestId = static_cast<quint64>(object.value(QStringLiteral("requestId")).toDouble());
    request.operation = *operation;
    if (*operation == ServiceOperation::Resolve) {
        request.roomId = object.value(QStringLiteral("roomId")).toString();
        const auto quality = qualityFromString(object.value(QStringLiteral("quality")).toString());
        const QJsonValue qualityRate = object.value(QStringLiteral("qualityRate"));
        if (!isValidRoomId(request.roomId) || !quality.has_value()
            || (!qualityRate.isUndefined()
                && (!qualityRate.isDouble() || qualityRate.toInt() < 0
                    || qualityRate.toInt() > 255
                    || qualityRate.toDouble() != qualityRate.toInt()))) {
            return std::nullopt;
        }
        request.quality = *quality;
        request.qualityRate = qualityRate.isUndefined() ? -1 : qualityRate.toInt();
    } else if (*operation == ServiceOperation::Search) {
        request.query = object.value(QStringLiteral("query")).toString().trimmed();
        if (request.query.isEmpty() || request.query.size() > 200) return std::nullopt;
    } else if (*operation == ServiceOperation::Cancel) {
        const QJsonValue target = object.value(QStringLiteral("targetRequestId"));
        if (!isPositiveRequestId(target)) return std::nullopt;
        request.targetRequestId = static_cast<quint64>(target.toDouble());
    }
    return request;
}

std::optional<ServiceResponse> decodeResponse(const QByteArray &line)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return std::nullopt;
    }

    const QJsonObject object = document.object();
    if (!isPositiveRequestId(object.value(QStringLiteral("requestId")))
        || !object.value(QStringLiteral("ok")).isBool()) {
        return std::nullopt;
    }

    ServiceResponse response;
    response.requestId = static_cast<quint64>(object.value(QStringLiteral("requestId")).toDouble());
    response.ok = object.value(QStringLiteral("ok")).toBool();
    if (!response.ok) {
        const QJsonObject error = object.value(QStringLiteral("error")).toObject();
        const QString code = error.value(QStringLiteral("code")).toString();
        if (!kErrorCodes.contains(code) || !error.value(QStringLiteral("retryable")).isBool()
            || error.contains(QStringLiteral("message"))) {
            return std::nullopt;
        }
        response.errorCode = code;
        response.retryable = error.value(QStringLiteral("retryable")).toBool();
        return response;
    }

    if (object.contains(QStringLiteral("pong"))) {
        if (!object.value(QStringLiteral("pong")).isBool()
            || !object.value(QStringLiteral("pong")).toBool()) {
            return std::nullopt;
        }
        response.pong = true;
        return response;
    }

    if (object.contains(QStringLiteral("shutdown"))) {
        if (!object.value(QStringLiteral("shutdown")).isBool()
            || !object.value(QStringLiteral("shutdown")).toBool()) {
            return std::nullopt;
        }
        response.shutdown = true;
        return response;
    }

    if (object.contains(QStringLiteral("cancelled"))) {
        if (!isPositiveRequestId(object.value(QStringLiteral("cancelled")))) {
            return std::nullopt;
        }
        response.cancelledRequestId = static_cast<quint64>(
            object.value(QStringLiteral("cancelled")).toDouble());
        return response;
    }

    if (object.contains(QStringLiteral("results"))) {
        if (!object.value(QStringLiteral("results")).isArray()) return std::nullopt;
        response.search = true;
        const QJsonArray results = object.value(QStringLiteral("results")).toArray();
        for (const QJsonValue &value : results) {
            if (!value.isObject()) return std::nullopt;
            const QJsonObject result = value.toObject();
            RoomSearchResult item;
            item.roomId = result.value(QStringLiteral("roomId")).toString();
            item.anchorName = result.value(QStringLiteral("anchorName")).toString();
            item.title = result.value(QStringLiteral("title")).toString();
            item.category = result.value(QStringLiteral("category")).toString();
            item.viewerLabel = result.value(QStringLiteral("viewerLabel")).toString();
            if (!isValidRoomId(item.roomId) || item.anchorName.isEmpty()
                || !result.value(QStringLiteral("online")).isBool()) {
                return std::nullopt;
            }
            item.online = result.value(QStringLiteral("online")).toBool();
            if (result.contains(QStringLiteral("avatarUrl"))) {
                item.avatarUrl = QUrl(result.value(QStringLiteral("avatarUrl")).toString());
                if (!isSafeHttpUrl(item.avatarUrl)) return std::nullopt;
            }
            response.results.push_back(item);
        }
        return response;
    }

    response.roomId = object.value(QStringLiteral("roomId")).toString();
    response.isLive = object.value(QStringLiteral("isLive")).toBool();
    if (!isValidRoomId(response.roomId) || !object.value(QStringLiteral("isLive")).isBool()
        || !object.value(QStringLiteral("variants")).isArray()) {
        return std::nullopt;
    }

    if (object.contains(QStringLiteral("qualityOptions"))) {
        if (!object.value(QStringLiteral("qualityOptions")).isArray()) return std::nullopt;
        QSet<QString> optionIds;
        QSet<int> optionRates;
        const QJsonArray options = object.value(QStringLiteral("qualityOptions")).toArray();
        for (const QJsonValue &value : options) {
            if (!value.isObject()) return std::nullopt;
            const QJsonObject option = value.toObject();
            const QString id = option.value(QStringLiteral("id")).toString();
            const QString label = option.value(QStringLiteral("label")).toString();
            const QJsonValue rate = option.value(QStringLiteral("rate"));
            if (id.isEmpty() || label.isEmpty() || !rate.isDouble()
                || rate.toInt() < 0 || rate.toInt() > 255
                || rate.toDouble() != rate.toInt()
                || optionIds.contains(id) || optionRates.contains(rate.toInt())) {
                return std::nullopt;
            }
            optionIds.insert(id);
            optionRates.insert(rate.toInt());
            response.qualityOptions.push_back({id, label, rate.toInt()});
        }
    }

    QSet<QString> variantIds;
    const QJsonArray variants = object.value(QStringLiteral("variants")).toArray();
    for (const QJsonValue &value : variants) {
        if (!value.isObject()) return std::nullopt;
        const QJsonObject variant = value.toObject();
        const QString id = variant.value(QStringLiteral("id")).toString();
        const QString label = variant.value(QStringLiteral("label")).toString();
        const auto quality = qualityFromString(variant.value(QStringLiteral("quality")).toString());
        const QJsonValue qualityRate = variant.value(QStringLiteral("qualityRate"));
        const QString container = variant.value(QStringLiteral("container")).toString();
        const QUrl playbackUrl(variant.value(QStringLiteral("playbackUrl")).toString());
        if (id.isEmpty() || label.isEmpty() || container.isEmpty() || !quality.has_value()
            || (!qualityRate.isUndefined()
                && (!qualityRate.isDouble() || qualityRate.toInt() < 0
                    || qualityRate.toInt() > 255
                    || qualityRate.toDouble() != qualityRate.toInt()))
            || !isAllowedPlaybackUrl(playbackUrl) || variantIds.contains(id)) {
            return std::nullopt;
        }
        variantIds.insert(id);
        StreamVariant parsed;
        parsed.id = id;
        parsed.label = label;
        parsed.quality = *quality;
        parsed.qualityRate = qualityRate.isUndefined() ? -1 : qualityRate.toInt();
        parsed.container = container;
        parsed.playbackUrl = playbackUrl;
        response.variants.push_back(std::move(parsed));
    }

    if (response.isLive && response.variants.isEmpty()) return std::nullopt;
    if (!response.isLive && !response.variants.isEmpty()) return std::nullopt;
    return response;
}
