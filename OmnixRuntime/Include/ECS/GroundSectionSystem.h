#pragma once
#include "ECS/SystemManager.h"
#include "ECSConfig.h"
#include "ECS/ECSComponents.h"
#include "ECS/Coordinator.h"
#include "Runtime/World/GroundSectionComponent.h"
#include "Rendering/Debug/DebugDraw.h"

class GroundSectionSystem : public System {
public:
    void Update(float deltaTime, Coordinator& coordinator) {
        auto type = coordinator.GetComponentType<eng::runtime::GroundSectionComponent>();
        for (Entity entity : m_Entities) {
            if (!coordinator.IsEntityAlive(entity)) {
                continue;
            }
            auto sig = coordinator.GetSignature(entity);
            if (sig.test(type)) {
                const auto& gsc = coordinator.GetComponent<eng::runtime::GroundSectionComponent>(entity);
                if (gsc.debugDraw) {
                    DrawBoundingBox(gsc.boundsMin, gsc.boundsMax, {0.0f, 1.0f, 0.0f, 1.0f}); // Green box
                }
            }
        }
    }

private:
    void DrawBoundingBox(const Vector3& min, const Vector3& max, const glm::vec4& color) {
        glm::vec3 p0 = {min.x, min.y, min.z};
        glm::vec3 p1 = {max.x, min.y, min.z};
        glm::vec3 p2 = {max.x, max.y, min.z};
        glm::vec3 p3 = {min.x, max.y, min.z};
        glm::vec3 p4 = {min.x, min.y, max.z};
        glm::vec3 p5 = {max.x, min.y, max.z};
        glm::vec3 p6 = {max.x, max.y, max.z};
        glm::vec3 p7 = {min.x, max.y, max.z};

        // Bottom ring
        eng::renderer::DebugDraw::DrawLine(p0, p1, color);
        eng::renderer::DebugDraw::DrawLine(p1, p5, color);
        eng::renderer::DebugDraw::DrawLine(p5, p4, color);
        eng::renderer::DebugDraw::DrawLine(p4, p0, color);

        // Top ring
        eng::renderer::DebugDraw::DrawLine(p3, p2, color);
        eng::renderer::DebugDraw::DrawLine(p2, p6, color);
        eng::renderer::DebugDraw::DrawLine(p6, p7, color);
        eng::renderer::DebugDraw::DrawLine(p7, p3, color);

        // Vertical edges
        eng::renderer::DebugDraw::DrawLine(p0, p3, color);
        eng::renderer::DebugDraw::DrawLine(p1, p2, color);
        eng::renderer::DebugDraw::DrawLine(p5, p6, color);
        eng::renderer::DebugDraw::DrawLine(p4, p7, color);
    }
};
