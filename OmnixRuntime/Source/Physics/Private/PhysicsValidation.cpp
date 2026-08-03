#include "Physics/Public/PhysicsValidation.h"
#include "Core/Logging/Logger.h"
#include <cmath>
#include <algorithm>

namespace eng::physics {

    bool IsFloatValid(float f) noexcept {
        return !std::isnan(f) && !std::isinf(f);
    }

    void ValidateStaticBody(StaticBodyComponent& comp) {
        // Layers and masks are uint32_t, so they are always valid numbers.
    }

    void ValidateBoxCollider(BoxColliderComponent& comp) {
        if (!IsFloatValid(comp.size.x) || comp.size.x < MIN_COLLIDER_SIZE) {
            comp.size.x = MIN_COLLIDER_SIZE;
        }
        if (!IsFloatValid(comp.size.y) || comp.size.y < MIN_COLLIDER_SIZE) {
            comp.size.y = MIN_COLLIDER_SIZE;
        }
        if (!IsFloatValid(comp.size.z) || comp.size.z < MIN_COLLIDER_SIZE) {
            comp.size.z = MIN_COLLIDER_SIZE;
        }

        if (!IsFloatValid(comp.offset.x)) comp.offset.x = 0.0f;
        if (!IsFloatValid(comp.offset.y)) comp.offset.y = 0.0f;
        if (!IsFloatValid(comp.offset.z)) comp.offset.z = 0.0f;
    }

    void ValidateSphereCollider(SphereColliderComponent& comp) {
        if (!IsFloatValid(comp.radius) || comp.radius < MIN_COLLIDER_RADIUS) {
            comp.radius = MIN_COLLIDER_RADIUS;
        }

        if (!IsFloatValid(comp.offset.x)) comp.offset.x = 0.0f;
        if (!IsFloatValid(comp.offset.y)) comp.offset.y = 0.0f;
        if (!IsFloatValid(comp.offset.z)) comp.offset.z = 0.0f;
    }

    void ValidateCapsuleCollider(CapsuleColliderComponent& comp) {
        if (!IsFloatValid(comp.radius) || comp.radius < MIN_COLLIDER_RADIUS) {
            comp.radius = MIN_COLLIDER_RADIUS;
        }

        // Total height must be at least twice the radius (diameter)
        float minHeight = comp.radius * 2.0f;
        if (!IsFloatValid(comp.height) || comp.height < minHeight) {
            comp.height = minHeight;
        }

        if (!IsFloatValid(comp.offset.x)) comp.offset.x = 0.0f;
        if (!IsFloatValid(comp.offset.y)) comp.offset.y = 0.0f;
        if (!IsFloatValid(comp.offset.z)) comp.offset.z = 0.0f;
    }

} // namespace eng::physics
