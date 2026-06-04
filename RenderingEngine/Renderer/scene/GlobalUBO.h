#pragma once
#include <glm/glm.hpp>

namespace eng::renderer {

/// Must match exactly the layout in the vertex shader:
/// layout(set = 0, binding = 0) uniform GlobalUBO { mat4 view; mat4 proj; };
struct GlobalUBO
{
    glm::mat4 view   = glm::mat4(1.0f);
    glm::mat4 proj   = glm::mat4(1.0f);
};

} // namespace eng::renderer
