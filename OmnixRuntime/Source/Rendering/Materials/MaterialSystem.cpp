#include "Core/pch.h"
#include "Rendering/Materials/MaterialSystem.h"

namespace eng::renderer {

Material* MaterialSystem::CreateMaterial(
    const std::string& vertPath,
    const std::string& fragPath,
    const std::string& albedoPath,
    const std::string& normalPath,
    EngineResources& resources
)
{
    Material* mat = new Material();
    if (!mat->create(vertPath, fragPath, albedoPath, normalPath, resources)) {
        delete mat;
        return nullptr;
    }
    return mat;
}

} // namespace eng::renderer
