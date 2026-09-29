#pragma once

#include <QString>

namespace RoomCapacity {

enum class Stage {
    Phase16,
    Phase24,
};

struct Limits {
    int maxLayoutRooms = 16;
    int defaultDecodedRooms = 16;
    int maxDecodedRooms = 16;
    int defaultDanmakuSessions = 12;
    int maxDanmakuSessions = 16;
    int defaultMultiAudioRooms = 4;
    int maxMultiAudioRooms = 8;
};

inline constexpr int kPhase16LayoutRooms = 16;
inline constexpr int kPhase24LayoutRooms = 24;

#if defined(DOUYU_ROOM_CAPACITY_PHASE24)
inline constexpr Stage kReleaseStage = Stage::Phase24;
#else
inline constexpr Stage kReleaseStage = Stage::Phase16;
#endif

inline constexpr int kLayoutRooms = kReleaseStage == Stage::Phase24
                                        ? kPhase24LayoutRooms
                                        : kPhase16LayoutRooms;

Limits limitsForStage(Stage stage);
Limits currentLimits();
int maxRoomsForLayout(const QString &layoutId);

} // namespace RoomCapacity
