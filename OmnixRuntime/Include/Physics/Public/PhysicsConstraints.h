#pragma once

#include "ECS/ECSconfig.h"
#include "Scene/Vector3.h"

namespace eng::physics {

    enum class JointType {
        Fixed,
        Distance,
        Hinge,
        Spherical
    };

    struct PhysicsJoint {
        JointType type = JointType::Fixed;
        Entity entityA = 0;
        Entity entityB = 0;

        Vector3 localAnchorA = { 0.0f, 0.0f, 0.0f };
        Vector3 localAnchorB = { 0.0f, 0.0f, 0.0f };

        float minDistance = 0.0f;
        float maxDistance = 1.0f;

        float breakForce = 1e30f;
        float breakTorque = 1e30f;
        bool isBroken = false;
    };

} // namespace eng::physics
