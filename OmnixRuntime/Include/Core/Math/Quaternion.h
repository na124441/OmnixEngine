#pragma once
#include "Core/Math/Vector3.h"
#include <cmath>
#include <algorithm>
#include <cassert>

namespace eng::math {

    struct Quaternion {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 1.0f;

        constexpr Quaternion() noexcept : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}
        constexpr Quaternion(float xx, float yy, float zz, float ww) noexcept
            : x(xx), y(yy), z(zz), w(ww) {}

        // Construction helpers
        static Quaternion FromAxisAngle(const Vector3& axis, float angleRadians) noexcept {
            float halfAngle = angleRadians * 0.5f;
            float sinHalf = std::sin(halfAngle);
            Vector3 normAxis = axis.Normalized();
            return Quaternion(
                normAxis.x * sinHalf,
                normAxis.y * sinHalf,
                normAxis.z * sinHalf,
                std::cos(halfAngle)
            );
        }

        static Quaternion FromEuler(float pitchRad, float yawRad, float rollRad) noexcept {
            // Pitch (X), Yaw (Y), Roll (Z) rotation ordering
            float c1 = std::cos(pitchRad * 0.5f);
            float s1 = std::sin(pitchRad * 0.5f);
            float c2 = std::cos(yawRad * 0.5f);
            float s2 = std::sin(yawRad * 0.5f);
            float c3 = std::cos(rollRad * 0.5f);
            float s3 = std::sin(rollRad * 0.5f);

            return Quaternion(
                s1 * c2 * c3 + c1 * s2 * s3,
                c1 * s2 * c3 - s1 * c2 * s3,
                c1 * c2 * s3 + s1 * s2 * c3,
                c1 * c2 * c3 - s1 * s2 * s3
            );
        }

        // Basic operators
        Quaternion operator*(const Quaternion& o) const noexcept {
            return Quaternion(
                w * o.x + x * o.w + y * o.z - z * o.y,
                w * o.y - x * o.z + y * o.w + z * o.x,
                w * o.z + x * o.y - y * o.x + z * o.w,
                w * o.w - x * o.x - y * o.y - z * o.z
            );
        }

        Vector3 operator*(const Vector3& v) const noexcept {
            Vector3 qv(x, y, z);
            Vector3 uv = qv.Cross(v);
            Vector3 uuv = qv.Cross(uv);
            return v + ((uv * w) + uuv) * 2.0f;
        }

        constexpr bool operator==(const Quaternion& o) const noexcept {
            return x == o.x && y == o.y && z == o.z && w == o.w;
        }

        constexpr bool operator!=(const Quaternion& o) const noexcept {
            return x != o.x || y != o.y || z != o.z || w != o.w;
        }

        // Methods
        [[nodiscard]] float LengthSquared() const noexcept {
            return x * x + y * y + z * z + w * w;
        }

        [[nodiscard]] float Length() const noexcept {
            return std::sqrt(LengthSquared());
        }

        void Normalize() noexcept {
            float len = Length();
            if (len > 0.0f) {
                float inv = 1.0f / len;
                x *= inv; y *= inv; z *= inv; w *= inv;
            }
        }

        [[nodiscard]] Quaternion Normalized() const noexcept {
            Quaternion q = *this;
            q.Normalize();
            return q;
        }

        [[nodiscard]] Quaternion Conjugate() const noexcept {
            return Quaternion(-x, -y, -z, w);
        }

        [[nodiscard]] Quaternion Inverse() const noexcept {
            float lenSq = LengthSquared();
            if (lenSq > 0.0f) {
                float inv = 1.0f / lenSq;
                return Quaternion(-x * inv, -y * inv, -z * inv, w * inv);
            }
            return Quaternion();
        }

        [[nodiscard]] Vector3 ToEuler() const noexcept {
            // Conversion to Roll, Pitch, Yaw euler angles
            Vector3 euler;
            
            // pitch (x-axis rotation)
            float sinr_cosp = 2.0f * (w * x + y * z);
            float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
            euler.x = std::atan2(sinr_cosp, cosr_cosp);

            // yaw (y-axis rotation)
            float sinp = 2.0f * (w * y - z * x);
            if (std::abs(sinp) >= 1.0f)
                euler.y = std::copysign(3.1415926535f / 2.0f, sinp); // use 90 degrees if out of range
            else
                euler.y = std::asin(sinp);

            // roll (z-axis rotation)
            float siny_cosp = 2.0f * (w * z + x * y);
            float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
            euler.z = std::atan2(siny_cosp, cosy_cosp);

            return euler;
        }

        [[nodiscard]] static Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t) noexcept {
            Quaternion q1 = a.Normalized();
            Quaternion q2 = b.Normalized();

            float dot = q1.x * q2.x + q1.y * q2.y + q1.z * q2.z + q1.w * q2.w;

            // If the dot product is negative, slerp won't take the shorter path.
            // Fix by reversing one quaternion.
            if (dot < 0.0f) {
                q2 = Quaternion(-q2.x, -q2.y, -q2.z, -q2.w);
                dot = -dot;
            }

            constexpr float DOT_THRESHOLD = 0.9995f;
            if (dot > DOT_THRESHOLD) {
                // If inputs are too close, linearly interpolate (LERP) and normalize
                Quaternion result(
                    q1.x + (q2.x - q1.x) * t,
                    q1.y + (q2.y - q1.y) * t,
                    q1.z + (q2.z - q1.z) * t,
                    q1.w + (q2.w - q1.w) * t
                );
                return result.Normalized();
            }

            float theta_0 = std::acos(dot);
            float theta = theta_0 * t;
            float sin_theta = std::sin(theta);
            float sin_theta_0 = std::sin(theta_0);

            float s0 = std::cos(theta) - dot * sin_theta / sin_theta_0;
            float s1 = sin_theta / sin_theta_0;

            return Quaternion(
                s0 * q1.x + s1 * q2.x,
                s0 * q1.y + s1 * q2.y,
                s0 * q1.z + s1 * q2.z,
                s0 * q1.w + s1 * q2.w
            );
        }
    };

} // namespace eng::math

// Export to global namespace for backward compatibility
using Quaternion = eng::math::Quaternion;
