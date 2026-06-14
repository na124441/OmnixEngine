#include "Core/pch.h"
#include "DebugDraw.h"
#include <cmath>

namespace eng::renderer {

static std::vector<DebugLine> s_DebugLines;

void DebugDraw::DrawLine(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& color) {
    s_DebugLines.push_back({ p1, p2, glm::vec4(color, 1.0f) });
}

void DebugDraw::DrawLine(const glm::vec3& p1, const glm::vec3& p2, const glm::vec4& color) {
    s_DebugLines.push_back({ p1, p2, color });
}

void DebugDraw::DrawSphere(const glm::vec3& position, float radius, const glm::vec4& color) {
    const int segments = 16;
    
    // XY circle
    {
        glm::vec3 prevPoint;
        for (int i = 0; i <= segments; ++i) {
            float angle = (float)i * 2.0f * 3.14159265f / (float)segments;
            glm::vec3 p = position + glm::vec3(cosf(angle) * radius, sinf(angle) * radius, 0.0f);
            if (i > 0) {
                DrawLine(prevPoint, p, color);
            }
            prevPoint = p;
        }
    }
    
    // XZ circle
    {
        glm::vec3 prevPoint;
        for (int i = 0; i <= segments; ++i) {
            float angle = (float)i * 2.0f * 3.14159265f / (float)segments;
            glm::vec3 p = position + glm::vec3(cosf(angle) * radius, 0.0f, sinf(angle) * radius);
            if (i > 0) {
                DrawLine(prevPoint, p, color);
            }
            prevPoint = p;
        }
    }
    
    // YZ circle
    {
        glm::vec3 prevPoint;
        for (int i = 0; i <= segments; ++i) {
            float angle = (float)i * 2.0f * 3.14159265f / (float)segments;
            glm::vec3 p = position + glm::vec3(0.0f, cosf(angle) * radius, sinf(angle) * radius);
            if (i > 0) {
                DrawLine(prevPoint, p, color);
            }
            prevPoint = p;
        }
    }
}

void DebugDraw::DrawCone(
    const glm::vec3& position,
    const glm::vec3& direction,
    float range,
    float outerAngle,
    const glm::vec4& color)
{
    float len = glm::length(direction);
    glm::vec3 forward = len > 0.0001f ? (direction / len) : glm::vec3(0.0f, 0.0f, 1.0f);

    glm::vec3 up = glm::abs(forward.y) > 0.99f
        ? glm::vec3(1.0f, 0.0f, 0.0f)
        : glm::vec3(0.0f, 1.0f, 0.0f);

    glm::vec3 right = glm::normalize(glm::cross(up, forward));
    glm::vec3 realUp = glm::normalize(glm::cross(forward, right));

    float radius = tanf(outerAngle) * range;
    glm::vec3 center = position + forward * range;

    const int segments = 16;
    glm::vec3 prevPoint;
    bool hasPrev = false;

    for (int i = 0; i <= segments; ++i)
    {
        float angle = (float)i * 2.0f * 3.14159265f / (float)segments;
        glm::vec3 p = center + (right * cosf(angle) + realUp * sinf(angle)) * radius;

        if (hasPrev)
        {
            DrawLine(prevPoint, p, color);
        }

        if (i % 4 == 0)
        {
            DrawLine(position, p, color);
        }

        prevPoint = p;
        hasPrev = true;
    }
}

const std::vector<DebugLine>& DebugDraw::GetLines() {
    return s_DebugLines;
}

void DebugDraw::ClearLines() {
    s_DebugLines.clear();
}

} // namespace eng::renderer

