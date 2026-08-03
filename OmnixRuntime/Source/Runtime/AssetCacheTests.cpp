#include "Runtime/AssetCacheTests.h"
#include "Runtime/AssetManager.h"
#include "Runtime/AssetRegistry.h"
#include "Core/Logging/Logger.h"
#include <iostream>

namespace eng::runtime {

    namespace {

        class MockTestLoader : public IAssetLoader
        {
        public:
            MockTestLoader(AssetType supportedType, bool failOnLoad = false)
                : m_Type(supportedType), m_FailOnLoad(failOnLoad)
            {
            }

            AssetType GetSupportedType() const override { return m_Type; }
            bool CanLoad(AssetType type) const override { return type == m_Type; }

            bool Load(const AssetMetadata& metadata, RuntimeAsset** outAsset) override
            {
                m_LoadCount++;
                if (m_FailOnLoad) {
                    return false;
                }

                if (m_Type == AssetType::Texture) {
                    *outAsset = new RuntimeTexture();
                } else if (m_Type == AssetType::Mesh) {
                    *outAsset = new RuntimeMesh();
                } else if (m_Type == AssetType::Material) {
                    *outAsset = new RuntimeMaterial();
                } else {
                    *outAsset = new RuntimeAsset();
                }

                (*outAsset)->handle = metadata.handle;
                (*outAsset)->type = m_Type;
                (*outAsset)->debugName = metadata.sourcePath;

                return true;
            }

            void Unload(RuntimeAsset* asset) override
            {
                delete asset;
                m_UnloadCount++;
            }

            uint32_t GetLoadCount() const { return m_LoadCount; }
            uint32_t GetUnloadCount() const { return m_UnloadCount; }

        private:
            AssetType m_Type;
            bool m_FailOnLoad = false;
            uint32_t m_LoadCount = 0;
            uint32_t m_UnloadCount = 0;
        };

    } // namespace

    bool RunAssetCacheTests() noexcept
    {
        LOG_INFO("================================================================================");
        LOG_INFO("                   RUNNING WEEK 13 ASSET CACHE & LOADING TESTS                  ");
        LOG_INFO("================================================================================");

        // Setup mock registry and metadata
        AssetRegistry registry;

        // Texture 1
        AssetHandle texHandle1{101};
        AssetMetadata texMeta1;
        texMeta1.handle = texHandle1;
        texMeta1.type = AssetType::Texture;
        texMeta1.sourcePath = "Assets/Textures/stone.png";
        texMeta1.importedPath = "Cache/Textures/stone.omnixtex";
        texMeta1.isImported = true;
        registry.UpdateMetadata(texMeta1);

        // Texture 2 (Different)
        AssetHandle texHandle2{102};
        AssetMetadata texMeta2;
        texMeta2.handle = texHandle2;
        texMeta2.type = AssetType::Texture;
        texMeta2.sourcePath = "Assets/Textures/wood.png";
        texMeta2.importedPath = "Cache/Textures/wood.omnixtex";
        texMeta2.isImported = true;
        registry.UpdateMetadata(texMeta2);

        // Mesh
        AssetHandle meshHandle1{501};
        AssetMetadata meshMeta1;
        meshMeta1.handle = meshHandle1;
        meshMeta1.type = AssetType::Mesh;
        meshMeta1.sourcePath = "Assets/Meshes/cube.obj";
        meshMeta1.importedPath = "Cache/Meshes/cube.omnixmesh";
        meshMeta1.isImported = true;
        registry.UpdateMetadata(meshMeta1);

        // Shader
        AssetHandle shaderHandle1{301};
        AssetMetadata shaderMeta1;
        shaderMeta1.handle = shaderHandle1;
        shaderMeta1.type = AssetType::Shader;
        shaderMeta1.sourcePath = "Assets/Shaders/pbr.glsl";
        shaderMeta1.importedPath = "Cache/Shaders/pbr.omnixspv";
        shaderMeta1.isImported = true;
        registry.UpdateMetadata(shaderMeta1);

        // Material (depends on Texture 1 and Shader 1)
        AssetHandle matHandle1{201};
        AssetMetadata matMeta1;
        matMeta1.handle = matHandle1;
        matMeta1.type = AssetType::Material;
        matMeta1.sourcePath = "Assets/Materials/stone.mat";
        matMeta1.importedPath = "Cache/Materials/stone.omnixmat";
        matMeta1.isImported = true;
        matMeta1.dependencies = { texHandle1, shaderHandle1 };
        registry.UpdateMetadata(matMeta1);

        // Material 2 (also depends on Texture 1 - shared dependency test)
        AssetHandle matHandle2{202};
        AssetMetadata matMeta2;
        matMeta2.handle = matHandle2;
        matMeta2.type = AssetType::Material;
        matMeta2.sourcePath = "Assets/Materials/stone_floor.mat";
        matMeta2.importedPath = "Cache/Materials/stone_floor.omnixmat";
        matMeta2.isImported = true;
        matMeta2.dependencies = { texHandle1 };
        registry.UpdateMetadata(matMeta2);

        // -----------------------------------------------------------------------------
        // Test 1 — Loader Dispatch Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[AssetCacheTest] Running Test 1: Loader Dispatch Test...");
        {
            AssetManager manager(registry);

            auto texLoaderUnique = std::make_unique<MockTestLoader>(AssetType::Texture);
            auto meshLoaderUnique = std::make_unique<MockTestLoader>(AssetType::Mesh);

            MockTestLoader* texLoader = texLoaderUnique.get();
            MockTestLoader* meshLoader = meshLoaderUnique.get();

            manager.RegisterLoader(AssetType::Texture, std::move(texLoaderUnique));
            manager.RegisterLoader(AssetType::Mesh, std::move(meshLoaderUnique));

            // Load texture
            RuntimeAsset* asset = manager.LoadAsset(texHandle1);
            if (!asset) {
                LOG_ERROR("[AssetCacheTest] Test 1 FAILED: Failed to load texture!");
                return false;
            }

            if (texLoader->GetLoadCount() != 1) {
                LOG_ERROR("[AssetCacheTest] Test 1 FAILED: Texture loader was not called!");
                return false;
            }

            if (meshLoader->GetLoadCount() != 0) {
                LOG_ERROR("[AssetCacheTest] Test 1 FAILED: Mesh loader was wrongly called!");
                return false;
            }
            LOG_INFO("[AssetCacheTest] Test 1 Passed.");
        }

        // -----------------------------------------------------------------------------
        // Test 2 — Cache Reuse Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[AssetCacheTest] Running Test 2: Cache Reuse Test...");
        {
            AssetManager manager(registry);
            auto texLoaderUnique = std::make_unique<MockTestLoader>(AssetType::Texture);
            MockTestLoader* texLoader = texLoaderUnique.get();
            manager.RegisterLoader(AssetType::Texture, std::move(texLoaderUnique));

            RuntimeAsset* a = manager.LoadAsset(texHandle1);
            RuntimeAsset* b = manager.LoadAsset(texHandle1);

            if (a == nullptr || b == nullptr) {
                LOG_ERROR("[AssetCacheTest] Test 2 FAILED: Asset pointer is null!");
                return false;
            }

            if (a != b) {
                LOG_ERROR("[AssetCacheTest] Test 2 FAILED: Cached pointer returned is different!");
                return false;
            }

            if (texLoader->GetLoadCount() != 1) {
                LOG_ERROR("[AssetCacheTest] Test 2 FAILED: Asset was loaded more than once! LoadCount: %u", texLoader->GetLoadCount());
                return false;
            }

            if (manager.GetStats().totalCacheHits != 1 || manager.GetStats().totalCacheMisses != 1) {
                LOG_ERROR("[AssetCacheTest] Test 2 FAILED: Cache stats mismatch!");
                return false;
            }
            LOG_INFO("[AssetCacheTest] Test 2 Passed.");
        }

        // -----------------------------------------------------------------------------
        // Test 3 — Different Handle Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[AssetCacheTest] Running Test 3: Different Handle Test...");
        {
            AssetManager manager(registry);
            auto texLoaderUnique = std::make_unique<MockTestLoader>(AssetType::Texture);
            manager.RegisterLoader(AssetType::Texture, std::move(texLoaderUnique));

            RuntimeAsset* a = manager.LoadAsset(texHandle1);
            RuntimeAsset* b = manager.LoadAsset(texHandle2);

            if (a == nullptr || b == nullptr) {
                LOG_ERROR("[AssetCacheTest] Test 3 FAILED: Returned asset pointer is null!");
                return false;
            }

            if (a == b) {
                LOG_ERROR("[AssetCacheTest] Test 3 FAILED: Different assets returned same pointer!");
                return false;
            }
            LOG_INFO("[AssetCacheTest] Test 3 Passed.");
        }

        // -----------------------------------------------------------------------------
        // Test 4 — Dependency Load Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[AssetCacheTest] Running Test 4: Dependency Load Test...");
        {
            AssetManager manager(registry);
            manager.RegisterLoader(AssetType::Texture, std::make_unique<MockTestLoader>(AssetType::Texture));
            manager.RegisterLoader(AssetType::Shader, std::make_unique<MockTestLoader>(AssetType::Shader));
            manager.RegisterLoader(AssetType::Material, std::make_unique<MockTestLoader>(AssetType::Material));

            // Load material (which depends on Texture 1 and Shader 1)
            RuntimeAsset* mat = manager.LoadAsset(matHandle1);
            if (!mat) {
                LOG_ERROR("[AssetCacheTest] Test 4 FAILED: Material load failed!");
                return false;
            }

            // Verify dependencies were loaded and cached
            RuntimeAsset* tex = manager.GetAsset(texHandle1);
            RuntimeAsset* shader = manager.GetAsset(shaderHandle1);

            if (!tex || !shader) {
                LOG_ERROR("[AssetCacheTest] Test 4 FAILED: Dependencies were not loaded into the cache!");
                return false;
            }

            if (manager.GetStats().totalDependenciesLoaded != 2) {
                LOG_ERROR("[AssetCacheTest] Test 4 FAILED: Dependencies count stats mismatch! Count: %u", manager.GetStats().totalDependenciesLoaded);
                return false;
            }
            LOG_INFO("[AssetCacheTest] Test 4 Passed.");
        }

        // -----------------------------------------------------------------------------
        // Test 5 — Lazy Load Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[AssetCacheTest] Running Test 5: Lazy Load Test...");
        {
            AssetManager manager(registry);
            auto texLoaderUnique = std::make_unique<MockTestLoader>(AssetType::Texture);
            MockTestLoader* texLoader = texLoaderUnique.get();
            manager.RegisterLoader(AssetType::Texture, std::move(texLoaderUnique));

            // Check it is unloaded
            if (manager.GetLoadState(texHandle1) != AssetLoadState::Unloaded) {
                LOG_ERROR("[AssetCacheTest] Test 5 FAILED: Unrequested asset is not Unloaded!");
                return false;
            }

            // GetOrLoad on demand
            RuntimeAsset* asset = manager.GetOrLoad(texHandle1);
            if (!asset) {
                LOG_ERROR("[AssetCacheTest] Test 5 FAILED: GetOrLoad returned nullptr!");
                return false;
            }

            if (manager.GetLoadState(texHandle1) != AssetLoadState::Loaded) {
                LOG_ERROR("[AssetCacheTest] Test 5 FAILED: Loaded state was not updated to Loaded!");
                return false;
            }

            if (texLoader->GetLoadCount() != 1) {
                LOG_ERROR("[AssetCacheTest] Test 5 FAILED: Asset loader load count should be 1!");
                return false;
            }
            LOG_INFO("[AssetCacheTest] Test 5 Passed.");
        }

        // -----------------------------------------------------------------------------
        // Test 6 — Missing Loader Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[AssetCacheTest] Running Test 6: Missing Loader Test...");
        {
            AssetManager manager(registry);
            // No loader registered for texture type

            RuntimeAsset* asset = manager.LoadAsset(texHandle1);
            if (asset != nullptr) {
                LOG_ERROR("[AssetCacheTest] Test 6 FAILED: Load succeeded despite missing loader!");
                return false;
            }

            if (manager.GetLoadState(texHandle1) != AssetLoadState::Failed) {
                LOG_ERROR("[AssetCacheTest] Test 6 FAILED: Load state is not Failed!");
                return false;
            }

            const AssetDiagnosticInfo* diag = manager.GetDiagnosticInfo(texHandle1);
            if (!diag || diag->lastError.find("No loader registered") == std::string::npos) {
                LOG_ERROR("[AssetCacheTest] Test 6 FAILED: Missing or incorrect diagnostic errors!");
                return false;
            }
            LOG_INFO("[AssetCacheTest] Test 6 Passed.");
        }

        // -----------------------------------------------------------------------------
        // Test 7 — Failed Load State Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[AssetCacheTest] Running Test 7: Failed Load State Test...");
        {
            AssetManager manager(registry);

            // Register loader that will fail on Load call
            manager.RegisterLoader(AssetType::Texture, std::make_unique<MockTestLoader>(AssetType::Texture, true));

            RuntimeAsset* asset = manager.LoadAsset(texHandle1);
            if (asset != nullptr) {
                LOG_ERROR("[AssetCacheTest] Test 7 FAILED: Load succeeded despite loader failure!");
                return false;
            }

            if (manager.GetLoadState(texHandle1) != AssetLoadState::Failed) {
                LOG_ERROR("[AssetCacheTest] Test 7 FAILED: State is not Failed!");
                return false;
            }

            const AssetDiagnosticInfo* diag = manager.GetDiagnosticInfo(texHandle1);
            if (!diag || diag->lastError.find("Loader failed to load file") == std::string::npos) {
                LOG_ERROR("[AssetCacheTest] Test 7 FAILED: Failure diagnostic was not recorded properly!");
                return false;
            }
            LOG_INFO("[AssetCacheTest] Test 7 Passed.");
        }

        // -----------------------------------------------------------------------------
        // Test 8 — Dependency Cache Test
        // -----------------------------------------------------------------------------
        LOG_INFO("[AssetCacheTest] Running Test 8: Dependency Cache Test...");
        {
            AssetManager manager(registry);
            auto texLoaderUnique = std::make_unique<MockTestLoader>(AssetType::Texture);
            MockTestLoader* texLoader = texLoaderUnique.get();
            manager.RegisterLoader(AssetType::Texture, std::move(texLoaderUnique));
            manager.RegisterLoader(AssetType::Material, std::make_unique<MockTestLoader>(AssetType::Material));

            // Load Material 1 and Material 2 (both share Texture 1)
            RuntimeAsset* mat1 = manager.LoadAsset(matHandle1); // Depends on Texture 1 and Shader 1. Shader loader is missing, so it fails, but Texture 1 still loads. Wait, Shader loader is missing so mat1 load fails, but Texture 1 is already loaded.
            // Let's add shader loader so it succeeds
            manager.RegisterLoader(AssetType::Shader, std::make_unique<MockTestLoader>(AssetType::Shader));

            RuntimeAsset* loadedMat1 = manager.LoadAsset(matHandle1);
            RuntimeAsset* loadedMat2 = manager.LoadAsset(matHandle2);

            if (!loadedMat1 || !loadedMat2) {
                LOG_ERROR("[AssetCacheTest] Test 8 FAILED: Loaded materials are null!");
                return false;
            }

            // Verify shared Texture 1 is loaded exactly once
            if (texLoader->GetLoadCount() != 1) {
                LOG_ERROR("[AssetCacheTest] Test 8 FAILED: Shared texture loaded %u times instead of 1!", texLoader->GetLoadCount());
                return false;
            }
            LOG_INFO("[AssetCacheTest] Test 8 Passed.");
        }

        LOG_INFO("[AssetCacheTest] ALL WEEK 13 CACHE VALIDATION TESTS PASSED!");
        return true;
    }

} // namespace eng::runtime
