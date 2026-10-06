#pragma once

#include "danmaku/danmaku_types.h"

#include <QDateTime>
#include <QVector>
#include <deque>

struct DanmakuTimeBucket {
    qint64 timestampMs = 0;
    qint64 count = 0;
};

struct DanmakuGovernanceRuntime {
    std::deque<DanmakuTimeBucket> inputTimestampsMs;
    std::deque<DanmakuTimeBucket> acceptedTimestampsMs;
    qint64 recentInputCount = 0;
    qint64 acceptedCount = 0;
    std::optional<DanmakuGovernanceSettings> rawSettings;
    DanmakuGovernanceSettings validatedSettings;
    QString lastComparableText;
    qint64 lastComparableAtMs = -1;
    qreal peakRate = 0;
    DanmakuGovernanceStats stats;
};

namespace DanmakuGovernance {

std::optional<DanmakuMessage> sanitizeMessage(const QString &roomId,
                                              const QString &id,
                                              const QString &nickname,
                                              const QString &text,
                                              const QDateTime &receivedAtUtc);

DanmakuGovernanceRuntime createRuntime();
DanmakuGovernanceRuntime createDanmakuGovernanceRuntime();

QVector<DanmakuMessage> apply(const QVector<DanmakuMessage> &messages,
                               const DanmakuGovernanceSettings &settings,
                               DanmakuGovernanceRuntime &runtime,
                               const QDateTime &nowUtc);

DanmakuDisplaySettings validatedDisplaySettings(const DanmakuDisplaySettings &settings);
DanmakuGovernanceSettings validatedGovernanceSettings(
    const DanmakuGovernanceSettings &settings);
DanmakuGovernanceSettings resolvedGovernance(const DanmakuGovernanceSettings &global,
                                             const DanmakuGovernanceOverride &override);

} // namespace DanmakuGovernance
