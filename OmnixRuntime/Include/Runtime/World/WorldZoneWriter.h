#pragma once

#include "Runtime/World/WorldZone.h"
#include <filesystem>

namespace Omnix
{
    class WorldZoneWriter
    {
    public:
        static WorldFileResult WriteToFile(
            const std::filesystem::path& outputPath,
            const WorldZone& zone
        );
    };
}
