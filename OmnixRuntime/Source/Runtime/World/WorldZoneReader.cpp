#include "Runtime/World/WorldZoneReader.h"
#include "Runtime/World/OmnixZoneHeader.h"
#include "Runtime/BinaryReader.h"
#include "Serializer/Serialization/SerializationCommon.h"
#include <fstream>
#include <cstring>
#include <vector>

namespace Omnix
{

    WorldFileResult WorldZoneReader::ReadFromFile(
        const std::filesystem::path& inputPath,
        WorldZone& outZone
    ) {
        if (!std::filesystem::exists(inputPath))
        {
            return WorldFileResult::Fail(WorldFileError::FileNotFound);
        }

        std::ifstream file(inputPath, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            return WorldFileResult::Fail(WorldFileError::FileOpenFailed);
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> fileBytes(static_cast<size_t>(size));
        if (size > 0)
        {
            if (!file.read(reinterpret_cast<char*>(fileBytes.data()), size))
            {
                return WorldFileResult::Fail(WorldFileError::FileReadFailed);
            }
        }
        file.close();

        size_t fileSize = fileBytes.size();
        if (fileSize < sizeof(OmnixZoneHeader))
        {
            return WorldFileResult::Fail(WorldFileError::FileTooSmall);
        }

        // Parse header
        OmnixZoneHeader header;
        std::memcpy(&header, fileBytes.data(), sizeof(OmnixZoneHeader));

        // Validate magic
        if (std::memcmp(header.magic, OMNIX_ZONE_MAGIC, 8) != 0)
        {
            return WorldFileResult::Fail(WorldFileError::InvalidMagic);
        }

        // Validate version
        if (header.version > OMNIX_ZONE_VERSION)
        {
            return WorldFileResult::Fail(WorldFileError::UnsupportedVersion);
        }

        // Validate header size
        if (header.headerSize != sizeof(OmnixZoneHeader))
        {
            return WorldFileResult::Fail(WorldFileError::InvalidHeaderSize);
        }

        // Validate offsets
        uint64_t depEnd = header.assetDependencyTableOffset + static_cast<uint64_t>(header.assetDependencyCount) * sizeof(SerializedZoneAssetDependency);
        uint64_t neighborEnd = header.neighborTableOffset + static_cast<uint64_t>(header.neighborCount) * sizeof(SerializedZoneNeighbor);
        uint64_t tagEnd = header.tagTableOffset + static_cast<uint64_t>(header.tagCount) * 64;
        uint64_t checksumEnd = header.checksumOffset + sizeof(uint32_t);

        if (header.assetDependencyTableOffset < sizeof(OmnixZoneHeader) || depEnd > fileSize ||
            header.neighborTableOffset < sizeof(OmnixZoneHeader) || neighborEnd > fileSize ||
            header.tagTableOffset < sizeof(OmnixZoneHeader) || tagEnd > fileSize ||
            header.checksumOffset < sizeof(OmnixZoneHeader) || checksumEnd > fileSize)
        {
            return WorldFileResult::Fail(WorldFileError::InvalidOffset);
        }

        // Validate limits
        constexpr uint32_t MAX_ZONE_DEPENDENCIES = 65536;
        if (header.assetDependencyCount > MAX_ZONE_DEPENDENCIES)
        {
            return WorldFileResult::Fail(WorldFileError::DependencyCountTooLarge);
        }

        constexpr uint32_t MAX_ZONE_NEIGHBORS = 4096;
        if (header.neighborCount > MAX_ZONE_NEIGHBORS)
        {
            return WorldFileResult::Fail(WorldFileError::ZoneCountTooLarge); // Reusing error code
        }

        // Validate Checksum
        uint32_t storedChecksum = *reinterpret_cast<const uint32_t*>(fileBytes.data() + header.checksumOffset);
        std::memset(fileBytes.data() + header.checksumOffset, 0, sizeof(uint32_t));

        uint32_t computedChecksum = SerializationCommon::CalculateCRC32(fileBytes.data(), fileBytes.size());
        std::memcpy(fileBytes.data() + header.checksumOffset, &storedChecksum, sizeof(uint32_t));

        if (computedChecksum != storedChecksum)
        {
            return WorldFileResult::Fail(WorldFileError::ChecksumMismatch);
        }

        // Deserialization using BinaryReader
        eng::runtime::BinaryReader reader;
        reader.LoadFromMemory(fileBytes.data(), fileBytes.size());

        // Read asset dependencies
        std::vector<ZoneAssetDependency> dependencies;
        dependencies.reserve(header.assetDependencyCount);
        reader.Seek(header.assetDependencyTableOffset);
        for (uint32_t i = 0; i < header.assetDependencyCount; ++i)
        {
            SerializedZoneAssetDependency sDep;
            reader.ReadBytes(&sDep, sizeof(SerializedZoneAssetDependency));

            ZoneAssetDependency dep;
            dep.assetUUIDHigh = sDep.assetUUIDHigh;
            dep.assetUUIDLow = sDep.assetUUIDLow;
            dep.assetPath = sDep.assetPath;
            dep.assetType = sDep.assetType;

            dependencies.push_back(dep);
        }

        // Read neighbors
        std::vector<ZoneNeighbor> neighbors;
        neighbors.reserve(header.neighborCount);
        reader.Seek(header.neighborTableOffset);
        for (uint32_t i = 0; i < header.neighborCount; ++i)
        {
            SerializedZoneNeighbor sNeighbor;
            reader.ReadBytes(&sNeighbor, sizeof(SerializedZoneNeighbor));

            ZoneNeighbor neighbor;
            neighbor.zoneUUIDHigh = sNeighbor.zoneUUIDHigh;
            neighbor.zoneUUIDLow = sNeighbor.zoneUUIDLow;

            neighbors.push_back(neighbor);
        }

        // Read tags
        std::vector<std::string> tags;
        tags.reserve(header.tagCount);
        reader.Seek(header.tagTableOffset);
        for (uint32_t i = 0; i < header.tagCount; ++i)
        {
            char serializedTag[64];
            reader.ReadBytes(serializedTag, 64);
            tags.push_back(std::string(serializedTag));
        }

        // Populate outZone
        outZone.zoneUUIDHigh = header.zoneUUIDHigh;
        outZone.zoneUUIDLow = header.zoneUUIDLow;
        outZone.zoneName = header.zoneName;
        outZone.sceneAssetPath = header.sceneAssetPath;
        outZone.bounds = header.bounds;
        outZone.loadingPriority = header.loadingPriority;
        outZone.activationRadius = header.activationRadius;
        outZone.assetDependencies = dependencies;
        outZone.neighbors = neighbors;
        outZone.gameplayTags = tags;
        outZone.state = ZoneState::Unloaded; // Start at Unloaded on load

        return WorldFileResult::Ok();
    }

    eng::core::Expected<WorldZone, WorldFileError> WorldZoneReader::ReadZone(const std::filesystem::path& inputPath)
    {
        WorldZone zone;
        WorldFileResult res = ReadFromFile(inputPath, zone);
        if (res.Success()) {
            return zone;
        }
        return res.error;
    }
}
