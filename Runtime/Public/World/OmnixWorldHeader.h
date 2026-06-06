#pragma once

#include <cstdint>

namespace Omnix
{
    constexpr char OMNIX_WORLD_MAGIC[8] = { 'O', 'M', 'X', 'W', 'O', 'R', 'L', 'D' };
    constexpr uint32_t OMNIX_WORLD_VERSION = 1;

    struct OmnixWorldHeader
    {
        char magic[8];

        uint32_t version;

        uint64_t worldUUIDHigh;
        uint64_t worldUUIDLow;

        char worldName[128];

        uint32_t zoneCount;

        uint64_t worldSettingsOffset;
        uint64_t entryPointOffset;
        uint64_t zoneTableOffset;
        uint64_t dependencyTableOffset;

        uint32_t dependencyCount;

        uint64_t checksumOffset;
        uint64_t fileSize;

        uint32_t headerSize;

        uint32_t reserved[16];
    };
}
