#include "workspace/room_capacity.h"

namespace RoomCapacity {

Limits limitsForStage(Stage stage)
{
    switch (stage) {
    case Stage::Phase24:
        return {
            kPhase24LayoutRooms,
            24,
            24,
            12,
            16,
            4,
            8,
        };
    case Stage::Phase16:
    default:
        return {
            kPhase16LayoutRooms,
            16,
            16,
            12,
            16,
            4,
            8,
        };
    }
}

Limits currentLimits()
{
    return limitsForStage(kReleaseStage);
}

int maxRoomsForLayout(const QString &layoutId)
{
    const QString normalized = layoutId.trimmed().toLower();
    if (normalized == QStringLiteral("primary-two")
        || normalized == QStringLiteral("primary")
        || normalized == QStringLiteral("auto")
        || normalized == QStringLiteral("single")
        || normalized == QStringLiteral("grid-2x2")
        || normalized == QStringLiteral("grid-3x2")
        || normalized == QStringLiteral("grid-3x3")
        || normalized == QStringLiteral("split-horizontal")
        || normalized == QStringLiteral("split-vertical")) {
        return currentLimits().maxLayoutRooms;
    }
    return 0;
}

} // namespace RoomCapacity
