#include "Runtime/AssetManager.h"
#include "Core/Logging/Logger.h"
#include <chrono>

namespace eng::runtime {

    AssetManager::AssetManager(AssetRegistry& registry)
        : m_Registry(registry)
    {
    }

    AssetManager::~AssetManager()
    {
        ClearCache();
    }

    void AssetManager::RegisterLoader(AssetType type, std::unique_ptr<IAssetLoader> loader)
    {
        if (!loader) return;
        m_Loaders[type] = std::move(loader);
    }

    void AssetManager::UnregisterLoader(AssetType type)
    {
        m_Loaders.erase(type);
    }

    RuntimeAsset* AssetManager::GetAsset(AssetHandle handle) const
    {
        auto it = m_LoadedAssets.find(handle);
        if (it != m_LoadedAssets.end()) {
            return it->second.get();
        }
        return nullptr;
    }

    RuntimeAsset* AssetManager::LoadAsset(AssetHandle handle)
    {
        m_Stats.totalLoadsAttempted++;

        if (!handle.IsValid()) {
            m_Stats.totalLoadFailures++;
            LOG_ERROR("[Assets] ERROR: Load failed. Handle is invalid!");
            return nullptr;
        }

        // 1. Check cache hit
        if (RuntimeAsset* cached = GetAsset(handle)) {
            m_Stats.totalCacheHits++;
            m_Diagnostics[handle].cacheHits++;
            return cached;
        }

        m_Stats.totalCacheMisses++;

        // Initialize or update diagnostics for this hit/miss cycle
        AssetDiagnosticInfo& diag = m_Diagnostics[handle];
        diag.handle = handle;
        diag.cacheMisses++;

        // 2. Circular dependency check
        AssetLoadState state = GetLoadState(handle);
        if (state == AssetLoadState::Loading) {
            m_Stats.totalLoadFailures++;
            diag.lastError = "Circular dependency detected";
            LOG_WARN("[Assets] WARNING: Circular dependency or re-entrant load detected for handle %llu", handle.value);
            return nullptr;
        }

        // 3. Resolve metadata
        const AssetMetadata* meta = m_Registry.GetMetadata(handle);
        if (!meta) {
            m_Stats.totalLoadFailures++;
            m_LoadStates[handle] = AssetLoadState::Failed;
            diag.lastError = "Missing metadata in registry";
            diag.type = AssetType::Unknown;
            LOG_ERROR("[Assets] ERROR: Load failed for handle %llu. Reason: Missing metadata", handle.value);
            return nullptr;
        }

        diag.type = meta->type;
        diag.sourcePath = meta->sourcePath;
        diag.importedPath = meta->importedPath;

        // 4. Find loader
        IAssetLoader* loader = FindLoader(meta->type);
        if (!loader) {
            m_Stats.totalLoadFailures++;
            m_LoadStates[handle] = AssetLoadState::Failed;
            diag.lastError = "No loader registered for type";
            LOG_ERROR("[Assets] ERROR: Load failed.\nHandle: %llu\nType: %s\nSource: %s\nRuntime File: %s\nReason: No loader registered",
                      handle.value, AssetTypeToString(meta->type), meta->sourcePath.c_str(), meta->importedPath.c_str());
            return nullptr;
        }

        // 5. Track loader starting sequence
        m_LoadStates[handle] = AssetLoadState::Loading;

        // Load dependencies first
        if (!LoadDependencies(*meta)) {
            m_Stats.totalLoadFailures++;
            m_LoadStates[handle] = AssetLoadState::Failed;
            diag.lastError = "Dependency load failure";
            LOG_ERROR("[Assets] ERROR: Load failed.\nHandle: %llu\nType: %s\nSource: %s\nRuntime File: %s\nReason: Dependency load failure",
                      handle.value, AssetTypeToString(meta->type), meta->sourcePath.c_str(), meta->importedPath.c_str());
            return nullptr;
        }

        // 6. Execute load
        auto startTime = std::chrono::high_resolution_clock::now();
        RuntimeAsset* asset = nullptr;
        bool success = loader->Load(*meta, &asset);
        auto endTime = std::chrono::high_resolution_clock::now();
        uint32_t durationMs = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count());
        diag.loadTimeMs = durationMs;

        if (!success || !asset) {
            m_Stats.totalLoadFailures++;
            m_LoadStates[handle] = AssetLoadState::Failed;
            diag.lastError = "Loader failed to load file";
            LOG_ERROR("[Assets] ERROR: Load failed.\nHandle: %llu\nType: %s\nSource: %s\nRuntime File: %s\nReason: Loader failed to parse or read file",
                      handle.value, AssetTypeToString(meta->type), meta->sourcePath.c_str(), meta->importedPath.c_str());
            return nullptr;
        }

        // Establish runtime asset fields
        asset->handle = handle;
        asset->type = meta->type;
        asset->debugName = meta->sourcePath;

        // Cache the loaded asset
        m_LoadedAssets[handle] = std::unique_ptr<RuntimeAsset>(asset);
        m_LoadStates[handle] = AssetLoadState::Loaded;
        m_Stats.totalLoadSuccesses++;

        LOG_INFO("[Assets] Success: Loaded asset %llu (%s) in %u ms", handle.value, meta->sourcePath.c_str(), durationMs);
        return asset;
    }

    RuntimeAsset* AssetManager::GetOrLoad(AssetHandle handle)
    {
        if (RuntimeAsset* asset = GetAsset(handle)) {
            m_Stats.totalCacheHits++;
            m_Diagnostics[handle].cacheHits++;
            return asset;
        }
        return LoadAsset(handle);
    }

    void AssetManager::UnloadAsset(AssetHandle handle)
    {
        auto it = m_LoadedAssets.find(handle);
        if (it != m_LoadedAssets.end()) {
            RuntimeAsset* asset = it->second.release();
            IAssetLoader* loader = FindLoader(asset->type);
            if (loader) {
                loader->Unload(asset);
            } else {
                delete asset;
            }
            m_LoadedAssets.erase(it);
            m_LoadStates[handle] = AssetLoadState::Unloaded;
            LOG_INFO("[Assets] Unloaded asset %llu", handle.value);
        }
    }

    void AssetManager::ClearCache()
    {
        for (auto& pair : m_LoadedAssets) {
            RuntimeAsset* asset = pair.second.release();
            if (asset) {
                IAssetLoader* loader = FindLoader(asset->type);
                if (loader) {
                    loader->Unload(asset);
                } else {
                    delete asset;
                }
            }
        }
        m_LoadedAssets.clear();
        m_LoadStates.clear();
        m_Diagnostics.clear();
        LOG_INFO("[Assets] Asset cache cleared.");
    }

    AssetLoadState AssetManager::GetLoadState(AssetHandle handle) const
    {
        auto it = m_LoadStates.find(handle);
        if (it != m_LoadStates.end()) {
            return it->second;
        }
        return AssetLoadState::Unloaded;
    }

    const AssetDiagnosticInfo* AssetManager::GetDiagnosticInfo(AssetHandle handle) const
    {
        auto it = m_Diagnostics.find(handle);
        if (it != m_Diagnostics.end()) {
            return &it->second;
        }
        return nullptr;
    }

    bool AssetManager::LoadDependencies(const AssetMetadata& meta)
    {
        for (AssetHandle dependency : meta.dependencies) {
            RuntimeAsset* depAsset = GetOrLoad(dependency);
            if (!depAsset) {
                return false;
            }
            m_Stats.totalDependenciesLoaded++;
        }
        return true;
    }

    IAssetLoader* AssetManager::FindLoader(AssetType type) const
    {
        auto it = m_Loaders.find(type);
        if (it != m_Loaders.end()) {
            return it->second.get();
        }
        return nullptr;
    }

    bool AssetManager::ReloadCachedAsset(AssetHandle handle)
    {
        if (!handle.IsValid()) return false;

        const AssetMetadata* meta = m_Registry.GetMetadata(handle);
        if (!meta) {
            LOG_ERROR("[Assets] Failed to reload asset %llu. Missing metadata.", handle.value);
            return false;
        }

        auto it = m_LoadedAssets.find(handle);
        if (it == m_LoadedAssets.end()) {
            m_LoadStates[handle] = AssetLoadState::Unloaded;
            return true;
        }

        IAssetLoader* loader = FindLoader(meta->type);
        if (!loader) {
            LOG_ERROR("[Assets] Failed to reload asset %llu. No loader found.", handle.value);
            return false;
        }

        RuntimeAsset* newAsset = nullptr;
        bool success = loader->Load(*meta, &newAsset);
        if (!success || !newAsset) {
            LOG_ERROR("[Assets] Hot reload failed for asset %llu. Keeping old asset active.", handle.value);
            return false;
        }

        // Validation bounds checks
        if (meta->type == AssetType::Mesh) {
            RuntimeMesh* mesh = dynamic_cast<RuntimeMesh*>(newAsset);
            if (!mesh || mesh->vertexCount == 0 || mesh->indexCount == 0) {
                LOG_ERROR("[Assets] Mesh validation failed during hot reload swap for asset %llu", handle.value);
                loader->Unload(newAsset);
                return false;
            }
        } else if (meta->type == AssetType::Texture) {
            RuntimeTexture* tex = dynamic_cast<RuntimeTexture*>(newAsset);
            if (!tex || tex->width == 0 || tex->height == 0) {
                LOG_ERROR("[Assets] Texture validation failed during hot reload swap for asset %llu", handle.value);
                loader->Unload(newAsset);
                return false;
            }
        }

        newAsset->handle = handle;
        newAsset->type = meta->type;
        newAsset->debugName = meta->sourcePath;

        RuntimeAsset* oldAsset = it->second.release();
        if (oldAsset) {
            loader->Unload(oldAsset);
        }

        it->second = std::unique_ptr<RuntimeAsset>(newAsset);
        m_LoadStates[handle] = AssetLoadState::Loaded;

        LOG_INFO("[Assets] Hot reload swapped runtime asset %llu in-place.", handle.value);
        return true;
    }
} // namespace eng::runtime
