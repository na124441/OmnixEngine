#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <vulkan/vulkan.h>
#include "RenderingEngine/Renderer/scene/Texture.h"
#include "RenderingEngine/Core/Engine/EngineResources.h"

namespace eng::renderer {

    class ReflectionProbeRegistry {
    public:
        static ReflectionProbeRegistry& Get() {
            static ReflectionProbeRegistry instance;
            return instance;
        }

        void Register(const std::string& path, std::unique_ptr<Texture> texture) {
            m_Probes[path] = std::move(texture);
        }

        Texture* GetTexture(const std::string& path) const {
            auto it = m_Probes.find(path);
            if (it != m_Probes.end()) {
                return it->second.get();
            }
            return nullptr;
        }

        void Clear() {
            m_Probes.clear();
        }

    private:
        std::unordered_map<std::string, std::unique_ptr<Texture>> m_Probes;
    };

} // namespace eng::renderer
