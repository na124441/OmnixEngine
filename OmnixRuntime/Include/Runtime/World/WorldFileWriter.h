#pragma once

#include "Runtime/World/WorldDescriptor.h"
#include "Runtime/World/WorldFileResult.h"
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
