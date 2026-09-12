#include "Runtime/Public/AssetManager.h"
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
        if (handle.value == BUILTIN_CUBE_HANDLE || (meta && meta->sourcePath == "builtin://cube")) {
            RuntimeMesh* cube = CreateProceduralCube();
            cube->handle = handle;
            m_LoadedAssets[handle] = std::unique_ptr<RuntimeAsset>(cube);
            m_LoadStates[handle] = AssetLoadState::Loaded;
            m_Stats.totalLoadSuccesses++;
            return cube;
        }
        if (handle.value == BUILTIN_PLANE_HANDLE || (meta && meta->sourcePath == "builtin://plane")) {
            RuntimeMesh* plane = CreateProceduralPlane();
            plane->handle = handle;
            m_LoadedAssets[handle] = std::unique_ptr<RuntimeAsset>(plane);
            m_LoadStates[handle] = AssetLoadState::Loaded;
            m_Stats.totalLoadSuccesses++;
            return plane;
        }

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
            if (meta->type == AssetType::Mesh) {
                LOG_WARN("[Assets] WARNING: Loader failed for mesh '%s' (%llu). Substituting procedural fallback cube.", meta->sourcePath.c_str(), handle.value);
                RuntimeMesh* cube = CreateProceduralCube();
                cube->handle = handle;
                cube->type = AssetType::Mesh;
                cube->debugName = meta->sourcePath + " (Fallback Cube)";
                m_LoadedAssets[handle] = std::unique_ptr<RuntimeAsset>(cube);
                m_LoadStates[handle] = AssetLoadState::Loaded;
                m_Stats.totalLoadSuccesses++;
                return cube;
            } else if (meta->type == AssetType::Texture) {
                LOG_WARN("[Assets] WARNING: Loader failed for texture '%s' (%llu). Substituting procedural fallback texture.", meta->sourcePath.c_str(), handle.value);
                RuntimeTexture* fallbackTex = new RuntimeTexture();
                fallbackTex->handle = handle;
                fallbackTex->type = AssetType::Texture;
                fallbackTex->debugName = meta->sourcePath + " (Fallback Texture)";
                fallbackTex->width = 2;
                fallbackTex->height = 2;
                fallbackTex->channels = 4;
                fallbackTex->mipCount = 1;
                fallbackTex->pixelData = {255, 255, 255, 255,  200, 200, 200, 255,
                                          200, 200, 200, 255,  255, 255, 255, 255};
                m_LoadedAssets[handle] = std::unique_ptr<RuntimeAsset>(fallbackTex);
                m_LoadStates[handle] = AssetLoadState::Loaded;
                m_Stats.totalLoadSuccesses++;
                return fallbackTex;
            } else if (meta->type == AssetType::Material) {
                LOG_WARN("[Assets] WARNING: Loader failed for material '%s' (%llu). Substituting procedural fallback material.", meta->sourcePath.c_str(), handle.value);
                RuntimeMaterial* fallbackMat = new RuntimeMaterial();
                fallbackMat->handle = handle;
                fallbackMat->type = AssetType::Material;
                fallbackMat->debugName = meta->sourcePath + " (Fallback Material)";
                fallbackMat->materialName = "FallbackMaterial";
                m_LoadedAssets[handle] = std::unique_ptr<RuntimeAsset>(fallbackMat);
                m_LoadStates[handle] = AssetLoadState::Loaded;
                m_Stats.totalLoadSuccesses++;
                return fallbackMat;
            }

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

    void AssetManager::InitProceduralFallbacks()
    {
        // Register builtin://cube and builtin://plane in registry if not already present
        if (!m_Registry.GetMetadata(AssetHandle(BUILTIN_CUBE_HANDLE))) {
            AssetMetadata cubeMeta{};
            cubeMeta.handle = AssetHandle(BUILTIN_CUBE_HANDLE);
            cubeMeta.sourcePath = "builtin://cube";
            cubeMeta.type = AssetType::Mesh;
            cubeMeta.isImported = true;
            m_Registry.UpdateMetadata(cubeMeta);
        }
        if (!m_Registry.GetMetadata(AssetHandle(BUILTIN_PLANE_HANDLE))) {
            AssetMetadata planeMeta{};
            planeMeta.handle = AssetHandle(BUILTIN_PLANE_HANDLE);
            planeMeta.sourcePath = "builtin://plane";
            planeMeta.type = AssetType::Mesh;
            planeMeta.isImported = true;
            m_Registry.UpdateMetadata(planeMeta);
        }

        // Pre-create and cache them
        if (!GetAsset(AssetHandle(BUILTIN_CUBE_HANDLE))) {
            auto cube = std::unique_ptr<RuntimeAsset>(CreateProceduralCube());
            cube->handle = AssetHandle(BUILTIN_CUBE_HANDLE);
            m_LoadedAssets[AssetHandle(BUILTIN_CUBE_HANDLE)] = std::move(cube);
            m_LoadStates[AssetHandle(BUILTIN_CUBE_HANDLE)] = AssetLoadState::Loaded;
        }
        if (!GetAsset(AssetHandle(BUILTIN_PLANE_HANDLE))) {
            auto plane = std::unique_ptr<RuntimeAsset>(CreateProceduralPlane());
            plane->handle = AssetHandle(BUILTIN_PLANE_HANDLE);
            m_LoadedAssets[AssetHandle(BUILTIN_PLANE_HANDLE)] = std::move(plane);
            m_LoadStates[AssetHandle(BUILTIN_PLANE_HANDLE)] = AssetLoadState::Loaded;
        }
    }

    RuntimeMesh* AssetManager::CreateProceduralCube()
    {
        RuntimeMesh* mesh = new RuntimeMesh();
        mesh->type = AssetType::Mesh;
        mesh->debugName = "builtin://cube";
        mesh->vertexStride = sizeof(OmnixVertex);
        mesh->hasSkeleton = false;
        mesh->submeshCount = 1;

        mesh->bounds.min = {-0.5f, -0.5f, -0.5f};
        mesh->bounds.max = { 0.5f,  0.5f,  0.5f};
        mesh->sphere.center = {0.0f, 0.0f, 0.0f};
        mesh->sphere.radius = 0.866025f;

        mesh->vertices.resize(24);

        auto setFace = [&](int faceIdx, Vec3 normal, Vec4 tangent,
                           Vec3 p0, Vec3 p1, Vec2 u0, Vec2 u1,
                           Vec3 p2, Vec3 p3, Vec2 u2, Vec2 u3) {
            int base = faceIdx * 4;
            mesh->vertices[base + 0] = {p0, normal, tangent, u0, u0};
            mesh->vertices[base + 1] = {p1, normal, tangent, u1, u1};
            mesh->vertices[base + 2] = {p2, normal, tangent, u2, u2};
            mesh->vertices[base + 3] = {p3, normal, tangent, u3, u3};
        };

        // Front (+Z)
        setFace(0, {0.f, 0.f, 1.f}, {1.f, 0.f, 0.f, 1.f},
                {-0.5f, -0.5f,  0.5f}, { 0.5f, -0.5f,  0.5f}, {0.f, 0.f}, {1.f, 0.f},
                { 0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f}, {1.f, 1.f}, {0.f, 1.f});
        // Back (-Z)
        setFace(1, {0.f, 0.f, -1.f}, {-1.f, 0.f, 0.f, 1.f},
                { 0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, -0.5f}, {0.f, 0.f}, {1.f, 0.f},
                {-0.5f,  0.5f, -0.5f}, { 0.5f,  0.5f, -0.5f}, {1.f, 1.f}, {0.f, 1.f});
        // Left (-X)
        setFace(2, {-1.f, 0.f, 0.f}, {0.f, 0.f, 1.f, 1.f},
                {-0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f,  0.5f}, {0.f, 0.f}, {1.f, 0.f},
                {-0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f, -0.5f}, {1.f, 1.f}, {0.f, 1.f});
        // Right (+X)
        setFace(3, {1.f, 0.f, 0.f}, {0.f, 0.f, -1.f, 1.f},
                { 0.5f, -0.5f,  0.5f}, { 0.5f, -0.5f, -0.5f}, {0.f, 0.f}, {1.f, 0.f},
                { 0.5f,  0.5f, -0.5f}, { 0.5f,  0.5f,  0.5f}, {1.f, 1.f}, {0.f, 1.f});
        // Top (+Y)
        setFace(4, {0.f, 1.f, 0.f}, {1.f, 0.f, 0.f, 1.f},
                {-0.5f,  0.5f,  0.5f}, { 0.5f,  0.5f,  0.5f}, {0.f, 0.f}, {1.f, 0.f},
                { 0.5f,  0.5f, -0.5f}, {-0.5f,  0.5f, -0.5f}, {1.f, 1.f}, {0.f, 1.f});
        // Bottom (-Y)
        setFace(5, {0.f, -1.f, 0.f}, {1.f, 0.f, 0.f, 1.f},
                {-0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f, -0.5f}, {0.f, 0.f}, {1.f, 0.f},
                { 0.5f, -0.5f,  0.5f}, {-0.5f, -0.5f,  0.5f}, {1.f, 1.f}, {0.f, 1.f});

        mesh->vertexCount = static_cast<uint32_t>(mesh->vertices.size());

        mesh->indices.reserve(36);
        for (int f = 0; f < 6; ++f) {
            uint32_t base = static_cast<uint32_t>(f * 4);
            mesh->indices.push_back(base + 0);
            mesh->indices.push_back(base + 1);
            mesh->indices.push_back(base + 2);
            mesh->indices.push_back(base + 0);
            mesh->indices.push_back(base + 2);
            mesh->indices.push_back(base + 3);
        }
        mesh->indexCount = static_cast<uint32_t>(mesh->indices.size());

        OmnixSubmesh submesh{};
        submesh.indexStart = 0;
        submesh.indexCount = mesh->indexCount;
        submesh.materialIndex = 0;
        mesh->submeshes.push_back(submesh);

        return mesh;
    }

    RuntimeMesh* AssetManager::CreateProceduralPlane()
    {
        RuntimeMesh* mesh = new RuntimeMesh();
        mesh->type = AssetType::Mesh;
        mesh->debugName = "builtin://plane";
        mesh->vertexStride = sizeof(OmnixVertex);
        mesh->hasSkeleton = false;
        mesh->submeshCount = 1;

        mesh->bounds.min = {-0.5f, 0.0f, -0.5f};
        mesh->bounds.max = { 0.5f, 0.0f,  0.5f};
        mesh->sphere.center = {0.0f, 0.0f, 0.0f};
        mesh->sphere.radius = 0.7071f;

        mesh->vertices.resize(4);
        Vec3 norm{0.f, 1.f, 0.f};
        Vec4 tang{1.f, 0.f, 0.f, 1.f};
        mesh->vertices[0] = {{-0.5f, 0.0f, -0.5f}, norm, tang, {0.f, 0.f}, {0.f, 0.f}};
        mesh->vertices[1] = {{ 0.5f, 0.0f, -0.5f}, norm, tang, {1.f, 0.f}, {1.f, 0.f}};
        mesh->vertices[2] = {{ 0.5f, 0.0f,  0.5f}, norm, tang, {1.f, 1.f}, {1.f, 1.f}};
        mesh->vertices[3] = {{-0.5f, 0.0f,  0.5f}, norm, tang, {0.f, 1.f}, {0.f, 1.f}};

        mesh->vertexCount = 4;

        mesh->indices = {0, 1, 2, 0, 2, 3};
        mesh->indexCount = 6;

        OmnixSubmesh submesh{};
        submesh.indexStart = 0;
        submesh.indexCount = 6;
        submesh.materialIndex = 0;
        mesh->submeshes.push_back(submesh);

        return mesh;
    }

    RuntimeAsset* AssetManager::GetProceduralFallback(AssetType type)
    {
        if (type == AssetType::Mesh) {
            return GetOrLoad(AssetHandle(BUILTIN_CUBE_HANDLE));
        }
        return nullptr;
    }

} // namespace eng::runtime

