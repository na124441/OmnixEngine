#pragma once
#include <glm/glm.hpp>
#include <vector>

namespace eng::renderer {

    struct DebugLine {
        glm::vec3 p1;
        glm::vec3 p2;
        glm::vec4 color;
    };

    class DebugDraw {
    public:
        static void DrawLine(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& color);
        static void DrawLine(const glm::vec3& p1, const glm::vec3& p2, const glm::vec4& color);
        static void DrawSphere(const glm::vec3& position, float radius, const glm::vec4& color);
        static void DrawCone(const glm::vec3& position, const glm::vec3& direction, float range, float outerAngleRadians, const glm::vec4& color);

        static const std::vector<DebugLine>& GetLines();
        static void ClearLines();
    };

} // namespace eng::renderer

