#pragma once
#include <cmath>
#include <algorithm>

namespace eng::math {

    struct Vector3 {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        constexpr Vector3() noexcept : x(0.0f), y(0.0f), z(0.0f) {}
        constexpr Vector3(float xx, float yy, float zz) noexcept : x(xx), y(yy), z(zz) {}

        // Basic operators
        constexpr Vector3 operator+(const Vector3& o) const noexcept {
            return Vector3(x + o.x, y + o.y, z + o.z);
        }

        constexpr Vector3 operator-(const Vector3& o) const noexcept {
            return Vector3(x - o.x, y - o.y, z - o.z);
        }

        constexpr Vector3 operator*(float s) const noexcept {
            return Vector3(x * s, y * s, z * s);
        }

        constexpr Vector3 operator/(float s) const noexcept {
            return Vector3(x / s, y / s, z / s);
        }

        constexpr Vector3 operator*(const Vector3& o) const noexcept {
            return Vector3(x * o.x, y * o.y, z * o.z);
        }

        Vector3 operator-() const noexcept {
            return Vector3(-x, -y, -z);
        }

        Vector3& operator+=(const Vector3& o) noexcept {
            x += o.x; y += o.y; z += o.z;
            return *this;
        }

        Vector3& operator-=(const Vector3& o) noexcept {
            x -= o.x; y -= o.y; z -= o.z;
            return *this;
        }

        Vector3& operator*=(float s) noexcept {
            x *= s; y *= s; z *= s;
            return *this;
        }

        Vector3& operator/=(float s) noexcept {
            x /= s; y /= s; z /= s;
            return *this;
        }

        constexpr bool operator==(const Vector3& o) const noexcept {
            return x == o.x && y == o.y && z == o.z;
        }

        constexpr bool operator!=(const Vector3& o) const noexcept {
            return x != o.x || y != o.y || z != o.z;
        }

        // Methods
        [[nodiscard]] float LengthSquared() const noexcept {
            return x * x + y * y + z * z;
        }

        [[nodiscard]] float Length() const noexcept {
            return std::sqrt(LengthSquared());
        }

        void Normalize() noexcept {
            float len = Length();
            if (len > 0.0f) {
                float inv = 1.0f / len;
                x *= inv; y *= inv; z *= inv;
            }
        }

        [[nodiscard]] Vector3 Normalized() const noexcept {
            Vector3 v = *this;
            v.Normalize();
            return v;
        }

        [[nodiscard]] constexpr float Dot(const Vector3& o) const noexcept {
            return x * o.x + y * o.y + z * o.z;
        }

        [[nodiscard]] constexpr Vector3 Cross(const Vector3& o) const noexcept {
            return Vector3(
                y * o.z - z * o.y,
                z * o.x - x * o.z,
                x * o.y - y * o.x
            );
        }

        [[nodiscard]] float Distance(const Vector3& o) const noexcept {
            return (*this - o).Length();
        }

        [[nodiscard]] static constexpr float Dot(const Vector3& a, const Vector3& b) noexcept {
            return a.Dot(b);
        }

        [[nodiscard]] static constexpr Vector3 Cross(const Vector3& a, const Vector3& b) noexcept {
            return a.Cross(b);
        }

        [[nodiscard]] static float Distance(const Vector3& a, const Vector3& b) noexcept {
            return a.Distance(b);
        }

        [[nodiscard]] static Vector3 Lerp(const Vector3& a, const Vector3& b, float t) noexcept {
            return a + (b - a) * t;
        }
    };

    inline constexpr Vector3 operator*(float s, const Vector3& v) noexcept {
        return v * s;
    }

} // namespace eng::math

// Export to global namespace for backward compatibility
using Vector3 = eng::math::Vector3;
