#pragma once

#include "Scene/Vector3.h"
#include "Scene/Quaternion.h"

namespace eng::runtime {

    Vector3 QuaternionToEuler(const Quaternion& q);
    Quaternion EulerToQuaternion(float rollX, float pitchY, float yawZ);
    bool IsFiniteVec3(const Vector3& v);
    Quaternion NormalizeQuaternion(const Quaternion& q);

} // namespace eng::runtime
