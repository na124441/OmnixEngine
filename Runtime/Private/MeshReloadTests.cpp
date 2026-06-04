#include "Runtime/Public/HotReloadTests.h"
#include "Runtime/Public/AssetManager.h"
#include "Runtime/Public/AssetRegistry.h"
#include "Runtime/Public/HotReloadSystem.h"
#include "Core/Logging/Logger.h"

namespace eng::runtime {

    namespace {
        class MockMeshLoader : public IAssetLoader
        {
        public:
            MockMeshLoader() = default;
            AssetType GetSupportedType() const override { return AssetType::Mesh; }
            bool CanLoad(AssetType type) const override { return type == AssetType::Mesh; }

            bool Load(const AssetMetadata& metadata, RuntimeAsset** outAsset) override
            {
                auto* mesh = new RuntimeMesh();
                mesh->vertexCount = m_VertexCount;
                mesh->indexCount = m_IndexCount;
                mesh->sphere.radius = m_Radius;
                *outAsset = mesh;
                m_LoadCount++;
                return true;
            }

            void Unload(RuntimeAsset* asset) override
            {
                delete asset;
                m_UnloadCount++;
            }

            void SetCount(uint32_t v, uint32_t i)
            {
                m_VertexCount = v;
                m_IndexCount = i;
            }

            void SetRadius(float r) { m_Radius = r; }
            uint32_t GetUnloadCount() const { return m_UnloadCount; }

        private:
            uint32_t m_VertexCount = 8;
            uint32_t m_IndexCount = 36;
            float m_Radius = 1.0f;
            uint32_t m_LoadCount = 0;
            uint32_t m_UnloadCount = 0;
        };
    }

    bool RunMeshReloadTests() noexcept
    {
        LOG_INFO("[MeshReloadTest] Running mesh reload tests...");
        AssetRegistry registry;
        AssetManager manager(registry);

        auto loaderUnique = std::make_unique<MockMeshLoader>();
        auto* loader = loaderUnique.get();
        manager.RegisterLoader(AssetType::Mesh, std::move(loaderUnique));

        AssetHandle handle{5001};
        AssetMetadata meta;
        meta.handle = handle;
        meta.type = AssetType::Mesh;
        meta.sourcePath = "Assets/Meshes/cube.obj";
        meta.importedPath = "Cache/Meshes/cube.omnixmesh";
        meta.isImported = true;
        registry.UpdateMetadata(meta);

        // Load initially
        RuntimeAsset* initial = manager.LoadAsset(handle);
        if (!initial) {
            LOG_ERROR("[MeshReloadTest] Initial load failed!");
            return false;
        }

        HotReloadSystem hotReload(registry, manager);

        // Test 1: Successful Reload and Swap
        {
            loader->SetCount(24, 72);
            loader->SetRadius(2.5f);

            bool success = hotReload.ReloadAsset(handle);
            if (!success) {
                LOG_ERROR("[MeshReloadTest] ReloadAsset failed!");
                return false;
            }

            RuntimeMesh* current = manager.Get<RuntimeMesh>(handle);
            if (!current || current->vertexCount != 24 || current->sphere.radius != 2.5f) {
                LOG_ERROR("[MeshReloadTest] Swap validation failed, mesh not updated!");
                return false;
            }

            if (loader->GetUnloadCount() != 1) {
                LOG_ERROR("[MeshReloadTest] Old mesh was not unloaded!");
                return false;
            }
        }

        // Test 2: Validation Failure Rejection
        {
            RuntimeMesh* previous = manager.Get<RuntimeMesh>(handle);
            loader->SetCount(0, 0); // invalid counts!

            bool success = hotReload.ReloadAsset(handle);
            if (success) {
                LOG_ERROR("[MeshReloadTest] ReloadAsset reported success for invalid vertex/index counts!");
                return false;
            }

            // Verify old mesh remains active
            RuntimeMesh* current = manager.Get<RuntimeMesh>(handle);
            if (current != previous || current == nullptr) {
                LOG_ERROR("[MeshReloadTest] Failed reload corrupted active mesh cache!");
                return false;
            }
        }

        LOG_INFO("[MeshReloadTest] All mesh reload tests passed.");
        return true;
    }

} // namespace eng::runtime
