#pragma once

#include "Runtime/Public/World/WorldDescriptor.h"
#include "Runtime/Public/World/WorldFileResult.h"
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
