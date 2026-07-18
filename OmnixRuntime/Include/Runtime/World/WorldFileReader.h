#pragma once

#include "Runtime/World/WorldDescriptor.h"
#include "Runtime/World/WorldFileResult.h"
#include <filesystem>

namespace Omnix
{
    class WorldFileReader
    {
    public:
        static WorldFileResult ReadFromFile(
            const std::filesystem::path& inputPath,
            WorldDescriptor& outDescriptor
        );
    };
}
