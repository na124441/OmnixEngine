#pragma once

#include "Runtime/World/WorldZone.h"
#include <filesystem>

#include "Core/Error/Result.h"

namespace Omnix
{
    class WorldZoneReader
    {
    public:
        static WorldFileResult ReadFromFile(
            const std::filesystem::path& inputPath,
            WorldZone& outZone
        );

        static eng::core::Expected<WorldZone, WorldFileError> ReadZone(
            const std::filesystem::path& inputPath
        );
    };
}
