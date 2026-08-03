#pragma once
#include <glm/glm.hpp>

namespace eng::renderer {

struct GPUFrustum
{
    glm::vec4 planes[6];
};

} // namespace eng::renderer
