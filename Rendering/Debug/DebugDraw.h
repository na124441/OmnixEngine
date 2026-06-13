#pragma once
#include <glm/glm.hpp>

namespace eng::renderer {

    class DebugDraw {
    public:
        static void DrawLine(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& color);
    };

} // namespace eng::renderer
