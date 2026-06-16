#pragma once
#include <glm/glm.hpp>
#include <cstdint>

namespace eng::renderer {

enum RenderFlags : uint32_t {
    RenderFlags_None = 0,
    RenderFlags_Opaque = 1 << 0
};

struct GPUInstance
{
    glm::mat4 worldMatrix;
    glm::mat4 previousWorldMatrix;
    glm::vec4 boundsCenterRadius; // xyz = world-space center, w = world-space radius
    uint32_t meshIndex;
    uint32_t materialIndex;
    uint32_t objectID;
    uint32_t flags;
};

} // namespace eng::renderer
