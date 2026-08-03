#pragma once

#include <cstdint>

namespace eng::runtime {

    struct GameplaySaveHeader
    {
        char Magic[8] = { 'O', 'M', 'N', 'S', 'A', 'V', 'E', '\0' };
        uint32_t Version = 1;
        uint64_t PayloadSize = 0;
        uint64_t Checksum = 0;
    };

} // namespace eng::runtime
