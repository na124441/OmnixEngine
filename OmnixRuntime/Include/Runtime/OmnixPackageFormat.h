#pragma once
#include "Runtime/FileHeader.h"
#include "Runtime/AssetHandle.h"
#include "Runtime/AssetType.h"
#include "Runtime/BinaryReader.h"
#include "Runtime/BinaryWriter.h"
#include <vector>
#include <string>

#pragma pack(push, 1)

struct PackageAssetEntry
{
    AssetHandle handle;
    uint32_t type = 0; // AssetType cast to uint32_t for packing

    uint64_t dataOffset = 0;
    uint64_t dataSize = 0;

    uint32_t compressionType = 0;
    uint32_t checksum = 0;

    bool operator==(const PackageAssetEntry& o) const noexcept {
        return handle == o.handle && type == o.type && dataOffset == o.dataOffset &&
               dataSize == o.dataSize && compressionType == o.compressionType && checksum == o.checksum;
    }
};

struct PackageDependencyEntry
{
    AssetHandle assetHandle;
    AssetHandle dependentHandle;

    bool operator==(const PackageDependencyEntry& o) const noexcept {
        return assetHandle == o.assetHandle && dependentHandle == o.dependentHandle;
    }
};

struct PackageChunkEntry
{
    uint32_t chunkID = 0;
    uint64_t offset = 0;
    uint64_t size = 0;
    uint32_t compressionType = 0;

    bool operator==(const PackageChunkEntry& o) const noexcept {
        return chunkID == o.chunkID && offset == o.offset && size == o.size && compressionType == o.compressionType;
    }
};

struct OmnixPackageHeader
{
    FileHeader file;

    uint32_t assetCount = 0;
    uint32_t dependencyCount = 0;
    uint32_t chunkCount = 0;

    uint64_t assetTableOffset = 0;
    uint64_t dependencyTableOffset = 0;
    uint64_t chunkTableOffset = 0;
    uint64_t dataBlockOffset = 0;
};

#pragma pack(pop)

constexpr char MAGIC_PACK[8] = {'O', 'M', 'X', 'P', 'A', 'C', 'K', '\0'};
constexpr uint32_t OMNIX_PACKAGE_VERSION_MAJOR = 1;
constexpr uint32_t OMNIX_PACKAGE_VERSION_MINOR = 0;

struct OmnixPackage
{
    OmnixPackageHeader header;
    std::vector<PackageAssetEntry> assets;
    std::vector<PackageDependencyEntry> dependencies;
    std::vector<PackageChunkEntry> chunks;
    std::vector<uint8_t> rawDataBlock;
};

inline bool SerializePackage(const OmnixPackage& pkg, const std::string& filepath) {
    eng::runtime::BinaryWriter writer;
    writer.BeginFile(MAGIC_PACK, OMNIX_PACKAGE_VERSION_MAJOR, OMNIX_PACKAGE_VERSION_MINOR);

    writer.WriteU32(pkg.header.assetCount);
    writer.WriteU32(pkg.header.dependencyCount);
    writer.WriteU32(pkg.header.chunkCount);

    writer.WriteU64(pkg.header.assetTableOffset);
    writer.WriteU64(pkg.header.dependencyTableOffset);
    writer.WriteU64(pkg.header.chunkTableOffset);
    writer.WriteU64(pkg.header.dataBlockOffset);

    // Asset Table
    if (!pkg.assets.empty()) {
        writer.WriteBytes(reinterpret_cast<const uint8_t*>(pkg.assets.data()), pkg.assets.size() * sizeof(PackageAssetEntry));
    }

    // Dependency Table
    if (!pkg.dependencies.empty()) {
        writer.WriteBytes(reinterpret_cast<const uint8_t*>(pkg.dependencies.data()), pkg.dependencies.size() * sizeof(PackageDependencyEntry));
    }

    // Chunk Table
    if (!pkg.chunks.empty()) {
        writer.WriteBytes(reinterpret_cast<const uint8_t*>(pkg.chunks.data()), pkg.chunks.size() * sizeof(PackageChunkEntry));
    }

    // Raw Data Block
    if (!pkg.rawDataBlock.empty()) {
        writer.WriteBytes(pkg.rawDataBlock.data(), pkg.rawDataBlock.size());
    }

    return writer.SaveToFile(filepath);
}

inline bool DeserializePackage(OmnixPackage& pkg, const std::string& filepath) {
    eng::runtime::BinaryReader reader;
    if (!reader.LoadFromFile(filepath)) {
        return false;
    }

    if (!reader.ValidateHeaderAndChecksum(MAGIC_PACK, OMNIX_PACKAGE_VERSION_MAJOR, OMNIX_PACKAGE_VERSION_MINOR)) {
        return false;
    }

    try {
        reader.Seek(0);
        reader.ReadBytes(reinterpret_cast<uint8_t*>(&pkg.header.file), sizeof(FileHeader));

        pkg.header.assetCount = reader.ReadU32();
        pkg.header.dependencyCount = reader.ReadU32();
        pkg.header.chunkCount = reader.ReadU32();

        pkg.header.assetTableOffset = reader.ReadU64();
        pkg.header.dependencyTableOffset = reader.ReadU64();
        pkg.header.chunkTableOffset = reader.ReadU64();
        pkg.header.dataBlockOffset = reader.ReadU64();

        // Asset Table
        pkg.assets.resize(pkg.header.assetCount);
        if (pkg.header.assetCount > 0) {
            reader.ReadBytes(reinterpret_cast<uint8_t*>(pkg.assets.data()), pkg.header.assetCount * sizeof(PackageAssetEntry));
        }

        // Dependency Table
        pkg.dependencies.resize(pkg.header.dependencyCount);
        if (pkg.header.dependencyCount > 0) {
            reader.ReadBytes(reinterpret_cast<uint8_t*>(pkg.dependencies.data()), pkg.header.dependencyCount * sizeof(PackageDependencyEntry));
        }

        // Chunk Table
        pkg.chunks.resize(pkg.header.chunkCount);
        if (pkg.header.chunkCount > 0) {
            reader.ReadBytes(reinterpret_cast<uint8_t*>(pkg.chunks.data()), pkg.header.chunkCount * sizeof(PackageChunkEntry));
        }

        // Raw Data Block
        // Size of raw data is calculated by: total file size - current offset
        size_t currentOffset = reader.GetOffset();
        size_t totalSize = reader.GetBufferSize();
        if (totalSize > currentOffset) {
            size_t dataSize = totalSize - currentOffset;
            pkg.rawDataBlock.resize(dataSize);
            reader.ReadBytes(pkg.rawDataBlock.data(), dataSize);
        } else {
            pkg.rawDataBlock.clear();
        }

    } catch (const std::exception&) {
        return false;
    }

    return true;
}
