#include "Runtime/Public/Editor/EditorMath.h"
#include <cmath>
#include <algorithm>

namespace eng::runtime {

    Vector3 QuaternionToEuler(const Quaternion& q) {
        Vector3 euler;

        // Roll (X-axis rotation)
        float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
        float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
        euler.x = std::atan2(sinr_cosp, cosr_cosp);

        // Pitch (Y-axis rotation)
        float sinp = 2.0f * (q.w * q.y - q.z * q.x);
        if (std::abs(sinp) >= 1.0f) {
            euler.y = std::copysign(3.14159265f / 2.0f, sinp);
        } else {
            euler.y = std::asin(sinp);
        }

        // Yaw (Z-axis rotation)
        float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
        float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
        euler.z = std::atan2(siny_cosp, cosy_cosp);

        // Convert to degrees
        euler.x = euler.x * (180.0f / 3.14159265f);
        euler.y = euler.y * (180.0f / 3.14159265f);
        euler.z = euler.z * (180.0f / 3.14159265f);

        return euler;
    }

    Quaternion EulerToQuaternion(float rollX, float pitchY, float yawZ) {
        // Convert to radians
        float rx = rollX * (3.14159265f / 180.0f);
        float ry = pitchY * (3.14159265f / 180.0f);
        float rz = yawZ * (3.14159265f / 180.0f);

        float cx = std::cos(rx * 0.5f);
        float sx = std::sin(rx * 0.5f);
        float cy = std::cos(ry * 0.5f);
        float sy = std::sin(ry * 0.5f);
        float cz = std::cos(rz * 0.5f);
        float sz = std::sin(rz * 0.5f);

        Quaternion q;
        q.x = sx * cy * cz + cx * sy * sz;
        q.y = cx * sy * cz - sx * cy * sz;
        q.z = cx * cy * sz + sx * sy * cz;
        q.w = cx * cy * cz - sx * sy * sz;

        return NormalizeQuaternion(q);
    }

    bool IsFiniteVec3(const Vector3& v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    }

    Quaternion NormalizeQuaternion(const Quaternion& q) {
        float lenSq = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
        if (lenSq < 0.00001f) {
            return Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
        }
        float len = std::sqrt(lenSq);
        return Quaternion(q.x / len, q.y / len, q.z / len, q.w / len);
    }

} // namespace eng::runtime
