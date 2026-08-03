#pragma once
#include <glm/glm.hpp>

namespace eng::renderer {

/// Must match exactly the layout in the vertex/fragment shaders:
/// layout(set = 0, binding = 0) uniform GlobalUBO { mat4 view; mat4 proj; vec4 cameraPos; };
struct GlobalUBO
{
    glm::mat4 view      = glm::mat4(1.0f);
    glm::mat4 proj      = glm::mat4(1.0f);
    glm::vec4 cameraPos = glm::vec4(0.0f); // xyz = world-space camera position, w = unused
};

} // namespace eng::renderer
