#pragma once

#include "Runtime/Public/World/WorldDescriptor.h"
#include "Runtime/Public/World/WorldFileResult.h"
#include <filesystem>

namespace Omnix
{
    class WorldFileWriter
    {
    public:
        static WorldFileResult WriteToFile(
            const std::filesystem::path& outputPath,
            const WorldDescriptor& descriptor
        );
    };
}
