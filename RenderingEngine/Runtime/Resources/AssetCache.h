#pragma once
#include <string>
#include <unordered_map>
#include "Core/types/Handle.h"
#include "RenderingEngine/Public/IAssetManager.h"

namespace eng::rhi {
    class Device;
}

namespace eng::runtime {
    class AssetManager;

    class AssetCache : public IAssetManager {
    public:
        explicit AssetCache(eng::rhi::Device* device = nullptr, AssetManager* assetManager = nullptr)
            : m_Device(device), m_AssetManager(assetManager) {}
        ~AssetCache() override = default;

        void SetAssetManager(AssetManager* manager) { m_AssetManager = manager; }
        AssetManager* GetAssetManager() const { return m_AssetManager; }

        template <typename T>
        eng::core::Handle<T> Load(const std::string& path) {
            return eng::core::Handle<T>();
        }

    protected:
        void* LoadRaw(const std::string& path, const std::type_info& typeInfo) override {
            return nullptr;
        }

    private:
        eng::rhi::Device* m_Device = nullptr;
        AssetManager* m_AssetManager = nullptr;
    };

} // namespace eng::runtime
