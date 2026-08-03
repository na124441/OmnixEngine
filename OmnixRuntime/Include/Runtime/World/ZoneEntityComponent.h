#pragma once

#include <cstdint>

namespace eng::runtime {

    struct ZoneEntityComponent
    {
        uint64_t zoneUUIDHigh = 0;
        uint64_t zoneUUIDLow = 0;
        bool simulating = true;
    };

} // namespace eng::runtime
