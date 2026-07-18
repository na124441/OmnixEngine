#pragma once
#include "Runtime/AssetHandle.h"
#include "Runtime/AssetType.h"
#include <vector>
#include <string>

namespace eng::runtime {

    class PackageBuilder
    {
    public:
        PackageBuilder() = default;
        ~PackageBuilder() = default;

        /**
         * @brief Adds an asset to be packaged.
         * @param handle The AssetHandle representing the asset.
         * @param type The type of the asset.
         * @param data The raw binary payload of the asset.
         * @param dependencies The list of dependency handles for this asset.
         */
        void AddAsset(AssetHandle handle, AssetType type, const std::vector<uint8_t>& data, const std::vector<AssetHandle>& dependencies);

        /**
         * @brief Builds and serializes the packaged asset archive to disk.
         * @param filepath The output path for the .omnixpackage file.
         * @return true if successful, false otherwise.
         */
        bool Build(const std::string& filepath);

    private:
        struct PendingAsset
        {
            AssetHandle handle;
            AssetType type;
            std::vector<uint8_t> data;
            std::vector<AssetHandle> dependencies;
        };

        std::vector<PendingAsset> m_Assets;
    };

} // namespace eng::runtime
