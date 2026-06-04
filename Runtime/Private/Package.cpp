#include "Runtime/Public/Package.h"
#include "Core/Logging/Logger.h"
#include <cstring>
#include <filesystem>

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

    bool Package::Open(const std::filesystem::path& path)
    {
        m_Path = path;
        if (!std::filesystem::exists(path)) {
            LOG_ERROR("[Package] File does not exist: %s", path.string().c_str());
            m_IsOpen = false;
            return false;
        }

        if (!DeserializePackage(m_Pkg, path.string())) {
            LOG_ERROR("[Package] Failed to deserialize package file: %s", path.string().c_str());
            m_IsOpen = false;
            return false;
        }

        std::string err;
        if (!Validate(err)) {
            LOG_ERROR("[Package] Validation failed for %s: %s", path.string().c_str(), err.c_str());
            m_IsOpen = false;
            return false;
        }

        m_IsOpen = true;
        return true;
    }

    bool Package::Validate(std::string& outError) const
    {
        // 1. Validate magic bytes & versions
        if (std::memcmp(m_Pkg.header.file.magic, MAGIC_PACK, 8) != 0) {
            outError = "Invalid magic bytes in package header";
            return false;
        }

        if (m_Pkg.header.file.versionMajor != OMNIX_PACKAGE_VERSION_MAJOR) {
            outError = "Unsupported package major version: " + std::to_string(m_Pkg.header.file.versionMajor);
            return false;
        }

        // 2. Validate file size consistency on disk
        std::error_code sizeEc;
        uint64_t realSize = std::filesystem::file_size(m_Path, sizeEc);
        if (sizeEc) {
            outError = "Failed to query package file size on disk";
            return false;
        }

        if (m_Pkg.header.file.fileSize != realSize) {
            outError = "Package file size mismatch: header shows " + std::to_string(m_Pkg.header.file.fileSize) + " but disk size is " + std::to_string(realSize);
            return false;
        }

        // 3. Validate table offsets are within file size bounds
        if (m_Pkg.header.assetTableOffset > realSize) {
            outError = "Asset table offset " + std::to_string(m_Pkg.header.assetTableOffset) + " exceeds file size " + std::to_string(realSize);
            return false;
        }
        if (m_Pkg.header.dependencyTableOffset > realSize) {
            outError = "Dependency table offset " + std::to_string(m_Pkg.header.dependencyTableOffset) + " exceeds file size " + std::to_string(realSize);
            return false;
        }
        if (m_Pkg.header.chunkTableOffset > realSize) {
            outError = "Chunk table offset " + std::to_string(m_Pkg.header.chunkTableOffset) + " exceeds file size " + std::to_string(realSize);
            return false;
        }
        if (m_Pkg.header.dataBlockOffset > realSize) {
            outError = "Data block offset " + std::to_string(m_Pkg.header.dataBlockOffset) + " exceeds file size " + std::to_string(realSize);
            return false;
        }

        // 4. Validate Asset Table Entries
        for (uint32_t i = 0; i < m_Pkg.assets.size(); ++i) {
            const auto& entry = m_Pkg.assets[i];
            if (!entry.handle.IsValid()) {
                outError = "Asset entry " + std::to_string(i) + " has invalid handle (0)";
                return false;
            }

            if (entry.dataOffset < m_Pkg.header.dataBlockOffset || entry.dataOffset + entry.dataSize > realSize) {
                outError = "Asset entry " + std::to_string(i) + " points outside package boundaries (Offset: " +
                           std::to_string(entry.dataOffset) + ", Size: " + std::to_string(entry.dataSize) + ")";
                return false;
            }

            uint64_t offsetInBlock = entry.dataOffset - m_Pkg.header.dataBlockOffset;
            if (offsetInBlock + entry.dataSize > m_Pkg.rawDataBlock.size()) {
                outError = "Asset entry " + std::to_string(i) + " payload goes outside raw data block boundaries";
                return false;
            }

            // Check payload checksum
            uint32_t computed = ComputeFNV1a32(m_Pkg.rawDataBlock.data() + offsetInBlock, entry.dataSize);
            if (computed != entry.checksum) {
                outError = "Asset entry " + std::to_string(i) + " payload checksum mismatch! Expected: " +
                           std::to_string(entry.checksum) + ", Computed: " + std::to_string(computed);
                return false;
            }
        }

        // 5. Validate Dependency Table Entries
        for (uint32_t i = 0; i < m_Pkg.dependencies.size(); ++i) {
            const auto& entry = m_Pkg.dependencies[i];
            if (!entry.assetHandle.IsValid()) {
                outError = "Dependency entry " + std::to_string(i) + " has invalid parent handle (0)";
                return false;
            }
            if (!entry.dependentHandle.IsValid()) {
                outError = "Dependency entry " + std::to_string(i) + " has invalid dependency handle (0)";
                return false;
            }
        }

        // 6. Validate Chunk Table Entries
        for (uint32_t i = 0; i < m_Pkg.chunks.size(); ++i) {
            const auto& entry = m_Pkg.chunks[i];
            if (entry.offset < m_Pkg.header.dataBlockOffset || entry.offset + entry.size > realSize) {
                outError = "Chunk entry " + std::to_string(i) + " offset " + std::to_string(entry.offset) + " or size " +
                           std::to_string(entry.size) + " out of file size boundaries " + std::to_string(realSize);
                return false;
            }
        }

        return true;
    }

    const PackageAssetEntry* Package::FindAsset(AssetHandle handle) const
    {
        if (!m_IsOpen) return nullptr;

        for (const auto& entry : m_Pkg.assets) {
            if (entry.handle == handle) {
                return &entry;
            }
        }
        return nullptr;
    }

    std::vector<uint8_t> Package::ReadAssetPayload(AssetHandle handle) const
    {
        if (!m_IsOpen) return {};

        const auto* entry = FindAsset(handle);
        if (!entry) return {};

        uint64_t offsetInBlock = entry->dataOffset - m_Pkg.header.dataBlockOffset;
        if (offsetInBlock + entry->dataSize > m_Pkg.rawDataBlock.size()) {
            return {};
        }

        std::vector<uint8_t> result(entry->dataSize);
        std::memcpy(result.data(), m_Pkg.rawDataBlock.data() + offsetInBlock, entry->dataSize);
        return result;
    }

    std::vector<AssetHandle> Package::GetAssetDependencies(AssetHandle handle) const
    {
        if (!m_IsOpen) return {};

        std::vector<AssetHandle> result;
        for (const auto& dep : m_Pkg.dependencies) {
            if (dep.assetHandle == handle) {
                result.push_back(dep.dependentHandle);
            }
        }
        return result;
    }

} // namespace eng::runtime
