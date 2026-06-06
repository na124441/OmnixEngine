#pragma once

#include "Runtime/Public/World/WorldZone.h"
#include <filesystem>

namespace Omnix
{
    class WorldZoneReader
    {
    public:
        static WorldFileResult ReadFromFile(
            const std::filesystem::path& inputPath,
            WorldZone& outZone
        );
    };
}
