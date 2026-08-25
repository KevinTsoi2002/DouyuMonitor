#pragma once

#include <QString>

#include "service/stream_service_protocol.h"

struct RoomQualityDecision {
    StreamQuality userQuality = StreamQuality::Auto;
    StreamQuality effectiveQuality = StreamQuality::Auto;
};

RoomQualityDecision resolveRoomQuality(int managedRoomCount,
                                       bool isPrimary,
                                       StreamQuality userQuality);
QString recommendedGridId(int roomCount);
