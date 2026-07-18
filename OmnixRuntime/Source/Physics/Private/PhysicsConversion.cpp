#include "Physics/Public/PhysicsConversion.h"
#include <PxPhysicsAPI.h>

namespace eng::physics {

    physx::PxVec3 ToPxVec3(const Vector3& vec) {
        return physx::PxVec3(vec.x, vec.y, vec.z);
    }

    Vector3 ToVector3(const physx::PxVec3& vec) {
        return Vector3(vec.x, vec.y, vec.z);
    }

    physx::PxQuat ToPxQuat(const Quaternion& quat) {
        // Quat structure is usually (x, y, z, w)
        return physx::PxQuat(quat.x, quat.y, quat.z, quat.w);
    }

    Quaternion ToQuaternion(const physx::PxQuat& quat) {
        return Quaternion(quat.x, quat.y, quat.z, quat.w);
    }

    physx::PxTransform ToPxTransform(const Vector3& pos, const Quaternion& rot) {
        return physx::PxTransform(ToPxVec3(pos), ToPxQuat(rot));
    }

} // namespace eng::physics
