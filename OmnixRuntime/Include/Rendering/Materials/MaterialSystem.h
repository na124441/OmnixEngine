#pragma once
#include <string>
#include "RenderingEngine/Core/Engine/EngineResources.h"
#include "RenderingEngine/Renderer/scene/Material.h"

namespace eng::renderer {

    class MaterialSystem {
    public:
        static Material* CreateMaterial(
            const std::string& vertPath,
            const std::string& fragPath,
            const std::string& albedoPath,
            const std::string& normalPath,
            EngineResources& resources
        );
    };

} // namespace eng::renderer
