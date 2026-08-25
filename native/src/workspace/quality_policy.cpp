#include "workspace/quality_policy.h"

RoomQualityDecision resolveRoomQuality(int managedRoomCount,
                                       bool isPrimary,
                                       StreamQuality userQuality)
{
    RoomQualityDecision decision;
    decision.userQuality = userQuality;
    if (managedRoomCount >= 5) {
        decision.effectiveQuality = isPrimary
            ? StreamQuality::Original
            : StreamQuality::Standard;
    } else {
        decision.effectiveQuality = userQuality;
    }
    return decision;
}

QString recommendedGridId(int roomCount)
{
    if (roomCount <= 1) return QStringLiteral("single");
    if (roomCount <= 4) return QStringLiteral("grid-2x2");
    if (roomCount <= 6) return QStringLiteral("grid-3x2");
    return QStringLiteral("grid-3x3");
}
