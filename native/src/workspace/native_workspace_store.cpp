#include "workspace/native_workspace_store.h"

#include "danmaku/danmaku_governance.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>
#include <QSet>

#include <optional>

namespace {

constexpr auto kSettingsKey = "DouyuMonitor/nativeWorkspaceV1";
constexpr int kCurrentVersion = 4;
constexpr int kMaxActiveRooms = 9;
constexpr int kMaxGroupRooms = 9;
const QRegularExpression kRoomIdPattern(QStringLiteral(R"(^[0-9]{1,20}$)"));

bool isSupportedLayoutId(const QString &layoutId)
{
    return layoutId == QStringLiteral("auto") || layoutId == QStringLiteral("single")
        || layoutId == QStringLiteral("grid-2x2") || layoutId == QStringLiteral("grid-3x2")
        || layoutId == QStringLiteral("grid-3x3") || layoutId == QStringLiteral("primary-two")
        || layoutId == QStringLiteral("split-horizontal")
        || layoutId == QStringLiteral("split-vertical");
}

bool isSupportedAudioMode(const QString &audioMode)
{
    return audioMode == QStringLiteral("single") || audioMode == QStringLiteral("multi");
}

double normalizePrimaryRatio(double ratio)
{
    constexpr double kRatios[] = {0.5, 0.6, 0.67};
    double closest = kRatios[0];
    for (double candidate : kRatios) {
        if (qAbs(candidate - ratio) < qAbs(closest - ratio)) closest = candidate;
    }
    return closest;
}

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

QString regionToString(DanmakuRegion region)
{
    switch (region) {
    case DanmakuRegion::Full:
        return QStringLiteral("full");
    case DanmakuRegion::Top:
        return QStringLiteral("top");
    case DanmakuRegion::Bottom:
        return QStringLiteral("bottom");
    }
    return QStringLiteral("full");
}

std::optional<DanmakuRegion> regionFromString(const QString &value)
{
    if (value == QStringLiteral("full")) return DanmakuRegion::Full;
    if (value == QStringLiteral("top")) return DanmakuRegion::Top;
    if (value == QStringLiteral("bottom")) return DanmakuRegion::Bottom;
    return std::nullopt;
}

QString densityToString(DanmakuDensity density)
{
    switch (density) {
    case DanmakuDensity::Massive:
        return QStringLiteral("massive");
    case DanmakuDensity::Normal:
        return QStringLiteral("normal");
    case DanmakuDensity::Reduced:
        return QStringLiteral("reduced");
    }
    return QStringLiteral("normal");
}

std::optional<DanmakuDensity> densityFromString(const QString &value)
{
    if (value == QStringLiteral("massive")) return DanmakuDensity::Massive;
    if (value == QStringLiteral("normal")) return DanmakuDensity::Normal;
    if (value == QStringLiteral("reduced")) return DanmakuDensity::Reduced;
    return std::nullopt;
}

QString fontFamilyToString(DanmakuFontFamily family)
{
    switch (family) {
    case DanmakuFontFamily::SimHei:
        return QStringLiteral("simhei");
    case DanmakuFontFamily::MicrosoftYaHei:
        return QStringLiteral("microsoft-yahei");
    }
    return QStringLiteral("microsoft-yahei");
}

std::optional<DanmakuFontFamily> fontFamilyFromString(const QString &value)
{
    if (value == QStringLiteral("simhei")) return DanmakuFontFamily::SimHei;
    if (value == QStringLiteral("microsoft-yahei")) return DanmakuFontFamily::MicrosoftYaHei;
    return std::nullopt;
}

QString renderingToString(DanmakuRendering rendering)
{
    switch (rendering) {
    case DanmakuRendering::Native:
        return QStringLiteral("native");
    case DanmakuRendering::Advanced:
        return QStringLiteral("advanced");
    }
    return QStringLiteral("native");
}

std::optional<DanmakuRendering> renderingFromString(const QString &value)
{
    if (value == QStringLiteral("native")) return DanmakuRendering::Native;
    if (value == QStringLiteral("advanced")) return DanmakuRendering::Advanced;
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

QStringList normalizePresetRoomIds(const QStringList &roomIds, int limit)
{
    QStringList normalized;
    for (const QString &roomId : roomIds) {
        if (!isValidRoomId(roomId) || normalized.contains(roomId)) continue;
        normalized.push_back(roomId);
        if (normalized.size() == limit) break;
    }
    return normalized;
}

DanmakuGovernanceOverride normalizeOverride(const DanmakuGovernanceOverride &raw)
{
    DanmakuGovernanceOverride result = raw;
    if (result.keywordBlacklist.has_value()) {
        DanmakuGovernanceSettings settings;
        settings.keywordBlacklist = *result.keywordBlacklist;
        result.keywordBlacklist = DanmakuGovernance::validatedGovernanceSettings(settings)
                                      .keywordBlacklist;
    }
    if (result.duplicateWindowSeconds.has_value()) {
        DanmakuGovernanceSettings settings;
        settings.duplicateWindowSeconds = *result.duplicateWindowSeconds;
        result.duplicateWindowSeconds = DanmakuGovernance::validatedGovernanceSettings(settings)
                                             .duplicateWindowSeconds;
    }
    return result;
}

NativeWorkspaceSnapshot normalize(NativeWorkspaceSnapshot snapshot)
{
    snapshot.version = kCurrentVersion;
    if (!isSupportedLayoutId(snapshot.layoutId)) snapshot.layoutId = QStringLiteral("auto");
    if (!isSupportedAudioMode(snapshot.audioMode)) snapshot.audioMode = QStringLiteral("single");
    snapshot.primaryRoomRatio = normalizePrimaryRatio(snapshot.primaryRoomRatio);
    snapshot.danmaku.display = DanmakuGovernance::validatedDisplaySettings(snapshot.danmaku.display);
    snapshot.danmaku.governance =
        DanmakuGovernance::validatedGovernanceSettings(snapshot.danmaku.governance);
    QVector<NativeRoomRecord> library;
    QSet<QString> knownIds;
    for (NativeRoomRecord record : snapshot.library) {
        if (!isValidRoomId(record.roomId) || knownIds.contains(record.roomId)) continue;
        record.metadata.roomId = record.roomId;
        if (!record.metadata.avatarUrl.isEmpty() && !isSafeHttpUrl(record.metadata.avatarUrl)) {
            record.metadata.avatarUrl = QUrl();
        }
        if (record.metadata.anchorName.isEmpty()) record.metadata.anchorName = record.roomId;
        if (record.volume < 0 || record.volume > 100) record.volume = 100;
        if (!record.favorite) {
            record.favoriteAddedAtMs = 0;
            record.favoriteSortOrder = 0;
        } else {
            const qint64 fallbackOrder = qMax<qint64>(1, record.lastOpenedAtMs);
            if (record.favoriteAddedAtMs <= 0) record.favoriteAddedAtMs = fallbackOrder;
            if (record.favoriteSortOrder <= 0) record.favoriteSortOrder = record.favoriteAddedAtMs;
        }
        knownIds.insert(record.roomId);
        library.push_back(std::move(record));
    }
    snapshot.library = std::move(library);
    QMap<QString, DanmakuGovernanceOverride> roomOverrides;
    for (auto it = snapshot.danmaku.roomOverrides.cbegin();
         it != snapshot.danmaku.roomOverrides.cend(); ++it) {
        if (knownIds.contains(it.key())) roomOverrides.insert(it.key(), normalizeOverride(it.value()));
    }
    snapshot.danmaku.roomOverrides = std::move(roomOverrides);
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

    QVector<NativeWorkspacePreset> presets;
    QSet<QString> presetIds;
    for (NativeWorkspacePreset preset : snapshot.presets) {
        preset.id = preset.id.trimmed();
        preset.name = preset.name.trimmed();
        if (preset.id.isEmpty() || preset.name.isEmpty() || preset.name.size() > 30
            || presetIds.contains(preset.id)) {
            continue;
        }
        preset.roomIds = normalizePresetRoomIds(preset.roomIds, kMaxActiveRooms);
        preset.danmaku.display =
            DanmakuGovernance::validatedDisplaySettings(preset.danmaku.display);
        preset.danmaku.governance =
            DanmakuGovernance::validatedGovernanceSettings(preset.danmaku.governance);
        QMap<QString, DanmakuGovernanceOverride> presetOverrides;
        for (auto it = preset.danmaku.roomOverrides.cbegin();
             it != preset.danmaku.roomOverrides.cend(); ++it) {
            if (knownIds.contains(it.key())) {
                presetOverrides.insert(it.key(), normalizeOverride(it.value()));
            }
        }
        preset.danmaku.roomOverrides = std::move(presetOverrides);
        if (!isSupportedAudioMode(preset.audioMode)) preset.audioMode = QStringLiteral("single");
        if (!groupIds.contains(preset.activeGroupId)) preset.activeGroupId.clear();
        if (!preset.roomIds.contains(preset.primaryRoomId)) preset.primaryRoomId.clear();
        if (!preset.roomIds.contains(preset.audioRoomId)) preset.audioRoomId.clear();
        presetIds.insert(preset.id);
        presets.push_back(std::move(preset));
    }
    snapshot.presets = std::move(presets);

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
    object.insert(QStringLiteral("volume"), record.volume);
    object.insert(QStringLiteral("danmakuEnabled"), record.danmakuEnabled);
    object.insert(QStringLiteral("favoriteAddedAtMs"), record.favoriteAddedAtMs);
    object.insert(QStringLiteral("favoriteSortOrder"), record.favoriteSortOrder);
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

QJsonObject toJson(const DanmakuDisplaySettings &settings)
{
    return {
        {QStringLiteral("durationSeconds"), settings.durationSeconds},
        {QStringLiteral("fontSize"), settings.fontSize},
        {QStringLiteral("opacity"), settings.opacity},
        {QStringLiteral("region"), regionToString(settings.region)},
        {QStringLiteral("density"), densityToString(settings.density)},
        {QStringLiteral("fontFamily"), fontFamilyToString(settings.fontFamily)},
        {QStringLiteral("rendering"), renderingToString(settings.rendering)},
    };
}

QJsonObject toJson(const DanmakuGovernanceSettings &settings)
{
    QJsonArray keywords;
    for (const QString &keyword : settings.keywordBlacklist) keywords.append(keyword);
    return {
        {QStringLiteral("enabled"), settings.enabled},
        {QStringLiteral("keywordBlacklist"), keywords},
        {QStringLiteral("duplicateWindowSeconds"), settings.duplicateWindowSeconds},
        {QStringLiteral("peakProtectionEnabled"), settings.peakProtectionEnabled},
    };
}

QJsonObject toJson(const DanmakuGovernanceOverride &override)
{
    QJsonObject object;
    if (override.enabled.has_value()) object.insert(QStringLiteral("enabled"), *override.enabled);
    if (override.keywordBlacklist.has_value()) {
        QJsonArray keywords;
        for (const QString &keyword : *override.keywordBlacklist) keywords.append(keyword);
        object.insert(QStringLiteral("keywordBlacklist"), keywords);
    }
    if (override.duplicateWindowSeconds.has_value()) {
        object.insert(QStringLiteral("duplicateWindowSeconds"),
                      *override.duplicateWindowSeconds);
    }
    if (override.peakProtectionEnabled.has_value()) {
        object.insert(QStringLiteral("peakProtectionEnabled"), *override.peakProtectionEnabled);
    }
    return object;
}

QJsonObject toJson(const NativeDanmakuConfiguration &configuration)
{
    QJsonObject roomOverrides;
    for (auto it = configuration.roomOverrides.cbegin();
         it != configuration.roomOverrides.cend(); ++it) {
        roomOverrides.insert(it.key(), toJson(it.value()));
    }
    return {
        {QStringLiteral("globalEnabled"), configuration.globalEnabled},
        {QStringLiteral("display"), toJson(configuration.display)},
        {QStringLiteral("governance"), toJson(configuration.governance)},
        {QStringLiteral("roomOverrides"), roomOverrides},
    };
}

QJsonObject toJson(const NativeWorkspacePreset &preset)
{
    QJsonArray roomIds;
    for (const QString &roomId : preset.roomIds) roomIds.append(roomId);
    return {
        {QStringLiteral("id"), preset.id},
        {QStringLiteral("name"), preset.name},
        {QStringLiteral("layoutId"), preset.layoutId},
        {QStringLiteral("activeGroupId"), preset.activeGroupId},
        {QStringLiteral("primaryRoomId"), preset.primaryRoomId},
        {QStringLiteral("audioRoomId"), preset.audioRoomId},
        {QStringLiteral("roomIds"), roomIds},
        {QStringLiteral("sidebarVisible"), preset.sidebarVisible},
        {QStringLiteral("primaryRoomRatio"), preset.primaryRoomRatio},
        {QStringLiteral("audioMode"), preset.audioMode},
        {QStringLiteral("globalMuted"), preset.globalMuted},
        {QStringLiteral("danmaku"), toJson(preset.danmaku)},
    };
}

QJsonObject toJson(const NativeWorkspaceSnapshot &snapshot)
{
    QJsonArray library;
    for (const NativeRoomRecord &record : snapshot.library) library.append(toJson(record));
    QJsonArray groups;
    for (const NativeRoomGroup &group : snapshot.groups) groups.append(toJson(group));
    QJsonArray presets;
    for (const NativeWorkspacePreset &preset : snapshot.presets) presets.append(toJson(preset));
    QJsonArray activeRoomIds;
    for (const QString &roomId : snapshot.activeRoomIds) activeRoomIds.append(roomId);
    return {
        {QStringLiteral("version"), snapshot.version},
        {QStringLiteral("library"), library},
        {QStringLiteral("groups"), groups},
        {QStringLiteral("presets"), presets},
        {QStringLiteral("danmaku"), toJson(snapshot.danmaku)},
        {QStringLiteral("activeRoomIds"), activeRoomIds},
        {QStringLiteral("activeGroupId"), snapshot.activeGroupId},
        {QStringLiteral("primaryRoomId"), snapshot.primaryRoomId},
        {QStringLiteral("audioRoomId"), snapshot.audioRoomId},
        {QStringLiteral("layoutId"), snapshot.layoutId},
        {QStringLiteral("primaryRoomRatio"), snapshot.primaryRoomRatio},
        {QStringLiteral("sidebarVisible"), snapshot.sidebarVisible},
        {QStringLiteral("audioMode"), snapshot.audioMode},
        {QStringLiteral("globalMuted"), snapshot.globalMuted},
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
        QStringLiteral("endpoint"),
        QStringLiteral("raw"),
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

std::optional<DanmakuDisplaySettings> displayFromJson(const QJsonObject &object)
{
    if (containsSensitiveKey(object)
        || !object.value(QStringLiteral("durationSeconds")).isDouble()
        || !object.value(QStringLiteral("fontSize")).isDouble()
        || !object.value(QStringLiteral("opacity")).isDouble()
        || !object.value(QStringLiteral("region")).isString()
        || !object.value(QStringLiteral("density")).isString()
        || !object.value(QStringLiteral("fontFamily")).isString()
        || !object.value(QStringLiteral("rendering")).isString()) {
        return std::nullopt;
    }
    const auto region = regionFromString(object.value(QStringLiteral("region")).toString());
    const auto density = densityFromString(object.value(QStringLiteral("density")).toString());
    const auto fontFamily =
        fontFamilyFromString(object.value(QStringLiteral("fontFamily")).toString());
    const auto rendering = renderingFromString(object.value(QStringLiteral("rendering")).toString());
    if (!region.has_value() || !density.has_value() || !fontFamily.has_value()
        || !rendering.has_value()) {
        return std::nullopt;
    }
    DanmakuDisplaySettings settings;
    settings.durationSeconds = object.value(QStringLiteral("durationSeconds")).toInt();
    settings.fontSize = object.value(QStringLiteral("fontSize")).toInt();
    settings.opacity = object.value(QStringLiteral("opacity")).toDouble();
    settings.region = *region;
    settings.density = *density;
    settings.fontFamily = *fontFamily;
    settings.rendering = *rendering;
    return DanmakuGovernance::validatedDisplaySettings(settings);
}

std::optional<QStringList> stringListFromJson(const QJsonValue &value)
{
    if (!value.isArray()) return std::nullopt;
    QStringList values;
    for (const QJsonValue &entry : value.toArray()) {
        if (!entry.isString()) return std::nullopt;
        values.push_back(entry.toString());
    }
    return values;
}

std::optional<DanmakuGovernanceSettings> governanceFromJson(const QJsonObject &object)
{
    if (containsSensitiveKey(object)
        || !object.value(QStringLiteral("enabled")).isBool()
        || !object.value(QStringLiteral("keywordBlacklist")).isArray()
        || !object.value(QStringLiteral("duplicateWindowSeconds")).isDouble()
        || !object.value(QStringLiteral("peakProtectionEnabled")).isBool()) {
        return std::nullopt;
    }
    const auto keywords = stringListFromJson(object.value(QStringLiteral("keywordBlacklist")));
    if (!keywords.has_value()) return std::nullopt;
    DanmakuGovernanceSettings settings;
    settings.enabled = object.value(QStringLiteral("enabled")).toBool();
    settings.keywordBlacklist = *keywords;
    settings.duplicateWindowSeconds =
        object.value(QStringLiteral("duplicateWindowSeconds")).toInt();
    settings.peakProtectionEnabled =
        object.value(QStringLiteral("peakProtectionEnabled")).toBool();
    return DanmakuGovernance::validatedGovernanceSettings(settings);
}

std::optional<DanmakuGovernanceOverride> overrideFromJson(const QJsonObject &object)
{
    if (containsSensitiveKey(object)) return std::nullopt;
    DanmakuGovernanceOverride result;
    if (object.contains(QStringLiteral("enabled"))) {
        if (!object.value(QStringLiteral("enabled")).isBool()) return std::nullopt;
        result.enabled = object.value(QStringLiteral("enabled")).toBool();
    }
    if (object.contains(QStringLiteral("keywordBlacklist"))) {
        const auto keywords = stringListFromJson(object.value(QStringLiteral("keywordBlacklist")));
        if (!keywords.has_value()) return std::nullopt;
        result.keywordBlacklist = *keywords;
    }
    if (object.contains(QStringLiteral("duplicateWindowSeconds"))) {
        if (!object.value(QStringLiteral("duplicateWindowSeconds")).isDouble()) {
            return std::nullopt;
        }
        result.duplicateWindowSeconds =
            object.value(QStringLiteral("duplicateWindowSeconds")).toInt();
    }
    if (object.contains(QStringLiteral("peakProtectionEnabled"))) {
        if (!object.value(QStringLiteral("peakProtectionEnabled")).isBool()) {
            return std::nullopt;
        }
        result.peakProtectionEnabled =
            object.value(QStringLiteral("peakProtectionEnabled")).toBool();
    }
    return normalizeOverride(result);
}

std::optional<NativeDanmakuConfiguration> danmakuFromJson(const QJsonObject &object)
{
    if (containsSensitiveKey(object)
        || !object.value(QStringLiteral("globalEnabled")).isBool()
        || !object.value(QStringLiteral("display")).isObject()
        || !object.value(QStringLiteral("governance")).isObject()
        || !object.value(QStringLiteral("roomOverrides")).isObject()) {
        return std::nullopt;
    }
    const auto display = displayFromJson(object.value(QStringLiteral("display")).toObject());
    const auto governance =
        governanceFromJson(object.value(QStringLiteral("governance")).toObject());
    if (!display.has_value() || !governance.has_value()) return std::nullopt;
    NativeDanmakuConfiguration result;
    result.globalEnabled = object.value(QStringLiteral("globalEnabled")).toBool();
    result.display = *display;
    result.governance = *governance;
    const QJsonObject overrides = object.value(QStringLiteral("roomOverrides")).toObject();
    for (auto it = overrides.constBegin(); it != overrides.constEnd(); ++it) {
        if (!it.value().isObject()) return std::nullopt;
        const auto parsed = overrideFromJson(it.value().toObject());
        if (!parsed.has_value()) return std::nullopt;
        result.roomOverrides.insert(it.key(), *parsed);
    }
    return result;
}

std::optional<NativeRoomRecord> recordFromJson(const QJsonObject &object, int version)
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
    if (version >= 2
        && (!object.value(QStringLiteral("volume")).isDouble()
            || !object.value(QStringLiteral("danmakuEnabled")).isBool())) {
        return std::nullopt;
    }
    if (version >= 4
        && (!object.value(QStringLiteral("favoriteAddedAtMs")).isDouble()
            || !object.value(QStringLiteral("favoriteSortOrder")).isDouble())) {
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
    if (version >= 2) {
        record.volume = object.value(QStringLiteral("volume")).toInt();
        record.danmakuEnabled = object.value(QStringLiteral("danmakuEnabled")).toBool();
    }
    if (version >= 4) {
        record.favoriteAddedAtMs = object.value(QStringLiteral("favoriteAddedAtMs")).toInteger();
        record.favoriteSortOrder = object.value(QStringLiteral("favoriteSortOrder")).toInteger();
    } else if (record.favorite) {
        record.favoriteAddedAtMs = qMax<qint64>(1, record.lastOpenedAtMs);
        record.favoriteSortOrder = record.favoriteAddedAtMs;
    }
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

std::optional<NativeWorkspacePreset> presetFromJson(const QJsonObject &object, int version)
{
    const QJsonValue roomIds = object.value(QStringLiteral("roomIds"));
    if (containsSensitiveKey(object) || !roomIds.isArray()
        || !object.value(QStringLiteral("sidebarVisible")).isBool()) {
        return std::nullopt;
    }
    if (version >= 3 && !object.value(QStringLiteral("danmaku")).isObject()) {
        return std::nullopt;
    }
    if (version == 2 && !object.value(QStringLiteral("danmakuEnabled")).isBool()) {
        return std::nullopt;
    }
    NativeWorkspacePreset preset;
    preset.id = object.value(QStringLiteral("id")).toString();
    preset.name = object.value(QStringLiteral("name")).toString();
    preset.layoutId = object.value(QStringLiteral("layoutId")).toString();
    preset.activeGroupId = object.value(QStringLiteral("activeGroupId")).toString();
    preset.primaryRoomId = object.value(QStringLiteral("primaryRoomId")).toString();
    preset.audioRoomId = object.value(QStringLiteral("audioRoomId")).toString();
    preset.sidebarVisible = object.value(QStringLiteral("sidebarVisible")).toBool();
    if (object.value(QStringLiteral("audioMode")).isString()) {
        preset.audioMode = object.value(QStringLiteral("audioMode")).toString();
    }
    if (object.value(QStringLiteral("globalMuted")).isBool()) {
        preset.globalMuted = object.value(QStringLiteral("globalMuted")).toBool();
    }
    if (object.value(QStringLiteral("primaryRoomRatio")).isDouble()) {
        preset.primaryRoomRatio = normalizePrimaryRatio(
            object.value(QStringLiteral("primaryRoomRatio")).toDouble());
    }
    if (version >= 3) {
        const auto danmaku = danmakuFromJson(object.value(QStringLiteral("danmaku")).toObject());
        if (!danmaku.has_value()) return std::nullopt;
        preset.danmaku = *danmaku;
    } else {
        preset.danmaku.globalEnabled = object.value(QStringLiteral("danmakuEnabled")).toBool();
    }
    for (const QJsonValue &value : roomIds.toArray()) {
        if (!value.isString()) return std::nullopt;
        preset.roomIds.push_back(value.toString());
    }
    return preset;
}

NativeWorkspaceSnapshot fromJson(const QJsonObject &object)
{
    const int version = object.value(QStringLiteral("version")).toInt();
    if ((version != 1 && version != 2 && version != 3 && version != kCurrentVersion)
        || !object.value(QStringLiteral("library")).isArray()
        || !object.value(QStringLiteral("groups")).isArray()
        || !object.value(QStringLiteral("activeRoomIds")).isArray()
        || (version >= 2 && !object.value(QStringLiteral("presets")).isArray())) {
        return {};
    }

    NativeWorkspaceSnapshot snapshot;
    for (const QJsonValue &value : object.value(QStringLiteral("library")).toArray()) {
        if (!value.isObject()) continue;
        const auto record = recordFromJson(value.toObject(), version);
        if (record.has_value()) snapshot.library.push_back(*record);
    }
    for (const QJsonValue &value : object.value(QStringLiteral("groups")).toArray()) {
        if (!value.isObject()) continue;
        const auto group = groupFromJson(value.toObject());
        if (group.has_value()) snapshot.groups.push_back(*group);
    }
    if (version >= 2) {
        for (const QJsonValue &value : object.value(QStringLiteral("presets")).toArray()) {
            if (!value.isObject()) continue;
            const auto preset = presetFromJson(value.toObject(), version);
            if (preset.has_value()) snapshot.presets.push_back(*preset);
        }
    }
    for (const QJsonValue &value : object.value(QStringLiteral("activeRoomIds")).toArray()) {
        if (value.isString()) snapshot.activeRoomIds.push_back(value.toString());
    }
    snapshot.activeGroupId = object.value(QStringLiteral("activeGroupId")).toString();
    snapshot.primaryRoomId = object.value(QStringLiteral("primaryRoomId")).toString();
    snapshot.audioRoomId = object.value(QStringLiteral("audioRoomId")).toString();
    if (object.value(QStringLiteral("layoutId")).isString()) {
        snapshot.layoutId = object.value(QStringLiteral("layoutId")).toString();
    }
    if (object.value(QStringLiteral("primaryRoomRatio")).isDouble()) {
        snapshot.primaryRoomRatio = normalizePrimaryRatio(
            object.value(QStringLiteral("primaryRoomRatio")).toDouble());
    }
    if (object.value(QStringLiteral("sidebarVisible")).isBool()) {
        snapshot.sidebarVisible = object.value(QStringLiteral("sidebarVisible")).toBool();
    }
    if (object.value(QStringLiteral("audioMode")).isString()) {
        snapshot.audioMode = object.value(QStringLiteral("audioMode")).toString();
    }
    if (object.value(QStringLiteral("globalMuted")).isBool()) {
        snapshot.globalMuted = object.value(QStringLiteral("globalMuted")).toBool();
    }
    if (version >= 3) {
        if (!object.value(QStringLiteral("danmaku")).isObject()) return {};
        const auto danmaku = danmakuFromJson(object.value(QStringLiteral("danmaku")).toObject());
        if (!danmaku.has_value()) return {};
        snapshot.danmaku = *danmaku;
    } else {
        snapshot.danmaku.globalEnabled = false;
    }
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
