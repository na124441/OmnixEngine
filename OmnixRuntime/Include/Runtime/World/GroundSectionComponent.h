#pragma once

#include <cstdint>
#include <string>
#include "../../Scene/Vector3.h"

namespace eng::runtime {

    struct GroundSectionComponent
    {
        uint64_t zoneUUIDHigh = 0;
        uint64_t zoneUUIDLow = 0;
        std::string meshAssetPath;
        std::string materialAssetPath;
        std::string collisionAssetPath;
        Vector3 boundsMin = {-10.0f, -1.0f, -10.0f};
        Vector3 boundsMax = {10.0f, 1.0f, 10.0f};
        bool debugDraw = true;
    };

} // namespace eng::runtime
