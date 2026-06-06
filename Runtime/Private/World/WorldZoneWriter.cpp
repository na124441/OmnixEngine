#include "Runtime/Public/World/WorldZoneWriter.h"
#include "Runtime/Public/World/OmnixZoneHeader.h"
#include "Runtime/Public/BinaryWriter.h"
#include "Serializer/Serialization/SerializationCommon.h"
#include <fstream>
#include <cstring>
#include <vector>

namespace Omnix
{

    WorldFileResult WorldZoneWriter::WriteToFile(
        const std::filesystem::path& outputPath,
        const WorldZone& zone
    ) {
        eng::runtime::BinaryWriter writer;
        writer.GetBuffer().clear();
        writer.Seek(0);

        // 1. Initialize header
        OmnixZoneHeader header;
        std::memset(&header, 0, sizeof(header));
        std::memcpy(header.magic, OMNIX_ZONE_MAGIC, 8);
        header.version = OMNIX_ZONE_VERSION;
        header.zoneUUIDHigh = zone.zoneUUIDHigh;
        header.zoneUUIDLow = zone.zoneUUIDLow;

        std::strncpy(header.zoneName, zone.zoneName.c_str(), sizeof(header.zoneName) - 1);
        header.zoneName[sizeof(header.zoneName) - 1] = '\0';

        std::strncpy(header.sceneAssetPath, zone.sceneAssetPath.c_str(), sizeof(header.sceneAssetPath) - 1);
        header.sceneAssetPath[sizeof(header.sceneAssetPath) - 1] = '\0';

        header.bounds = zone.bounds;
        header.loadingPriority = zone.loadingPriority;
        header.activationRadius = zone.activationRadius;

        header.assetDependencyCount = static_cast<uint32_t>(zone.assetDependencies.size());
        header.neighborCount = static_cast<uint32_t>(zone.neighbors.size());
        header.tagCount = static_cast<uint32_t>(zone.gameplayTags.size());
        header.headerSize = sizeof(OmnixZoneHeader);

        // 2. Write placeholder header
        writer.WriteBytes(&header, sizeof(header));

        // 3. Write asset dependencies
        header.assetDependencyTableOffset = writer.Tell();
        for (const auto& dep : zone.assetDependencies)
        {
            SerializedZoneAssetDependency sDep;
            std::memset(&sDep, 0, sizeof(sDep));
            sDep.assetUUIDHigh = dep.assetUUIDHigh;
            sDep.assetUUIDLow = dep.assetUUIDLow;
            std::strncpy(sDep.assetPath, dep.assetPath.c_str(), sizeof(sDep.assetPath) - 1);
            sDep.assetPath[sizeof(sDep.assetPath) - 1] = '\0';
            sDep.assetType = dep.assetType;

            writer.WriteBytes(&sDep, sizeof(sDep));
        }

        // 4. Write neighbors
        header.neighborTableOffset = writer.Tell();
        for (const auto& neighbor : zone.neighbors)
        {
            SerializedZoneNeighbor sNeighbor;
            sNeighbor.zoneUUIDHigh = neighbor.zoneUUIDHigh;
            sNeighbor.zoneUUIDLow = neighbor.zoneUUIDLow;

            writer.WriteBytes(&sNeighbor, sizeof(sNeighbor));
        }

        // 5. Write gameplay tags (fixed size of 64 bytes)
        header.tagTableOffset = writer.Tell();
        for (const auto& tag : zone.gameplayTags)
        {
            char serializedTag[64];
            std::memset(serializedTag, 0, sizeof(serializedTag));
            std::strncpy(serializedTag, tag.c_str(), sizeof(serializedTag) - 1);
            serializedTag[sizeof(serializedTag) - 1] = '\0';

            writer.WriteBytes(serializedTag, sizeof(serializedTag));
        }

        // 6. Write checksum placeholder
        header.checksumOffset = writer.Tell();
        uint32_t checksumPlaceholder = 0;
        writer.WriteUInt32(checksumPlaceholder);

        // 7. Write total size
        header.fileSize = writer.Tell();

        // 8. Seek back and rewrite header
        writer.Seek(0);
        writer.WriteBytes(&header, sizeof(header));

        // 9. Compute CRC32
        std::vector<uint8_t>& buffer = writer.GetBuffer();
        uint32_t computedChecksum = SerializationCommon::CalculateCRC32(buffer.data(), buffer.size());

        // 10. Seek to checksum offset and write
        writer.Seek(header.checksumOffset);
        writer.WriteUInt32(computedChecksum);

        // Create directory
        std::filesystem::path parentDir = outputPath.parent_path();
        if (!parentDir.empty() && !std::filesystem::exists(parentDir))
        {
            std::filesystem::create_directories(parentDir);
        }

        // Write to file
        std::ofstream file(outputPath, std::ios::binary);
        if (!file.is_open())
        {
            return WorldFileResult::Fail(WorldFileError::FileOpenFailed);
        }
        file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        if (!file.good())
        {
            return WorldFileResult::Fail(WorldFileError::FileWriteFailed);
        }

        return WorldFileResult::Ok();
    }
}
