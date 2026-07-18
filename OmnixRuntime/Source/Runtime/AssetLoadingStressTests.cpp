#include "Runtime/AssetLoadingStressTests.h"
#include "Runtime/AssetManager.h"
#include "Runtime/AssetRegistry.h"
#include "Core/Logging/Logger.h"
#include <chrono>
#include <random>

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
                *outAsset = new RuntimeAsset();
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

            static uint32_t GetLoadCount() { return m_LoadCount; }
            static uint32_t GetUnloadCount() { return m_UnloadCount; }
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

        // Define statics
        uint32_t StressMockLoader::m_LoadCount = 0;
        uint32_t StressMockLoader::m_UnloadCount = 0;
        uint32_t StressMockLoader::m_ActiveCount = 0;

    } // namespace

    bool RunAssetLoadingStressTests() noexcept
    {
        LOG_INFO("================================================================================");
        LOG_INFO("                   RUNNING WEEK 13 ASSET LOADING STRESS TESTS                    ");
        LOG_INFO("================================================================================");

        StressMockLoader::Reset();

        AssetRegistry registry;

        // Generate 100 textures (IDs 1000 to 1099)
        std::vector<AssetHandle> textureHandles;
        for (int i = 0; i < 100; ++i) {
            AssetHandle handle{static_cast<uint64_t>(1000 + i)};
            AssetMetadata meta;
            meta.handle = handle;
            meta.type = AssetType::Texture;
            meta.sourcePath = "Assets/Textures/tex_" + std::to_string(i) + ".png";
            meta.importedPath = "Cache/Textures/tex_" + std::to_string(i) + ".omnixtex";
            meta.isImported = true;
            registry.UpdateMetadata(meta);
            textureHandles.push_back(handle);
        }

        // Generate 50 meshes (IDs 2000 to 2049)
        std::vector<AssetHandle> meshHandles;
        for (int i = 0; i < 50; ++i) {
            AssetHandle handle{static_cast<uint64_t>(2000 + i)};
            AssetMetadata meta;
            meta.handle = handle;
            meta.type = AssetType::Mesh;
            meta.sourcePath = "Assets/Meshes/mesh_" + std::to_string(i) + ".obj";
            meta.importedPath = "Cache/Meshes/mesh_" + std::to_string(i) + ".omnixmesh";
            meta.isImported = true;
            registry.UpdateMetadata(meta);
            meshHandles.push_back(handle);
        }

        // Generate 25 materials depending on textures (IDs 3000 to 3024)
        std::vector<AssetHandle> materialHandles;
        for (int i = 0; i < 25; ++i) {
            AssetHandle handle{static_cast<uint64_t>(3000 + i)};
            AssetMetadata meta;
            meta.handle = handle;
            meta.type = AssetType::Material;
            meta.sourcePath = "Assets/Materials/mat_" + std::to_string(i) + ".mat";
            meta.importedPath = "Cache/Materials/mat_" + std::to_string(i) + ".omnixmat";
            meta.isImported = true;
            // Depend on 2 textures
            meta.dependencies = {
                textureHandles[i % textureHandles.size()],
                textureHandles[(i + 5) % textureHandles.size()]
            };
            registry.UpdateMetadata(meta);
            materialHandles.push_back(handle);
        }

        // Generate 5 scenes (IDs 4000 to 4004) depending on meshes and materials
        std::vector<AssetHandle> sceneHandles;
        for (int i = 0; i < 5; ++i) {
            AssetHandle handle{static_cast<uint64_t>(4000 + i)};
            AssetMetadata meta;
            meta.handle = handle;
            meta.type = AssetType::Scene;
            meta.sourcePath = "Assets/Scenes/scene_" + std::to_string(i) + ".scene";
            meta.importedPath = "Cache/Scenes/scene_" + std::to_string(i) + ".omnixscene";
            meta.isImported = true;
            // Depend on 10 meshes and 5 materials
            for (int m = 0; m < 10; ++m) {
                meta.dependencies.push_back(meshHandles[(i * 10 + m) % meshHandles.size()]);
            }
            for (int mat = 0; mat < 5; ++mat) {
                meta.dependencies.push_back(materialHandles[(i * 5 + mat) % materialHandles.size()]);
            }
            registry.UpdateMetadata(meta);
            sceneHandles.push_back(handle);
        }

        // Setup AssetManager and register StressMockLoaders
        AssetManager manager(registry);
        manager.RegisterLoader(AssetType::Texture, std::make_unique<StressMockLoader>(AssetType::Texture));
        manager.RegisterLoader(AssetType::Mesh, std::make_unique<StressMockLoader>(AssetType::Mesh));
        manager.RegisterLoader(AssetType::Material, std::make_unique<StressMockLoader>(AssetType::Material));
        manager.RegisterLoader(AssetType::Scene, std::make_unique<StressMockLoader>(AssetType::Scene));

        LOG_INFO("[AssetStressTest] Starting 100 load-unload stress cycles...");
        auto startTime = std::chrono::high_resolution_clock::now();

        std::mt19937 rng(1337); // Seeded random number generator
        std::uniform_int_distribution<size_t> textureDist(0, textureHandles.size() - 1);
        std::uniform_int_distribution<size_t> meshDist(0, meshHandles.size() - 1);
        std::uniform_int_distribution<size_t> matDist(0, materialHandles.size() - 1);
        std::uniform_int_distribution<size_t> sceneDist(0, sceneHandles.size() - 1);

        for (int cycle = 0; cycle < 100; ++cycle) {
            // Load all scenes (will trigger loading of their meshes, materials, and textures)
            for (AssetHandle sceneH : sceneHandles) {
                RuntimeAsset* s = manager.GetOrLoad(sceneH);
                if (!s) {
                    LOG_ERROR("[AssetStressTest] Cycle %d FAILED: Failed to load scene %llu", cycle, sceneH.value);
                    return false;
                }
            }

            // Perform repeated requests to check cache hits
            for (int r = 0; r < 50; ++r) {
                AssetHandle randomTex = textureHandles[textureDist(rng)];
                AssetHandle randomMesh = meshHandles[meshDist(rng)];
                AssetHandle randomMat = materialHandles[matDist(rng)];
                AssetHandle randomScene = sceneHandles[sceneDist(rng)];

                manager.GetOrLoad(randomTex);
                manager.GetOrLoad(randomMesh);
                manager.GetOrLoad(randomMat);
                manager.GetOrLoad(randomScene);
            }

            // Clear cache at the end of the cycle to free memory
            manager.ClearCache();

            // Assert memory footprint is stable: active allocations must be exactly 0
            if (StressMockLoader::GetActiveAllocations() != 0) {
                LOG_ERROR("[AssetStressTest] Cycle %d FAILED: Memory leak detected! Active allocations: %u",
                          cycle, StressMockLoader::GetActiveAllocations());
                return false;
            }
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        double durationSeconds = std::chrono::duration<double>(endTime - startTime).count();

        LOG_INFO("[AssetStressTest] Stress tests finished successfully in %.3f seconds.", durationSeconds);

        const AssetManagerStats& stats = manager.GetStats();
        LOG_INFO("[AssetStressTest] Metrics:");
        LOG_INFO("  - Total Loads Attempted: %u", stats.totalLoadsAttempted);
        LOG_INFO("  - Total Load Successes: %u", stats.totalLoadSuccesses);
        LOG_INFO("  - Total Load Failures: %u", stats.totalLoadFailures);
        LOG_INFO("  - Total Cache Hits: %u", stats.totalCacheHits);
        LOG_INFO("  - Total Cache Misses: %u", stats.totalCacheMisses);
        LOG_INFO("  - Total Dependencies Loaded: %u", stats.totalDependenciesLoaded);

        // Final leakage check
        if (StressMockLoader::GetActiveAllocations() != 0) {
            LOG_ERROR("[AssetStressTest] FAILED: Final leakage check failed! Active allocations left: %u",
                      StressMockLoader::GetActiveAllocations());
            return false;
        }

        LOG_INFO("[AssetStressTest] ALL WEEK 13 STRESS TESTS PASSED!");
        return true;
    }

} // namespace eng::runtime
