#pragma once
#include "ECS/Coordinator.h"
#include "Scene/Vector3.h"
#include <glm/glm.hpp>
#include <vector>

namespace eng::physics {

    struct DebugRaycastInfo {
        Vector3 origin;
        Vector3 direction;
        float distance;
        bool hit;
        Vector3 hitPoint;
        Vector3 hitNormal;
    };

    class PhysicsDebugDraw {
    public:
        static void Render(Coordinator& coordinator, const glm::mat4& view, const glm::mat4& proj, float screenWidth, float screenHeight, float viewportOffsetX = 0.0f, float viewportOffsetY = 0.0f);
        static void RenderBounds(Coordinator& coordinator, const glm::mat4& view, const glm::mat4& proj, float screenWidth, float screenHeight, float viewportOffsetX = 0.0f, float viewportOffsetY = 0.0f);
        static void AddDebugRaycast(const Vector3& origin, const Vector3& direction, float distance, bool hit, const Vector3& hitPoint, const Vector3& hitNormal);
        static void ClearDebugVisuals();

    private:
        static std::vector<DebugRaycastInfo> s_Raycasts;
    };
}
