#include "Runtime/HotReloadTests.h"
#include "Runtime/AssetManager.h"
#include "Runtime/AssetRegistry.h"
#include "Runtime/HotReloadSystem.h"
#include "Core/Logging/Logger.h"
#include <vector>
#include <chrono>

namespace eng::runtime {

    namespace {
        class StressMockLoader : public IAssetLoader
        {
        public:
            StressMockLoader(AssetType type) : m_Type(type) {}

            AssetType GetSupportedType() const override { return m_Type; }
            bool CanLoad(AssetType type) const override { return type == m_Type; }

            bool Load(const AssetMetadata& metadata, RuntimeAsset** outAsset) override
            {
                if (m_Type == AssetType::Texture) {
                    auto* t = new RuntimeTexture();
                    t->width = 1;
                    t->height = 1;
                    *outAsset = t;
                } else if (m_Type == AssetType::Mesh) {
                    auto* m = new RuntimeMesh();
                    m->vertexCount = 1;
                    m->indexCount = 1;
                    *outAsset = m;
                } else {
                    *outAsset = new RuntimeAsset();
                }
                m_LoadCount++;
                m_ActiveCount++;
                return true;
            }

            void Unload(RuntimeAsset* asset) override
            {
                delete asset;
                m_UnloadCount++;
                m_ActiveCount--;
            }

            static uint32_t GetActiveAllocations() { return m_ActiveCount; }
            static void Reset()
            {
                m_LoadCount = 0;
                m_UnloadCount = 0;
                m_ActiveCount = 0;
            }

        private:
            AssetType m_Type;
            static uint32_t m_LoadCount;
            static uint32_t m_UnloadCount;
            static uint32_t m_ActiveCount;
        };

        uint32_t StressMockLoader::m_LoadCount = 0;
        uint32_t StressMockLoader::m_UnloadCount = 0;
        uint32_t StressMockLoader::m_ActiveCount = 0;
    }

    bool RunHotReloadStressTests() noexcept
    {
        LOG_INFO("[HotReloadStressTest] Running hot reload stress tests...");
        StressMockLoader::Reset();

        AssetRegistry registry;
        AssetManager manager(registry);

        manager.RegisterLoader(AssetType::Texture, std::make_unique<StressMockLoader>(AssetType::Texture));
        manager.RegisterLoader(AssetType::Mesh, std::make_unique<StressMockLoader>(AssetType::Mesh));
        manager.RegisterLoader(AssetType::Material, std::make_unique<StressMockLoader>(AssetType::Material));
        manager.RegisterLoader(AssetType::Scene, std::make_unique<StressMockLoader>(AssetType::Scene));

        // Create 10 assets with deep dependencies:
        // Texture 1 (100) -> Material 1 (200) -> Scene 1 (300)
        // Texture 2 (101) -> Material 2 (201) -> Scene 1 (300)
        // ...

        // Register textures 100-103
        std::vector<AssetHandle> textures;
        for (int i = 0; i < 4; ++i) {
            AssetHandle handle{static_cast<uint64_t>(100 + i)};
            AssetMetadata meta;
            meta.handle = handle;
            meta.type = AssetType::Texture;
            meta.sourcePath = "Assets/Textures/stone_" + std::to_string(i) + ".png";
            meta.importedPath = "Cache/Textures/stone_" + std::to_string(i) + ".omnixtex";
            meta.isImported = true;
            registry.UpdateMetadata(meta);
            textures.push_back(handle);
        }

        // Register materials 200-203
        std::vector<AssetHandle> materials;
        for (int i = 0; i < 4; ++i) {
            AssetHandle handle{static_cast<uint64_t>(200 + i)};
            AssetMetadata meta;
            meta.handle = handle;
            meta.type = AssetType::Material;
            meta.sourcePath = "Assets/Materials/stone_" + std::to_string(i) + ".mat";
            meta.importedPath = "Cache/Materials/stone_" + std::to_string(i) + ".omnixmat";
            meta.isImported = true;
            meta.dependencies = { textures[i], textures[(i + 1) % textures.size()] };
            registry.UpdateMetadata(meta);
            materials.push_back(handle);
        }

        // Register scenes 300-301
        std::vector<AssetHandle> scenes;
        for (int i = 0; i < 2; ++i) {
            AssetHandle handle{static_cast<uint64_t>(300 + i)};
            AssetMetadata meta;
            meta.handle = handle;
            meta.type = AssetType::Scene;
            meta.sourcePath = "Assets/Scenes/level_" + std::to_string(i) + ".scene";
            meta.importedPath = "Cache/Scenes/level_" + std::to_string(i) + ".omnixscene";
            meta.isImported = true;
            meta.dependencies = { materials[i * 2], materials[i * 2 + 1] };
            registry.UpdateMetadata(meta);
            scenes.push_back(handle);
        }

        // Load all scenes initially (forcing full dependency load)
        for (auto h : scenes) {
            manager.LoadAsset(h);
        }

        uint32_t initialAllocations = StressMockLoader::GetActiveAllocations();
        if (initialAllocations == 0) {
            LOG_ERROR("[HotReloadStressTest] Initial assets were not loaded!");
            return false;
        }

        HotReloadSystem hotReload(registry, manager);

        // Test 1: Verify Material dependency refresh
        {
            hotReload.ClearHistory();
            // Trigger reload of Texture 0 (handle 100)
            // Topological order should reload:
            // 1. Texture 100
            // 2. Material 200 (depends on 100)
            // 3. Scene 300 (depends on 200)
            // 4. Material 203 (depends on 100 because of cycle dependencies (i + 1) % size)
            // 5. Scene 301 (depends on 203)
            bool success = hotReload.ReloadAsset(textures[0]);
            if (!success) {
                LOG_ERROR("[HotReloadStressTest] Dependency reload failed!");
                return false;
            }

            const auto& history = hotReload.GetHistory();
            // Checking if dependents are updated
            bool textureUpdated = false;
            bool materialUpdated = false;
            bool sceneUpdated = false;

            for (const auto& ev : history) {
                if (ev.handle == textures[0]) textureUpdated = true;
                if (ev.handle == materials[0]) materialUpdated = true;
                if (ev.handle == scenes[0]) sceneUpdated = true;
            }

            if (!textureUpdated || !materialUpdated || !sceneUpdated) {
                LOG_ERROR("[HotReloadStressTest] Dependent assets did not reload topologically!");
                return false;
            }
        }

        // Test 2: Stress Reload Loop (100 times)
        {
            auto startStress = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < 100; ++i) {
                // Modify texture handle dynamically
                AssetHandle targetTex = textures[i % textures.size()];
                bool success = hotReload.ReloadAsset(targetTex);
                if (!success) {
                    LOG_ERROR("[HotReloadStressTest] ReloadAsset failed at cycle %d", i);
                    return false;
                }
            }
            auto endStress = std::chrono::high_resolution_clock::now();
            double durationSec = std::chrono::duration<double>(endStress - startStress).count();
            LOG_INFO("[HotReloadStressTest] Completed 100 reload stress cycles in %.3f seconds.", durationSec);
        }

        // Test 3: Circular dependency cycle protection
        {
            // Inject a cycle: textures[0] depends on scenes[0], while scenes[0] depends on materials[0] which depends on textures[0]
            const AssetMetadata* origMeta = registry.GetMetadata(textures[0]);
            AssetMetadata cycleMeta = *origMeta;
            cycleMeta.dependencies = { scenes[0] };
            registry.UpdateMetadata(cycleMeta);

            hotReload.ClearHistory();
            // This should not cause infinite loops/crashes because of cycle tracking
            bool success = hotReload.ReloadAsset(textures[0]);
            if (!success) {
                LOG_ERROR("[HotReloadStressTest] Cycle reload test failed to complete safely!");
                return false;
            }

            // Restore registry state
            registry.UpdateMetadata(*origMeta);
        }

        // Validate final memory allocation stability
        manager.ClearCache();
        uint32_t finalAllocations = StressMockLoader::GetActiveAllocations();
        if (finalAllocations != 0) {
            LOG_ERROR("[HotReloadStressTest] Memory leak detected during hot reload cycles! Leaked: %u", finalAllocations);
            return false;
        }

        LOG_INFO("[HotReloadStressTest] All hot reload stress tests passed successfully.");
        return true;
    }

} // namespace eng::runtime
