#include "workspace/native_workspace_store.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>
#include <QSet>

#include <optional>

namespace {

constexpr auto kSettingsKey = "DouyuMonitor/nativeWorkspaceV1";
constexpr int kMaxActiveRooms = 9;
constexpr int kMaxGroupRooms = 9;
const QRegularExpression kRoomIdPattern(QStringLiteral(R"(^[0-9]{1,20}$)"));

bool isValidRoomId(const QString &roomId)
{
    return kRoomIdPattern.match(roomId).hasMatch();
}

bool isSafeHttpUrl(const QUrl &url)
{
    return url.isValid()
        && (url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https"))
        && !url.host().isEmpty()
        && url.userName().isEmpty()
        && url.password().isEmpty();
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
    return QStringLiteral("auto");
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

QStringList normalizeRoomIds(const QStringList &roomIds, const QSet<QString> &knownIds, int limit)
{
    QStringList normalized;
    for (const QString &roomId : roomIds) {
        if (!knownIds.contains(roomId) || normalized.contains(roomId)) continue;
        normalized.push_back(roomId);
        if (normalized.size() == limit) break;
    }
    return normalized;
}

NativeWorkspaceSnapshot normalize(NativeWorkspaceSnapshot snapshot)
{
    snapshot.version = 1;
    QVector<NativeRoomRecord> library;
    QSet<QString> knownIds;
    for (NativeRoomRecord record : snapshot.library) {
        if (!isValidRoomId(record.roomId) || knownIds.contains(record.roomId)) continue;
        record.metadata.roomId = record.roomId;
        if (!record.metadata.avatarUrl.isEmpty() && !isSafeHttpUrl(record.metadata.avatarUrl)) {
            record.metadata.avatarUrl = QUrl();
        }
        if (record.metadata.anchorName.isEmpty()) record.metadata.anchorName = record.roomId;
        knownIds.insert(record.roomId);
        library.push_back(std::move(record));
    }
    snapshot.library = std::move(library);
    snapshot.activeRoomIds = normalizeRoomIds(snapshot.activeRoomIds, knownIds, kMaxActiveRooms);

    QVector<NativeRoomGroup> groups;
    QSet<QString> groupIds;
    for (NativeRoomGroup group : snapshot.groups) {
        group.id = group.id.trimmed();
        group.name = group.name.trimmed();
        if (group.id.isEmpty() || group.name.isEmpty() || groupIds.contains(group.id)) continue;
        group.roomIds = normalizeRoomIds(group.roomIds, knownIds, kMaxGroupRooms);
        groupIds.insert(group.id);
        groups.push_back(std::move(group));
    }
    snapshot.groups = std::move(groups);

    if (!groupIds.contains(snapshot.activeGroupId)) snapshot.activeGroupId.clear();
    if (!snapshot.activeRoomIds.contains(snapshot.primaryRoomId)) snapshot.primaryRoomId.clear();
    if (!snapshot.activeRoomIds.contains(snapshot.audioRoomId)) snapshot.audioRoomId.clear();
    return snapshot;
}

QJsonObject toJson(const RoomMetadata &metadata)
{
    QJsonObject object;
    object.insert(QStringLiteral("roomId"), metadata.roomId);
    object.insert(QStringLiteral("anchorName"), metadata.anchorName);
    object.insert(QStringLiteral("title"), metadata.title);
    object.insert(QStringLiteral("category"), metadata.category);
    object.insert(QStringLiteral("viewerLabel"), metadata.viewerLabel);
    if (!metadata.avatarUrl.isEmpty()) object.insert(QStringLiteral("avatarUrl"), metadata.avatarUrl.toString());
    return object;
}

QJsonObject toJson(const NativeRoomRecord &record)
{
    QJsonObject object;
    object.insert(QStringLiteral("roomId"), record.roomId);
    object.insert(QStringLiteral("metadata"), toJson(record.metadata));
    object.insert(QStringLiteral("requestedQuality"), qualityToString(record.requestedQuality));
    object.insert(QStringLiteral("favorite"), record.favorite);
    object.insert(QStringLiteral("lastOpenedAtMs"), record.lastOpenedAtMs);
    return object;
}

QJsonObject toJson(const NativeRoomGroup &group)
{
    QJsonArray roomIds;
    for (const QString &roomId : group.roomIds) roomIds.append(roomId);
    return {
        {QStringLiteral("id"), group.id},
        {QStringLiteral("name"), group.name},
        {QStringLiteral("roomIds"), roomIds},
    };
}

QJsonObject toJson(const NativeWorkspaceSnapshot &snapshot)
{
    QJsonArray library;
    for (const NativeRoomRecord &record : snapshot.library) library.append(toJson(record));
    QJsonArray groups;
    for (const NativeRoomGroup &group : snapshot.groups) groups.append(toJson(group));
    QJsonArray activeRoomIds;
    for (const QString &roomId : snapshot.activeRoomIds) activeRoomIds.append(roomId);
    return {
        {QStringLiteral("version"), snapshot.version},
        {QStringLiteral("library"), library},
        {QStringLiteral("groups"), groups},
        {QStringLiteral("activeRoomIds"), activeRoomIds},
        {QStringLiteral("activeGroupId"), snapshot.activeGroupId},
        {QStringLiteral("primaryRoomId"), snapshot.primaryRoomId},
        {QStringLiteral("audioRoomId"), snapshot.audioRoomId},
    };
}

bool containsSensitiveKey(const QJsonObject &object)
{
    static const QSet<QString> sensitiveKeys = {
        QStringLiteral("playbackUrl"),
        QStringLiteral("cookie"),
        QStringLiteral("cookies"),
        QStringLiteral("token"),
        QStringLiteral("signature"),
        QStringLiteral("requestHeaders"),
    };
    for (const QString &key : sensitiveKeys) {
        if (object.contains(key)) return true;
    }
    return false;
}

std::optional<RoomMetadata> metadataFromJson(const QJsonObject &object)
{
    if (containsSensitiveKey(object)) return std::nullopt;
    RoomMetadata metadata;
    metadata.roomId = object.value(QStringLiteral("roomId")).toString();
    metadata.anchorName = object.value(QStringLiteral("anchorName")).toString();
    metadata.title = object.value(QStringLiteral("title")).toString();
    metadata.category = object.value(QStringLiteral("category")).toString();
    metadata.viewerLabel = object.value(QStringLiteral("viewerLabel")).toString();
    if (object.contains(QStringLiteral("avatarUrl"))) {
        metadata.avatarUrl = QUrl(object.value(QStringLiteral("avatarUrl")).toString());
        if (!isSafeHttpUrl(metadata.avatarUrl)) return std::nullopt;
    }
    return metadata;
}

std::optional<NativeRoomRecord> recordFromJson(const QJsonObject &object)
{
    if (containsSensitiveKey(object)) return std::nullopt;
    const QString roomId = object.value(QStringLiteral("roomId")).toString();
    const auto quality = qualityFromString(object.value(QStringLiteral("requestedQuality")).toString());
    if (!isValidRoomId(roomId) || !quality.has_value()
        || !object.value(QStringLiteral("favorite")).isBool()
        || !object.value(QStringLiteral("lastOpenedAtMs")).isDouble()
        || !object.value(QStringLiteral("metadata")).isObject()) {
        return std::nullopt;
    }

    const auto metadata = metadataFromJson(object.value(QStringLiteral("metadata")).toObject());
    if (!metadata.has_value() || metadata->roomId != roomId) return std::nullopt;
    NativeRoomRecord record;
    record.roomId = roomId;
    record.metadata = *metadata;
    record.requestedQuality = *quality;
    record.favorite = object.value(QStringLiteral("favorite")).toBool();
    record.lastOpenedAtMs = object.value(QStringLiteral("lastOpenedAtMs")).toInteger();
    return record;
}

std::optional<NativeRoomGroup> groupFromJson(const QJsonObject &object)
{
    const QJsonValue roomIds = object.value(QStringLiteral("roomIds"));
    if (!roomIds.isArray()) return std::nullopt;
    NativeRoomGroup group;
    group.id = object.value(QStringLiteral("id")).toString();
    group.name = object.value(QStringLiteral("name")).toString();
    for (const QJsonValue &value : roomIds.toArray()) {
        if (!value.isString()) return std::nullopt;
        group.roomIds.push_back(value.toString());
    }
    return group;
}

NativeWorkspaceSnapshot fromJson(const QJsonObject &object)
{
    if (object.value(QStringLiteral("version")).toInt() != 1
        || !object.value(QStringLiteral("library")).isArray()
        || !object.value(QStringLiteral("groups")).isArray()
        || !object.value(QStringLiteral("activeRoomIds")).isArray()) {
        return {};
    }

    NativeWorkspaceSnapshot snapshot;
    for (const QJsonValue &value : object.value(QStringLiteral("library")).toArray()) {
        if (!value.isObject()) continue;
        const auto record = recordFromJson(value.toObject());
        if (record.has_value()) snapshot.library.push_back(*record);
    }
    for (const QJsonValue &value : object.value(QStringLiteral("groups")).toArray()) {
        if (!value.isObject()) continue;
        const auto group = groupFromJson(value.toObject());
        if (group.has_value()) snapshot.groups.push_back(*group);
    }
    for (const QJsonValue &value : object.value(QStringLiteral("activeRoomIds")).toArray()) {
        if (value.isString()) snapshot.activeRoomIds.push_back(value.toString());
    }
    snapshot.activeGroupId = object.value(QStringLiteral("activeGroupId")).toString();
    snapshot.primaryRoomId = object.value(QStringLiteral("primaryRoomId")).toString();
    snapshot.audioRoomId = object.value(QStringLiteral("audioRoomId")).toString();
    return normalize(std::move(snapshot));
}

} // namespace

NativeWorkspaceStore::NativeWorkspaceStore(QSettings *settings)
    : settings_(settings)
{
}

NativeWorkspaceSnapshot NativeWorkspaceStore::load() const
{
    if (settings_ == nullptr) return {};
    const QJsonDocument document = QJsonDocument::fromJson(
        settings_->value(QString::fromLatin1(kSettingsKey)).toByteArray());
    return document.isObject() ? fromJson(document.object()) : NativeWorkspaceSnapshot{};
}

bool NativeWorkspaceStore::save(const NativeWorkspaceSnapshot &snapshot)
{
    if (settings_ == nullptr) return false;
    const NativeWorkspaceSnapshot normalized = normalize(snapshot);
    settings_->setValue(QString::fromLatin1(kSettingsKey),
                        QJsonDocument(toJson(normalized)).toJson(QJsonDocument::Compact));
    settings_->sync();
    return settings_->status() == QSettings::NoError;
}
