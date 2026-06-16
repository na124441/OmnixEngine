#pragma once
#include <glm/glm.hpp>

namespace eng::renderer {

    struct Vertex {
        glm::vec3 pos;
        glm::vec3 color;
        glm::vec2 uv;
    };

    struct PbrVertex {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec2 uv;
        glm::vec4 tangent;
    };

} // namespace eng::renderer
