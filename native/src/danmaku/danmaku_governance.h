#pragma once

#include "danmaku/danmaku_types.h"

#include <QDateTime>
#include <QVector>

struct DanmakuGovernanceRuntime {
    QVector<qint64> inputTimestampsMs;
    QVector<qint64> acceptedTimestampsMs;
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
