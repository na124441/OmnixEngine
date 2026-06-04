#include "Runtime/Public/HotReloadTests.h"
#include "Runtime/Public/AssetManager.h"
#include "Runtime/Public/AssetRegistry.h"
#include "Runtime/Public/HotReloadSystem.h"
#include "Core/Logging/Logger.h"

namespace eng::runtime {

    namespace {
        class MockTextureLoader : public IAssetLoader
        {
        public:
            MockTextureLoader(bool failOnLoad = false) : m_FailOnLoad(failOnLoad) {}
            AssetType GetSupportedType() const override { return AssetType::Texture; }
            bool CanLoad(AssetType type) const override { return type == AssetType::Texture; }

            bool Load(const AssetMetadata& metadata, RuntimeAsset** outAsset) override
            {
                if (m_FailOnLoad) return false;
                auto* tex = new RuntimeTexture();
                tex->width = m_Width;
                tex->height = m_Height;
                *outAsset = tex;
                m_LoadCount++;
                return true;
            }

            void Unload(RuntimeAsset* asset) override
            {
                delete asset;
                m_UnloadCount++;
            }

            void SetFail(bool fail) { m_FailOnLoad = fail; }
            void SetDimensions(uint32_t w, uint32_t h) { m_Width = w; m_Height = h; }
            uint32_t GetLoadCount() const { return m_LoadCount; }
            uint32_t GetUnloadCount() const { return m_UnloadCount; }

        private:
            bool m_FailOnLoad = false;
            uint32_t m_Width = 10;
            uint32_t m_Height = 10;
            uint32_t m_LoadCount = 0;
            uint32_t m_UnloadCount = 0;
        };
    }

    bool RunTextureReloadTests() noexcept
    {
        LOG_INFO("[TextureReloadTest] Running texture reload tests...");
        AssetRegistry registry;
        AssetManager manager(registry);

        auto loaderUnique = std::make_unique<MockTextureLoader>();
        auto* loader = loaderUnique.get();
        manager.RegisterLoader(AssetType::Texture, std::move(loaderUnique));

        AssetHandle handle{1001};
        AssetMetadata meta;
        meta.handle = handle;
        meta.type = AssetType::Texture;
        meta.sourcePath = "Assets/Textures/stone.png";
        meta.importedPath = "Cache/Textures/stone.omnixtex";
        meta.isImported = true;
        registry.UpdateMetadata(meta);

        // Load initially
        RuntimeAsset* initial = manager.LoadAsset(handle);
        if (!initial) {
            LOG_ERROR("[TextureReloadTest] Initial load failed!");
            return false;
        }

        HotReloadSystem hotReload(registry, manager);

        // Test 1: Texture Reload Stability (Successful reload)
        {
            loader->SetDimensions(20, 20); // modify size for reload
            bool success = hotReload.ReloadAsset(handle);
            if (!success) {
                LOG_ERROR("[TextureReloadTest] ReloadAsset returned false!");
                return false;
            }

            RuntimeTexture* current = manager.Get<RuntimeTexture>(handle);
            if (!current || current->width != 20) {
                LOG_ERROR("[TextureReloadTest] Swap validation failed, texture not updated!");
                return false;
            }

            if (loader->GetUnloadCount() != 1) {
                LOG_ERROR("[TextureReloadTest] Old texture was not unloaded!");
                return false;
            }
        }

        // Test 2: Failed Texture Reload
        {
            RuntimeTexture* previous = manager.Get<RuntimeTexture>(handle);
            loader->SetFail(true); // make load fail

            bool success = hotReload.ReloadAsset(handle);
            if (success) {
                LOG_ERROR("[TextureReloadTest] ReloadAsset reported success when loader failed!");
                return false;
            }

            // Old working asset must remain active
            RuntimeTexture* current = manager.Get<RuntimeTexture>(handle);
            if (current != previous || current == nullptr) {
                LOG_ERROR("[TextureReloadTest] Failed reload corrupted active cache!");
                return false;
            }
        }

        LOG_INFO("[TextureReloadTest] All texture reload tests passed.");
        return true;
    }

} // namespace eng::runtime
