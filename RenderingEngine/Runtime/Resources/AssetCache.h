#pragma once
#include <string>
#include <unordered_map>
#include "Core/types/Handle.h"
#include "RenderingEngine/Public/IAssetManager.h"

namespace eng::rhi {
    class Device;
}

namespace eng::runtime {

    class AssetCache : public IAssetManager {
    public:
        explicit AssetCache(eng::rhi::Device* device) {}
        ~AssetCache() override = default;

        template <typename T>
        eng::core::Handle<T> Load(const std::string& path) {
            return eng::core::Handle<T>();
        }

    protected:
        void* LoadRaw(const std::string& path, const std::type_info& typeInfo) override {
            return nullptr;
        }
    };

} // namespace eng::runtime
