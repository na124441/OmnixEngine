#pragma once

#include "Core/types/Handle.h"
#include <string>

namespace eng::runtime {

    class IAssetManager {
    public:
        virtual ~IAssetManager() = default;

        template <typename T>
        eng::core::Handle<T> Load(const std::string& path) {
            return LoadImpl<T>(path);
        }

    protected:
        // Implementation hook to be overridden by subclasses
        virtual void* LoadRaw(const std::string& path, const std::type_info& typeInfo) = 0;

    private:
        template <typename T>
        eng::core::Handle<T> LoadImpl(const std::string& path) {
            void* rawAsset = LoadRaw(path, typeid(T));
            // Cast or wrap raw asset in handle
            return eng::core::Handle<T>();
        }
    };

} // namespace eng::runtime
