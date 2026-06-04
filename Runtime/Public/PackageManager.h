#pragma once
#include "Runtime/Public/Package.h"
#include <vector>
#include <memory>
#include <filesystem>

namespace eng::runtime {

    class PackageManager
    {
    public:
        PackageManager() = default;
        ~PackageManager() = default;

        // Prevent copying
        PackageManager(const PackageManager&) = delete;
        PackageManager& operator=(const PackageManager&) = delete;

        /**
         * @brief Mounts a package at the given filepath.
         * Deserializes and validates it before inserting it into the stack.
         */
        bool MountPackage(const std::filesystem::path& path);

        /**
         * @brief Unmounts a mounted package by path.
         */
        bool UnmountPackage(const std::filesystem::path& path);

        /**
         * @brief Queries all mounted packages for an AssetHandle.
         * Employs "latest mounted wins" lookup priority (iterating stack in reverse).
         */
        const PackageAssetEntry* FindAsset(AssetHandle handle) const;

        /**
         * @brief Reads the binary payload of an asset by handle.
         * Returns empty vector if the asset is not contained in any mounted package.
         */
        std::vector<uint8_t> ReadAssetPayload(AssetHandle handle) const;

        /**
         * @brief Checks if the asset handle is contained in any mounted package.
         */
        bool Contains(AssetHandle handle) const;

        /**
         * @brief Returns a list of dependencies for a given asset handle.
         */
        std::vector<AssetHandle> GetDependencies(AssetHandle handle) const;

        /**
         * @brief Unmounts all packages and clears the stack.
         */
        void Clear();

        /**
         * @brief Returns list of mounted packages.
         */
        const std::vector<std::unique_ptr<Package>>& GetMountedPackages() const { return m_MountedPackages; }

    private:
        std::vector<std::unique_ptr<Package>> m_MountedPackages;
    };

    /**
     * @brief Singleton accessor for the global PackageManager.
     */
    PackageManager& GetPackageManager();

} // namespace eng::runtime
