#pragma once
#include "Runtime/AssetHandle.h"
#include "Runtime/AssetRegistry.h"
#include "Runtime/IAssetLoader.h"
#include "Runtime/AssetLoadState.h"
#include "Runtime/AssetDiagnostics.h"
#include <unordered_map>
#include <memory>
#include <string>

namespace eng::runtime {

    class AssetManager
    {
    public:
        AssetManager(AssetRegistry& registry);
        ~AssetManager();

        // Prevent copying
        AssetManager(const AssetManager&) = delete;
        AssetManager& operator=(const AssetManager&) = delete;

        /**
         * @brief Registers an asset loader for a specific asset type.
         * Takes ownership of the loader.
         */
        void RegisterLoader(AssetType type, std::unique_ptr<IAssetLoader> loader);

        /**
         * @brief Unregisters the loader for a specific type.
         */
        void UnregisterLoader(AssetType type);

        /**
         * @brief Checks the cache and returns a pointer to the loaded asset, or nullptr if not loaded.
         */
        RuntimeAsset* GetAsset(AssetHandle handle) const;

        /**
         * @brief Resolves and loads the asset by handle (along with all dependencies).
         * Respects caching (if already loaded, returns the cached pointer).
         */
        RuntimeAsset* LoadAsset(AssetHandle handle);

        /**
         * @brief Lazy load support. Resolves cached asset if loaded, otherwise loads it on demand.
         */
        RuntimeAsset* GetOrLoad(AssetHandle handle);

        /**
         * @brief Template helper to retrieve an asset cast to its specialized runtime type.
         */
        template<typename T>
        T* Get(AssetHandle handle) const {
            return dynamic_cast<T*>(GetAsset(handle));
        }

        /**
         * @brief Template helper to lazy load and cast to its specialized runtime type.
         */
        template<typename T>
        T* GetOrLoad(AssetHandle handle) {
            return dynamic_cast<T*>(GetOrLoad(handle));
        }

        /**
         * @brief Unloads the asset by handle and removes it from the cache.
         */
        void UnloadAsset(AssetHandle handle);

        /**
         * @brief Clears the entire loaded asset cache, calling Unload on respective loaders.
         */
        void ClearCache();

        /**
         * @brief Reloads an already cached asset in-place from its native cache file.
         * Keeps the old asset active if reloading fails.
         */
        bool ReloadCachedAsset(AssetHandle handle);

        /**
         * @brief Returns the load state of an asset handle.
         */
        AssetLoadState GetLoadState(AssetHandle handle) const;

        /**
         * @brief Returns diagnostics for a specific handle, or nullptr if none exists.
         */
        const AssetDiagnosticInfo* GetDiagnosticInfo(AssetHandle handle) const;

        /**
         * @brief Returns the global asset manager statistics.
         */
        const AssetManagerStats& GetStats() const { return m_Stats; }

    private:
        bool LoadDependencies(const AssetMetadata& meta);
        IAssetLoader* FindLoader(AssetType type) const;

        AssetRegistry& m_Registry;
        std::unordered_map<AssetType, std::unique_ptr<IAssetLoader>> m_Loaders;
        std::unordered_map<AssetHandle, std::unique_ptr<RuntimeAsset>> m_LoadedAssets;
        std::unordered_map<AssetHandle, AssetLoadState> m_LoadStates;
        std::unordered_map<AssetHandle, AssetDiagnosticInfo> m_Diagnostics;
        AssetManagerStats m_Stats;
    };

} // namespace eng::runtime
