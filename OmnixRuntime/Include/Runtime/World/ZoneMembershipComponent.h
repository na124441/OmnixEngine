#pragma once

#include <cstdint>

namespace eng::runtime {

    struct ZoneMembershipComponent
    {
        uint64_t zoneUUIDHigh = 0;
        uint64_t zoneUUIDLow = 0;
    };

} // namespace eng::runtime
