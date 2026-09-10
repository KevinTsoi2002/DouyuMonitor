#include "danmaku/danmaku_controller.h"

#include <QDateTime>

#include <utility>

namespace {

QString stateString(DanmakuConnectionState state)
{
    switch (state) {
    case DanmakuConnectionState::Idle:
        return QStringLiteral("idle");
    case DanmakuConnectionState::Connecting:
        return QStringLiteral("connecting");
    case DanmakuConnectionState::Connected:
        return QStringLiteral("connected");
    case DanmakuConnectionState::Reconnecting:
        return QStringLiteral("reconnecting");
    case DanmakuConnectionState::Failed:
        return QStringLiteral("failed");
    case DanmakuConnectionState::PlatformBlocked:
        return QStringLiteral("platform-blocked");
    }
    return QStringLiteral("idle");
}

QString errorString(DanmakuErrorCode code)
{
    switch (code) {
    case DanmakuErrorCode::None:
        return QStringLiteral("NONE");
    case DanmakuErrorCode::NetworkUnavailable:
        return QStringLiteral("NETWORK_UNAVAILABLE");
    case DanmakuErrorCode::HandshakeTimeout:
        return QStringLiteral("HANDSHAKE_TIMEOUT");
    case DanmakuErrorCode::ProtocolChanged:
        return QStringLiteral("PROTOCOL_CHANGED");
    case DanmakuErrorCode::RetryExhausted:
        return QStringLiteral("RETRY_EXHAUSTED");
    case DanmakuErrorCode::AuthRequired:
        return QStringLiteral("AUTH_REQUIRED");
    }
    return QStringLiteral("NONE");
}

QString regionString(DanmakuRegion region)
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

QString densityString(DanmakuDensity density)
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

QString fontFamilyString(DanmakuFontFamily family)
{
    return family == DanmakuFontFamily::SimHei ? QStringLiteral("simhei")
                                                : QStringLiteral("microsoft-yahei");
}

QString renderingString(DanmakuRendering rendering)
{
    return rendering == DanmakuRendering::Advanced ? QStringLiteral("advanced")
                                                    : QStringLiteral("native");
}

} // namespace

DanmakuController::DanmakuController(DanmakuClientFactory factory, QObject *parent)
    : QObject(parent)
    , sessions_(std::move(factory), this)
{
    connect(&sessions_, &DanmakuSessionManager::roomStateChanged,
            this, &DanmakuController::roomStateChanged);
    connect(&sessions_, &DanmakuSessionManager::messageAvailable,
            this, &DanmakuController::messageAvailable);
}

bool DanmakuController::globalEnabled() const noexcept
{
    return configuration_.globalEnabled;
}

QVariantMap DanmakuController::displaySettings() const
{
    const DanmakuDisplaySettings &display = configuration_.display;
    return {
        {QStringLiteral("durationSeconds"), display.durationSeconds},
        {QStringLiteral("fontSize"), display.fontSize},
        {QStringLiteral("opacity"), display.opacity},
        {QStringLiteral("region"), regionString(display.region)},
        {QStringLiteral("density"), densityString(display.density)},
        {QStringLiteral("fontFamily"), fontFamilyString(display.fontFamily)},
        {QStringLiteral("rendering"), renderingString(display.rendering)},
    };
}

QVariantMap DanmakuController::governanceSettings() const
{
    const DanmakuGovernanceSettings &governance = configuration_.governance;
    return {
        {QStringLiteral("enabled"), governance.enabled},
        {QStringLiteral("keywordBlacklist"), governance.keywordBlacklist},
        {QStringLiteral("duplicateWindowSeconds"), governance.duplicateWindowSeconds},
        {QStringLiteral("peakProtectionEnabled"), governance.peakProtectionEnabled},
    };
}

const NativeDanmakuConfiguration &DanmakuController::configuration() const noexcept
{
    return configuration_;
}

bool DanmakuController::presentationSuspended() const noexcept
{
    return presentationSuspended_;
}

void DanmakuController::setPresentationSuspended(bool suspended)
{
    if (presentationSuspended_ == suspended) return;
    presentationSuspended_ = suspended;
    emit presentationSuspendedChanged();
}

void DanmakuController::setConfiguration(const NativeDanmakuConfiguration &configuration)
{
    NativeDanmakuConfiguration normalized = configuration;
    normalized.display = DanmakuGovernance::validatedDisplaySettings(normalized.display);
    normalized.governance =
        DanmakuGovernance::validatedGovernanceSettings(normalized.governance);
    const NativeDanmakuConfiguration previous = configuration_;
    configuration_ = std::move(normalized);
    emitSettingsIfChanged(previous);
}

void DanmakuController::synchronize(const QVector<DanmakuRoomEligibility> &rooms)
{
    sessions_.synchronize(rooms);
}

void DanmakuController::stopAll()
{
    sessions_.stopAll();
}

void DanmakuController::setGlobalEnabled(bool enabled)
{
    if (configuration_.globalEnabled == enabled) return;
    const NativeDanmakuConfiguration previous = configuration_;
    configuration_.globalEnabled = enabled;
    emitSettingsIfChanged(previous);
}

void DanmakuController::setDisplaySetting(const QString &key, const QVariant &value)
{
    DanmakuDisplaySettings updated = configuration_.display;
    if (key == QStringLiteral("durationSeconds")) updated.durationSeconds = value.toInt();
    else if (key == QStringLiteral("fontSize")) updated.fontSize = value.toInt();
    else if (key == QStringLiteral("opacity")) updated.opacity = value.toDouble();
    else if (key == QStringLiteral("region")) {
        const QString text = value.toString();
        if (text == QStringLiteral("top")) updated.region = DanmakuRegion::Top;
        else if (text == QStringLiteral("bottom")) updated.region = DanmakuRegion::Bottom;
        else if (text == QStringLiteral("full")) updated.region = DanmakuRegion::Full;
        else return;
    } else if (key == QStringLiteral("density")) {
        const QString text = value.toString();
        if (text == QStringLiteral("massive")) updated.density = DanmakuDensity::Massive;
        else if (text == QStringLiteral("reduced")) updated.density = DanmakuDensity::Reduced;
        else if (text == QStringLiteral("normal")) updated.density = DanmakuDensity::Normal;
        else return;
    } else if (key == QStringLiteral("fontFamily")) {
        const QString text = value.toString();
        if (text == QStringLiteral("simhei")) updated.fontFamily = DanmakuFontFamily::SimHei;
        else if (text == QStringLiteral("microsoft-yahei")) {
            updated.fontFamily = DanmakuFontFamily::MicrosoftYaHei;
        } else return;
    } else if (key == QStringLiteral("rendering")) {
        const QString text = value.toString();
        if (text == QStringLiteral("advanced")) updated.rendering = DanmakuRendering::Advanced;
        else if (text == QStringLiteral("native")) updated.rendering = DanmakuRendering::Native;
        else return;
    } else {
        return;
    }
    updated = DanmakuGovernance::validatedDisplaySettings(updated);
    if (updated == configuration_.display) return;
    const NativeDanmakuConfiguration previous = configuration_;
    configuration_.display = updated;
    emitSettingsIfChanged(previous);
}

void DanmakuController::applyGovernanceSetting(DanmakuGovernanceSettings &settings,
                                                const QString &key,
                                                const QVariant &value)
{
    if (key == QStringLiteral("enabled")) settings.enabled = value.toBool();
    else if (key == QStringLiteral("keywordBlacklist")) {
        settings.keywordBlacklist = value.toStringList();
    } else if (key == QStringLiteral("duplicateWindowSeconds")) {
        settings.duplicateWindowSeconds = value.toInt();
    } else if (key == QStringLiteral("peakProtectionEnabled")) {
        settings.peakProtectionEnabled = value.toBool();
    }
}

DanmakuGovernanceOverride DanmakuController::overrideWithSetting(
    DanmakuGovernanceOverride current, const QString &key, const QVariant &value)
{
    if (key == QStringLiteral("enabled")) current.enabled = value.toBool();
    else if (key == QStringLiteral("keywordBlacklist")) current.keywordBlacklist = value.toStringList();
    else if (key == QStringLiteral("duplicateWindowSeconds")) {
        current.duplicateWindowSeconds = value.toInt();
    } else if (key == QStringLiteral("peakProtectionEnabled")) {
        current.peakProtectionEnabled = value.toBool();
    }
    return current;
}

void DanmakuController::setGovernanceSetting(const QString &roomId,
                                             const QString &key,
                                             const QVariant &value)
{
    const NativeDanmakuConfiguration previous = configuration_;
    if (roomId.trimmed().isEmpty()) {
        applyGovernanceSetting(configuration_.governance, key, value);
        configuration_.governance =
            DanmakuGovernance::validatedGovernanceSettings(configuration_.governance);
    } else {
        configuration_.roomOverrides.insert(
            roomId, overrideWithSetting(configuration_.roomOverrides.value(roomId), key, value));
    }
    emitSettingsIfChanged(previous);
}

void DanmakuController::clearRoomGovernanceOverride(const QString &roomId)
{
    if (!configuration_.roomOverrides.contains(roomId)) return;
    const NativeDanmakuConfiguration previous = configuration_;
    configuration_.roomOverrides.remove(roomId);
    emitSettingsIfChanged(previous);
}

QVariantMap DanmakuController::takeNextMessage(const QString &roomId)
{
    if (presentationSuspended_) return {};
    const auto message = sessions_.takeNextMessage(roomId);
    if (!message.has_value()) return {};
    return {
        {QStringLiteral("id"), message->id},
        {QStringLiteral("roomId"), message->roomId},
        {QStringLiteral("nickname"), message->nickname},
        {QStringLiteral("text"), message->text},
        {QStringLiteral("receivedAtUtc"), message->receivedAtUtc.toString(Qt::ISODateWithMs)},
    };
}

QVariantMap DanmakuController::statusForRoom(const QString &roomId) const
{
    const DanmakuConnectionStatus status = sessions_.statusForRoom(roomId);
    return {
        {QStringLiteral("state"), stateString(status.state)},
        {QStringLiteral("errorCode"), errorString(status.errorCode)},
        {QStringLiteral("attempt"), status.attempt},
    };
}

QVariantMap DanmakuController::statsForRoom(const QString &roomId) const
{
    const DanmakuGovernanceStats stats = sessions_.statsForRoom(roomId);
    return {
        {QStringLiteral("level"), stats.level},
        {QStringLiteral("recentRate"), stats.recentRate},
        {QStringLiteral("peakRate"), stats.peakRate},
        {QStringLiteral("filtered"), stats.filtered},
        {QStringLiteral("duplicates"), stats.duplicates},
        {QStringLiteral("rateLimited"), stats.rateLimited},
        {QStringLiteral("queueOverflow"), stats.queueOverflow},
        {QStringLiteral("upstreamDropped"), stats.upstreamDropped},
    };
}

void DanmakuController::clearStats(const QString &roomId)
{
    sessions_.clearStats(roomId);
}

void DanmakuController::retry(const QString &roomId)
{
    sessions_.retry(roomId);
}

void DanmakuController::clearRoom(const QString &roomId)
{
    sessions_.clearRoom(roomId);
}

#ifdef DOUYU_TESTING
int DanmakuController::activeSessionCountForTest() const
{
    return sessions_.activeSessionCount();
}
#endif

void DanmakuController::emitSettingsIfChanged(const NativeDanmakuConfiguration &previous)
{
    if (previous == configuration_) return;
    emit settingsChanged();
}
