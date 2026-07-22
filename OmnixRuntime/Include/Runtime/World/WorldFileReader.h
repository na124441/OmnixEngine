#pragma once

#include "Runtime/World/WorldDescriptor.h"
#include "Runtime/World/WorldFileResult.h"
#include <filesystem>

#include "Core/Error/Result.h"

namespace Omnix
{
    class WorldFileReader
    {
    public:
        static WorldFileResult ReadFromFile(
            const std::filesystem::path& inputPath,
            WorldDescriptor& outDescriptor
        );

        static eng::core::Expected<WorldDescriptor, WorldFileError> ReadWorld(
            const std::filesystem::path& inputPath
        );
    };
}
