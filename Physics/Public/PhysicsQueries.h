#pragma once
#include "ECS/ECSconfig.h" // For Entity ID
#include "Scene/Vector3.h"

namespace eng::physics {

    struct PhysicsRay {
        Vector3 origin;
        Vector3 direction;
    };

    struct RaycastHit {
        bool hit = false;
        Entity entity = 0;
        Vector3 position = { 0.0f, 0.0f, 0.0f };
        Vector3 normal = { 0.0f, 0.0f, 0.0f };
        float distance = 0.0f;
    };

} // namespace eng::physics
