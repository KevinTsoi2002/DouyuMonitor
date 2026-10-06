#include "danmaku/danmaku_governance.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr qint64 kInputWindowMs = 3'000;
constexpr qint64 kAcceptedWindowMs = 1'000;
constexpr int kCrowdedLimit = 20;
constexpr int kBurstLimit = 10;
constexpr int kMaxKeywords = 50;
constexpr int kMaxKeywordCodePoints = 40;

QString sanitizeField(QString value)
{
    QString sanitized;
    sanitized.reserve(value.size());
    bool lineBreakSpacePending = false;
    for (const QChar ch : value) {
        const ushort code = ch.unicode();
        if (ch == QLatin1Char('\r') || ch == QLatin1Char('\n')) {
            if (!sanitized.isEmpty() && !lineBreakSpacePending) {
                sanitized += QLatin1Char(' ');
            }
            lineBreakSpacePending = true;
            continue;
        }
        if (code < 0x20 || code == 0x7f) {
            continue;
        }
        sanitized += ch;
        lineBreakSpacePending = false;
    }
    return sanitized.trimmed();
}

QString truncateCodePoints(const QString &value, int maximum)
{
    if (maximum <= 0 || value.isEmpty()) return {};
    const QVector<uint> codePoints = value.toUcs4();
    const qsizetype count = std::min<qsizetype>(codePoints.size(), maximum);
    return QString::fromUcs4(codePoints.constData(), count);
}

QString normalizeComparable(const QString &text)
{
    return text.trimmed().toCaseFolded();
}

void prune(std::deque<DanmakuTimeBucket> &buckets, qint64 &count,
           qint64 nowMs, qint64 windowMs)
{
    const qint64 cutoff = nowMs - windowMs;
    while (!buckets.empty() && buckets.front().timestampMs < cutoff) {
        count -= buckets.front().count;
        buckets.pop_front();
    }
    // System clock corrections must not retain events from the future.
    while (!buckets.empty() && buckets.back().timestampMs > nowMs) {
        count -= buckets.back().count;
        buckets.pop_back();
    }
}

void append(std::deque<DanmakuTimeBucket> &buckets, qint64 &count,
            qint64 nowMs, qint64 amount)
{
    if (amount == 0) return;
    if (!buckets.empty() && buckets.back().timestampMs == nowMs)
        buckets.back().count += amount;
    else
        buckets.push_back({nowMs, amount});
    count += amount;
}

QString peakLevel(qreal rate)
{
    if (!qIsFinite(rate) || rate <= 10.0) return QStringLiteral("normal");
    if (rate <= 30.0) return QStringLiteral("crowded");
    return QStringLiteral("burst");
}

int acceptedLimit(const QString &level)
{
    if (level == QLatin1String("crowded")) return kCrowdedLimit;
    if (level == QLatin1String("burst")) return kBurstLimit;
    return std::numeric_limits<int>::max();
}

QStringList normalizedKeywords(const QStringList &keywords)
{
    QStringList result;
    for (const QString &raw : keywords) {
        const QString keyword = normalizeComparable(
            truncateCodePoints(sanitizeField(raw), kMaxKeywordCodePoints));
        if (keyword.isEmpty() || result.contains(keyword, Qt::CaseInsensitive)) continue;
        result.push_back(keyword);
        if (result.size() >= kMaxKeywords) break;
    }
    return result;
}

DanmakuGovernanceStats defaultStats()
{
    return {};
}

} // namespace

namespace DanmakuGovernance {

std::optional<DanmakuMessage> sanitizeMessage(const QString &roomId,
                                              const QString &id,
                                              const QString &nickname,
                                              const QString &text,
                                              const QDateTime &receivedAtUtc)
{
    DanmakuMessage message;
    message.roomId = truncateCodePoints(sanitizeField(roomId), 200);
    message.id = truncateCodePoints(sanitizeField(id), 200);
    message.nickname = truncateCodePoints(sanitizeField(nickname), 40);
    message.text = truncateCodePoints(sanitizeField(text), 200);
    message.receivedAtUtc = receivedAtUtc.toUTC();
    if (message.roomId.isEmpty() || message.text.isEmpty()) return std::nullopt;
    if (message.nickname.isEmpty()) message.nickname = QStringLiteral("匿名");
    return message;
}

DanmakuGovernanceRuntime createRuntime()
{
    DanmakuGovernanceRuntime runtime;
    runtime.stats = defaultStats();
    return runtime;
}

DanmakuGovernanceRuntime createDanmakuGovernanceRuntime()
{
    return createRuntime();
}

QVector<DanmakuMessage> apply(const QVector<DanmakuMessage> &messages,
                               const DanmakuGovernanceSettings &rawSettings,
                               DanmakuGovernanceRuntime &runtime,
                               const QDateTime &nowUtc)
{
    if (!runtime.rawSettings || *runtime.rawSettings != rawSettings) {
        runtime.rawSettings = rawSettings;
        runtime.validatedSettings = validatedGovernanceSettings(rawSettings);
    }
    const auto &settings = runtime.validatedSettings;
    const qint64 nowMs = nowUtc.toUTC().toMSecsSinceEpoch();
    prune(runtime.inputTimestampsMs, runtime.recentInputCount, nowMs, kInputWindowMs);
    append(runtime.inputTimestampsMs, runtime.recentInputCount, nowMs, messages.size());
    const qreal recentRate = static_cast<qreal>(runtime.recentInputCount) / 3.0;
    runtime.stats.level = peakLevel(recentRate);
    runtime.stats.recentRate = std::round(recentRate * 100.0) / 100.0;
    runtime.stats.peakRate = std::max(runtime.stats.peakRate, runtime.stats.recentRate);
    runtime.peakRate = runtime.stats.peakRate;

    prune(runtime.acceptedTimestampsMs, runtime.acceptedCount, nowMs, kAcceptedWindowMs);
    const auto &keywords = settings.keywordBlacklist;
    const int limit = settings.peakProtectionEnabled ? acceptedLimit(runtime.stats.level)
                                                      : std::numeric_limits<int>::max();
    QVector<DanmakuMessage> accepted;
    for (const DanmakuMessage &message : messages) {
        const QString comparable = normalizeComparable(message.text);
        if (settings.enabled && std::any_of(keywords.cbegin(), keywords.cend(),
                                             [&](const QString &keyword) {
                                                 return comparable.contains(keyword);
                                             })) {
            ++runtime.stats.filtered;
            continue;
        }
        if (settings.enabled && !runtime.lastComparableText.isEmpty()
            && comparable == runtime.lastComparableText && runtime.lastComparableAtMs >= 0
            && nowMs - runtime.lastComparableAtMs
                   < static_cast<qint64>(settings.duplicateWindowSeconds) * 1'000) {
            ++runtime.stats.duplicates;
            continue;
        }
        if (settings.enabled) {
            runtime.lastComparableText = comparable;
            runtime.lastComparableAtMs = nowMs;
        }
        if (settings.enabled && runtime.acceptedCount >= limit) {
            ++runtime.stats.rateLimited;
            continue;
        }
        accepted.push_back(message);
        append(runtime.acceptedTimestampsMs, runtime.acceptedCount, nowMs, 1);
    }
    return accepted;
}

DanmakuDisplaySettings validatedDisplaySettings(const DanmakuDisplaySettings &settings)
{
    DanmakuDisplaySettings result = settings;
    result.durationSeconds = std::clamp(result.durationSeconds, 4, 15);
    result.fontSize = std::clamp(result.fontSize, 14, 36);
    result.opacity = std::clamp(result.opacity, qreal(0.3), qreal(1.0));
    return result;
}

DanmakuGovernanceSettings validatedGovernanceSettings(
    const DanmakuGovernanceSettings &settings)
{
    DanmakuGovernanceSettings result = settings;
    result.duplicateWindowSeconds = std::clamp(result.duplicateWindowSeconds, 1, 10);
    result.keywordBlacklist = normalizedKeywords(result.keywordBlacklist);
    return result;
}

DanmakuGovernanceSettings resolvedGovernance(const DanmakuGovernanceSettings &global,
                                             const DanmakuGovernanceOverride &override)
{
    DanmakuGovernanceSettings result = validatedGovernanceSettings(global);
    if (override.enabled.has_value()) result.enabled = *override.enabled;
    if (override.keywordBlacklist.has_value()) {
        result.keywordBlacklist = *override.keywordBlacklist;
    }
    if (override.duplicateWindowSeconds.has_value()) {
        result.duplicateWindowSeconds = *override.duplicateWindowSeconds;
    }
    if (override.peakProtectionEnabled.has_value()) {
        result.peakProtectionEnabled = *override.peakProtectionEnabled;
    }
    return validatedGovernanceSettings(result);
}

} // namespace DanmakuGovernance
