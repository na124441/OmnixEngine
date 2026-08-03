#pragma once
#include "ECS/ECSComponents.h"

namespace eng::physics {

    constexpr float MIN_COLLIDER_SIZE = 0.001f;
    constexpr float MIN_COLLIDER_RADIUS = 0.001f;

    void ValidateStaticBody(StaticBodyComponent& comp);
    void ValidateBoxCollider(BoxColliderComponent& comp);
    void ValidateSphereCollider(SphereColliderComponent& comp);
    void ValidateCapsuleCollider(CapsuleColliderComponent& comp);

} // namespace eng::physics
