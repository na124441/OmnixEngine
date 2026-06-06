#pragma once

#include "Runtime/Public/World/WorldSettingsBlock.h"
#include "Runtime/Public/World/WorldEntryPoint.h"
#include <vector>
#include <string>

namespace Omnix
{
    struct WorldZoneEntry
    {
        uint64_t zoneUUIDHigh = 0;
        uint64_t zoneUUIDLow = 0;

        char zoneName[128] = "";
        char zonePath[256] = "";

        uint32_t flags = 0;
    };

    struct WorldDependencyEntry
    {
        uint64_t assetUUIDHigh = 0;
        uint64_t assetUUIDLow = 0;

        char assetPath[256] = "";

        uint32_t assetType = 0;
    };

    struct WorldDescriptor
    {
        uint64_t worldUUIDHigh = 0;
        uint64_t worldUUIDLow = 0;

        std::string worldName;

        WorldSettingsBlock settings;
        WorldEntryPoint entryPoint;

        std::vector<WorldZoneEntry> zones;
        std::vector<WorldDependencyEntry> dependencies;
    };
}
