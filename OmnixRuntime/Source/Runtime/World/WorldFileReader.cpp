#include "Runtime/World/WorldFileReader.h"
#include "Runtime/World/OmnixWorldHeader.h"
#include "Runtime/BinaryReader.h"
#include "Serializer/Serialization/SerializationCommon.h"
#include <fstream>
#include <cstring>
#include <vector>

namespace Omnix {

    WorldFileResult WorldFileReader::ReadFromFile(
        const std::filesystem::path& inputPath,
        WorldDescriptor& outDescriptor
    ) {
        if (!std::filesystem::exists(inputPath)) {
            return WorldFileResult::Fail(WorldFileError::FileNotFound);
        }

        std::ifstream file(inputPath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return WorldFileResult::Fail(WorldFileError::FileOpenFailed);
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> fileBytes(static_cast<size_t>(size));
        if (size > 0) {
            if (!file.read(reinterpret_cast<char*>(fileBytes.data()), size)) {
                return WorldFileResult::Fail(WorldFileError::FileReadFailed);
            }
        }
        file.close();

        size_t fileSize = fileBytes.size();
        if (fileSize < sizeof(OmnixWorldHeader)) {
            return WorldFileResult::Fail(WorldFileError::FileTooSmall);
        }

        // Parse header
        OmnixWorldHeader header;
        std::memcpy(&header, fileBytes.data(), sizeof(OmnixWorldHeader));

        // Validate magic
        if (std::memcmp(header.magic, OMNIX_WORLD_MAGIC, 8) != 0) {
            return WorldFileResult::Fail(WorldFileError::InvalidMagic);
        }

        // Validate version
        if (header.version > OMNIX_WORLD_VERSION) {
            return WorldFileResult::Fail(WorldFileError::UnsupportedVersion);
        }

        // Validate header size
        if (header.headerSize != sizeof(OmnixWorldHeader)) {
            return WorldFileResult::Fail(WorldFileError::InvalidHeaderSize);
        }

        // Validate offsets
        uint64_t settingsEnd = header.worldSettingsOffset + sizeof(WorldSettingsBlock);
        uint64_t entryPointEnd = header.entryPointOffset + sizeof(WorldEntryPoint);
        uint64_t zoneTableEnd = header.zoneTableOffset + static_cast<uint64_t>(header.zoneCount) * sizeof(WorldZoneEntry);
        uint64_t dependencyTableEnd = header.dependencyTableOffset + static_cast<uint64_t>(header.dependencyCount) * sizeof(WorldDependencyEntry);
        uint64_t checksumEnd = header.checksumOffset + sizeof(uint32_t);

        if (header.worldSettingsOffset < sizeof(OmnixWorldHeader) || settingsEnd > fileSize ||
            header.entryPointOffset < sizeof(OmnixWorldHeader) || entryPointEnd > fileSize ||
            header.zoneTableOffset < sizeof(OmnixWorldHeader) || zoneTableEnd > fileSize ||
            header.dependencyTableOffset < sizeof(OmnixWorldHeader) || dependencyTableEnd > fileSize ||
            header.checksumOffset < sizeof(OmnixWorldHeader) || checksumEnd > fileSize) {
            return WorldFileResult::Fail(WorldFileError::InvalidOffset);
        }

        // Validate count limits
        constexpr uint32_t MAX_WORLD_ZONES = 4096;
        if (header.zoneCount > MAX_WORLD_ZONES) {
            return WorldFileResult::Fail(WorldFileError::ZoneCountTooLarge);
        }

        constexpr uint32_t MAX_WORLD_DEPENDENCIES = 65536;
        if (header.dependencyCount > MAX_WORLD_DEPENDENCIES) {
            return WorldFileResult::Fail(WorldFileError::DependencyCountTooLarge);
        }

        // Validate checksum
        uint32_t storedChecksum = *reinterpret_cast<const uint32_t*>(fileBytes.data() + header.checksumOffset);
        
        // Zero out checksum bytes in fileBytes temporarily
        std::memset(fileBytes.data() + header.checksumOffset, 0, sizeof(uint32_t));
        
        uint32_t computedChecksum = SerializationCommon::CalculateCRC32(fileBytes.data(), fileBytes.size());
        
        // Restore storedChecksum
        std::memcpy(fileBytes.data() + header.checksumOffset, &storedChecksum, sizeof(uint32_t));

        if (computedChecksum != storedChecksum) {
            return WorldFileResult::Fail(WorldFileError::ChecksumMismatch);
        }

        // Load into BinaryReader for component deserialization
        eng::runtime::BinaryReader reader;
        reader.LoadFromMemory(fileBytes.data(), fileBytes.size());

        // Read settings
        reader.Seek(header.worldSettingsOffset);
        WorldSettingsBlock settings;
        reader.ReadBytes(&settings, sizeof(WorldSettingsBlock));

        // Read entry point
        reader.Seek(header.entryPointOffset);
        WorldEntryPoint entryPoint;
        reader.ReadBytes(&entryPoint, sizeof(WorldEntryPoint));

        // Read zones
        std::vector<WorldZoneEntry> zones(header.zoneCount);
        reader.Seek(header.zoneTableOffset);
        for (uint32_t i = 0; i < header.zoneCount; ++i) {
            reader.ReadBytes(&zones[i], sizeof(WorldZoneEntry));
        }

        // Read dependencies
        std::vector<WorldDependencyEntry> dependencies(header.dependencyCount);
        reader.Seek(header.dependencyTableOffset);
        for (uint32_t i = 0; i < header.dependencyCount; ++i) {
            reader.ReadBytes(&dependencies[i], sizeof(WorldDependencyEntry));
        }

        // Populate descriptor
        outDescriptor.worldUUIDHigh = header.worldUUIDHigh;
        outDescriptor.worldUUIDLow = header.worldUUIDLow;
        outDescriptor.worldName = header.worldName;
        outDescriptor.settings = settings;
        outDescriptor.entryPoint = entryPoint;
        outDescriptor.zones = zones;
        outDescriptor.dependencies = dependencies;
        return WorldFileResult::Ok();
    }

    eng::core::Expected<WorldDescriptor, WorldFileError> WorldFileReader::ReadWorld(const std::filesystem::path& inputPath)
    {
        WorldDescriptor desc;
        WorldFileResult res = ReadFromFile(inputPath, desc);
        if (res.Success()) {
            return desc;
        }
        return res.error;
    }
}
