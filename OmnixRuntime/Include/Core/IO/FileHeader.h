#pragma once
#include <cstdint>

#pragma pack(push, 1)

namespace eng::core {

    struct FileHeader
    {
        char magic[8] = {0};
        uint32_t versionMajor = 0;
        uint32_t versionMinor = 0;
        uint64_t checksum = 0;
        uint64_t fileSize = 0;
    };

} // namespace eng::core

#pragma pack(pop)
