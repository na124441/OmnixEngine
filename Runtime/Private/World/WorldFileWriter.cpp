#include "Runtime/Public/World/WorldFileWriter.h"
#include "Runtime/Public/World/OmnixWorldHeader.h"
#include "Runtime/Public/BinaryWriter.h"
#include "Serializer/Serialization/SerializationCommon.h"
#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>

namespace Omnix {

    WorldFileResult WorldFileWriter::WriteToFile(
        const std::filesystem::path& outputPath,
        const WorldDescriptor& descriptor
    ) {
        eng::runtime::BinaryWriter writer;
        
        // Ensure clean buffer state
        writer.GetBuffer().clear();
        writer.Seek(0);

        // 1. Build header in memory with placeholder offsets
        OmnixWorldHeader header;
        std::memset(&header, 0, sizeof(header));
        std::memcpy(header.magic, OMNIX_WORLD_MAGIC, 8);
        header.version = OMNIX_WORLD_VERSION;
        header.worldUUIDHigh = descriptor.worldUUIDHigh;
        header.worldUUIDLow = descriptor.worldUUIDLow;
        
        std::strncpy(header.worldName, descriptor.worldName.c_str(), sizeof(header.worldName) - 1);
        header.worldName[sizeof(header.worldName) - 1] = '\0';
        
        header.zoneCount = static_cast<uint32_t>(descriptor.zones.size());
        header.dependencyCount = static_cast<uint32_t>(descriptor.dependencies.size());
        header.headerSize = sizeof(OmnixWorldHeader);

        // Placed as 0 to begin with
        header.worldSettingsOffset = 0;
        header.entryPointOffset = 0;
        header.zoneTableOffset = 0;
        header.dependencyTableOffset = 0;
        header.checksumOffset = 0;
        header.fileSize = 0;

        // 2. Write placeholder header
        writer.WriteBytes(&header, sizeof(header));

        // 3. Write world settings block
        header.worldSettingsOffset = writer.Tell();
        writer.WriteBytes(&descriptor.settings, sizeof(WorldSettingsBlock));

        // 5. Write entry point block
        header.entryPointOffset = writer.Tell();
        writer.WriteBytes(&descriptor.entryPoint, sizeof(WorldEntryPoint));

        // 7. Write zone table
        header.zoneTableOffset = writer.Tell();
        for (const auto& zone : descriptor.zones) {
            writer.WriteBytes(&zone, sizeof(WorldZoneEntry));
        }

        // 9. Write dependency table
        header.dependencyTableOffset = writer.Tell();
        for (const auto& dep : descriptor.dependencies) {
            writer.WriteBytes(&dep, sizeof(WorldDependencyEntry));
        }

        // 11. Write checksum placeholder
        header.checksumOffset = writer.Tell();
        uint32_t checksumPlaceholder = 0;
        writer.WriteUInt32(checksumPlaceholder);

        // 13. Compute final file size
        header.fileSize = writer.Tell();

        // 14. Seek back and rewrite final header
        writer.Seek(0);
        writer.WriteBytes(&header, sizeof(header));

        // 15. Compute CRC32 (with checksum field at checksumOffset being 0)
        std::vector<uint8_t>& buffer = writer.GetBuffer();
        uint32_t computedChecksum = SerializationCommon::CalculateCRC32(buffer.data(), buffer.size());

        // 16. Seek to checksum offset and write checksum
        writer.Seek(header.checksumOffset);
        writer.WriteUInt32(computedChecksum);

        // Create output directory if it doesn't exist
        std::filesystem::path parentDir = outputPath.parent_path();
        if (!parentDir.empty() && !std::filesystem::exists(parentDir)) {
            std::filesystem::create_directories(parentDir);
        }

        // Write buffer to file
        std::ofstream file(outputPath, std::ios::binary);
        if (!file.is_open()) {
            return WorldFileResult::Fail(WorldFileError::FileOpenFailed);
        }
        file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        if (!file.good()) {
            return WorldFileResult::Fail(WorldFileError::FileWriteFailed);
        }

        return WorldFileResult::Ok();
    }
}
