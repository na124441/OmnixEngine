#pragma once
#include "Runtime/AssetHandle.h"
#include "Runtime/AssetMetadata.h"
#include <unordered_map>
#include <string>

namespace eng::runtime {

    AssetHandle GenerateAssetUUID(const std::string& path, AssetType type) noexcept;

    class AssetRegistry
    {
    public:
        AssetRegistry() = default;
        ~AssetRegistry() = default;

        /**
         * @brief Registers an asset with the given path and type.
         * Generates a deterministic handle from the path and type.
         */
        AssetHandle RegisterAsset(const std::string& path, AssetType type);

        /**
         * @brief Looks up metadata by handle. Returns nullptr if not found.
         */
        const AssetMetadata* GetMetadata(AssetHandle handle) const;

        /**
         * @brief Checks if the registry contains the given handle.
         */
        bool Contains(AssetHandle handle) const;

        /**
         * @brief Directly updates or inserts metadata. Used in serialization/loading.
         */
        void UpdateMetadata(const AssetMetadata& metadata);

        /**
         * @brief Serializes the current registry state to a JSON file.
         */
        bool SaveRegistry(const std::string& filepath);

        /**
         * @brief Deserializes registry state from a JSON file.
         */
        bool LoadRegistry(const std::string& filepath);

        /**
         * @brief Clears the registry in memory.
         */
        void Clear() noexcept;

        /**
         * @brief Scans project asset directories and registers found assets.
         */
        void ScanProjectAssets();

        /**
         * @brief Returns a reference to all assets in memory.
         */
        const std::unordered_map<AssetHandle, AssetMetadata>& GetAssets() const noexcept { return m_Assets; }

    private:
        std::unordered_map<AssetHandle, AssetMetadata> m_Assets;
    };

} // namespace eng::runtime
