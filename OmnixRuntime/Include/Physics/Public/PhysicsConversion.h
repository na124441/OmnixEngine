#pragma once

#include "Scene/Vector3.h"
#include "Scene/Quaternion.h"

#include <foundation/PxVec3.h>
#include <foundation/PxQuat.h>
#include <foundation/PxTransform.h>

namespace eng::physics {

    physx::PxVec3 ToPxVec3(const Vector3& vec);
    Vector3 ToVector3(const physx::PxVec3& vec);

    physx::PxQuat ToPxQuat(const Quaternion& quat);
    Quaternion ToQuaternion(const physx::PxQuat& quat);

    physx::PxTransform ToPxTransform(const Vector3& pos, const Quaternion& rot);

} // namespace eng::physics
