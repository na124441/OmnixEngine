#pragma once
#include "Runtime/Public/OmnixPackageFormat.h"
#include <filesystem>
#include <string>
#include <vector>

namespace eng::runtime {

    class Package
    {
    public:
        Package() = default;
        ~Package() = default;

        // Prevent copying
        Package(const Package&) = delete;
        Package& operator=(const Package&) = delete;

        /**
         * @brief Opens and deserializes the package file.
         */
        bool Open(const std::filesystem::path& path);

        /**
         * @brief Validates package headers, sizes, checksums, offsets, and payloads.
         * @param outError Receives a descriptive error string on failure.
         */
        bool Validate(std::string& outError) const;

        /**
         * @brief Finds an asset entry inside the package. Returns nullptr if not found.
         */
        const PackageAssetEntry* FindAsset(AssetHandle handle) const;

        /**
         * @brief Reads the binary payload of an asset by handle.
         * Returns empty vector if not found or corrupted.
         */
        std::vector<uint8_t> ReadAssetPayload(AssetHandle handle) const;

        /**
         * @brief Returns list of dependency handles for a given asset handle.
         */
        std::vector<AssetHandle> GetAssetDependencies(AssetHandle handle) const;

        const OmnixPackage& GetInternalPackage() const { return m_Pkg; }
        const std::filesystem::path& GetPath() const { return m_Path; }

    private:
        std::filesystem::path m_Path;
        OmnixPackage m_Pkg;
        bool m_IsOpen = false;
    };

} // namespace eng::runtime
