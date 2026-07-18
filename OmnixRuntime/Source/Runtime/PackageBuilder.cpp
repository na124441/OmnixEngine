#include "Runtime/PackageBuilder.h"
#include "Runtime/OmnixPackageFormat.h"
#include "Core/Logging/Logger.h"
#include <algorithm>

namespace eng::runtime {

    namespace {
        uint32_t ComputeFNV1a32(const uint8_t* data, size_t size) noexcept
        {
            uint32_t hash = 2166136261U;
            for (size_t i = 0; i < size; ++i) {
                hash ^= data[i];
                hash *= 16777619U;
            }
            return hash;
        }
    }

    void PackageBuilder::AddAsset(AssetHandle handle, AssetType type, const std::vector<uint8_t>& data, const std::vector<AssetHandle>& dependencies)
    {
        PendingAsset asset;
        asset.handle = handle;
        asset.type = type;
        asset.data = data;
        asset.dependencies = dependencies;
        m_Assets.push_back(std::move(asset));
    }

    bool PackageBuilder::Build(const std::string& filepath)
    {
        OmnixPackage pkg;
        pkg.header.assetCount = static_cast<uint32_t>(m_Assets.size());
        pkg.header.chunkCount = static_cast<uint32_t>(m_Assets.size());

        uint32_t depCount = 0;
        for (const auto& asset : m_Assets) {
            depCount += static_cast<uint32_t>(asset.dependencies.size());
        }
        pkg.header.dependencyCount = depCount;

        // Offset calculations:
        // Header starts at 0. The size of OmnixPackageHeader is 76 bytes.
        pkg.header.assetTableOffset = 76;
        pkg.header.dependencyTableOffset = pkg.header.assetTableOffset + pkg.header.assetCount * sizeof(PackageAssetEntry);
        pkg.header.chunkTableOffset = pkg.header.dependencyTableOffset + pkg.header.dependencyCount * sizeof(PackageDependencyEntry);
        pkg.header.dataBlockOffset = pkg.header.chunkTableOffset + pkg.header.chunkCount * sizeof(PackageChunkEntry);

        uint64_t currentOffsetInBlock = 0;

        for (uint32_t i = 0; i < m_Assets.size(); ++i) {
            const auto& pending = m_Assets[i];

            // 1. Asset Entry
            PackageAssetEntry assetEntry;
            assetEntry.handle = pending.handle;
            assetEntry.type = static_cast<uint32_t>(pending.type);
            assetEntry.dataOffset = pkg.header.dataBlockOffset + currentOffsetInBlock;
            assetEntry.dataSize = pending.data.size();
            assetEntry.compressionType = 0; // None
            assetEntry.checksum = ComputeFNV1a32(pending.data.data(), pending.data.size());
            pkg.assets.push_back(assetEntry);

            // 2. Dependency Entries
            for (AssetHandle dep : pending.dependencies) {
                PackageDependencyEntry depEntry;
                depEntry.assetHandle = pending.handle;
                depEntry.dependentHandle = dep;
                pkg.dependencies.push_back(depEntry);
            }

            // 3. Chunk Entry (1 chunk per asset)
            PackageChunkEntry chunkEntry;
            chunkEntry.chunkID = i;
            chunkEntry.offset = pkg.header.dataBlockOffset + currentOffsetInBlock;
            chunkEntry.size = pending.data.size();
            chunkEntry.compressionType = 0;
            pkg.chunks.push_back(chunkEntry);

            // 4. Concat to Raw Data Block
            if (!pending.data.empty()) {
                pkg.rawDataBlock.insert(pkg.rawDataBlock.end(), pending.data.begin(), pending.data.end());
            }
            currentOffsetInBlock += pending.data.size();
        }

        bool success = SerializePackage(pkg, filepath);
        if (!success) {
            LOG_ERROR("[PackageBuilder] Failed to serialize package to %s", filepath.c_str());
            return false;
        }

        LOG_INFO("[PackageBuilder] Successfully built package %s with %u assets.", filepath.c_str(), pkg.header.assetCount);
        return true;
    }

} // namespace eng::runtime
