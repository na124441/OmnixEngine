#pragma once
#include "Core/Math/Vector3.h"
#include "Core/Math/Quaternion.h"
#include <cmath>
#include <algorithm>
#include <cassert>

namespace eng::math {

    struct Matrix4x4 {
        float m[16]; // Column-major storage

        Matrix4x4() noexcept {
            SetIdentity();
        }

        void SetIdentity() noexcept {
            m[0]  = 1.0f; m[4]  = 0.0f; m[8]  = 0.0f; m[12] = 0.0f;
            m[1]  = 0.0f; m[5]  = 1.0f; m[9]  = 0.0f; m[13] = 0.0f;
            m[2]  = 0.0f; m[6]  = 0.0f; m[10] = 1.0f; m[14] = 0.0f;
            m[3]  = 0.0f; m[7]  = 0.0f; m[11] = 0.0f; m[15] = 1.0f;
        }

        Matrix4x4 operator*(const Matrix4x4& o) const noexcept {
            Matrix4x4 result;
            for (int col = 0; col < 4; ++col) {
                for (int row = 0; row < 4; ++row) {
                    result.m[col * 4 + row] =
                        m[0 * 4 + row] * o.m[col * 4 + 0] +
                        m[1 * 4 + row] * o.m[col * 4 + 1] +
                        m[2 * 4 + row] * o.m[col * 4 + 2] +
                        m[3 * 4 + row] * o.m[col * 4 + 3];
                }
            }
            return result;
        }

        Vector3 TransformPoint(const Vector3& p) const noexcept {
            float x = m[0]*p.x + m[4]*p.y + m[8] *p.z + m[12];
            float y = m[1]*p.x + m[5]*p.y + m[9] *p.z + m[13];
            float z = m[2]*p.x + m[6]*p.y + m[10]*p.z + m[14];
            float w = m[3]*p.x + m[7]*p.y + m[11]*p.z + m[15];
            if (w != 0.0f && w != 1.0f) { 
                float invW = 1.0f / w;
                x *= invW; y *= invW; z *= invW; 
            }
            return Vector3(x, y, z);
        }

        Matrix4x4& operator=(const Matrix4x4& o) noexcept {
            if (this != &o) {
                std::copy(std::begin(o.m), std::end(o.m), std::begin(m));
            }
            return *this;
        }

        // Static transform generators
        static Matrix4x4 Translation(const Vector3& position) noexcept {
            Matrix4x4 mat;
            mat.m[12] = position.x;
            mat.m[13] = position.y;
            mat.m[14] = position.z;
            return mat;
        }

        static Matrix4x4 Scale(const Vector3& scale) noexcept {
            Matrix4x4 mat;
            mat.m[0]  = scale.x;
            mat.m[5]  = scale.y;
            mat.m[10] = scale.z;
            return mat;
        }

        static Matrix4x4 Rotation(const Quaternion& r) noexcept {
            Matrix4x4 mat;
            mat.m[0]  = 1.0f - 2.0f * (r.y * r.y + r.z * r.z);
            mat.m[1]  = 2.0f * (r.x * r.y + r.z * r.w);
            mat.m[2]  = 2.0f * (r.x * r.z - r.y * r.w);
            mat.m[3]  = 0.0f;

            mat.m[4]  = 2.0f * (r.x * r.y - r.z * r.w);
            mat.m[5]  = 1.0f - 2.0f * (r.x * r.x + r.z * r.z);
            mat.m[6]  = 2.0f * (r.y * r.z + r.x * r.w);
            mat.m[7]  = 0.0f;

            mat.m[8]  = 2.0f * (r.x * r.z + r.y * r.w);
            mat.m[9]  = 2.0f * (r.y * r.z - r.x * r.w);
            mat.m[10] = 1.0f - 2.0f * (r.x * r.x + r.y * r.y);
            mat.m[11] = 0.0f;

            mat.m[12] = 0.0f;
            mat.m[13] = 0.0f;
            mat.m[14] = 0.0f;
            mat.m[15] = 1.0f;
            return mat;
        }

        static Matrix4x4 TRS(const Vector3& position, const Quaternion& rotation, const Vector3& scale) noexcept {
            return Translation(position) * Rotation(rotation) * Scale(scale);
        }

        // Projection and view matrices
        static Matrix4x4 Perspective(float fovYRadians, float aspect, float zNear, float zFar) noexcept {
            Matrix4x4 mat;
            for (int i = 0; i < 16; ++i) mat.m[i] = 0.0f;
            float tanHalfFovy = std::tan(fovYRadians * 0.5f);
            mat.m[0] = 1.0f / (aspect * tanHalfFovy);
            mat.m[5] = 1.0f / tanHalfFovy;
            mat.m[10] = -(zFar + zNear) / (zFar - zNear);
            mat.m[11] = -1.0f;
            mat.m[14] = -(2.0f * zFar * zNear) / (zFar - zNear);
            return mat;
        }

        static Matrix4x4 Orthographic(float left, float right, float bottom, float top, float zNear, float zFar) noexcept {
            Matrix4x4 mat;
            mat.m[0] = 2.0f / (right - left);
            mat.m[5] = 2.0f / (top - bottom);
            mat.m[10] = -2.0f / (zFar - zNear);
            mat.m[12] = -(right + left) / (right - left);
            mat.m[13] = -(top + bottom) / (top - bottom);
            mat.m[14] = -(zFar + zNear) / (zFar - zNear);
            mat.m[15] = 1.0f;
            return mat;
        }

        static Matrix4x4 LookAt(const Vector3& eye, const Vector3& center, const Vector3& up) noexcept {
            Vector3 f = (center - eye).Normalized();
            Vector3 s = f.Cross(up).Normalized();
            Vector3 u = s.Cross(f);

            Matrix4x4 mat;
            mat.m[0] = s.x;
            mat.m[4] = s.y;
            mat.m[8] = s.z;
            mat.m[12] = -s.Dot(eye);

            mat.m[1] = u.x;
            mat.m[5] = u.y;
            mat.m[9] = u.z;
            mat.m[13] = -u.Dot(eye);

            mat.m[2] = -f.x;
            mat.m[6] = -f.y;
            mat.m[10] = -f.z;
            mat.m[14] = f.Dot(eye);

            mat.m[3] = 0.0f;
            mat.m[7] = 0.0f;
            mat.m[11] = 0.0f;
            mat.m[15] = 1.0f;
            return mat;
        }

        // Algebraic transformations
        void Transpose() noexcept {
            std::swap(m[1], m[4]);
            std::swap(m[2], m[8]);
            std::swap(m[3], m[12]);
            std::swap(m[6], m[9]);
            std::swap(m[7], m[13]);
            std::swap(m[11], m[14]);
        }

        [[nodiscard]] Matrix4x4 Transposed() const noexcept {
            Matrix4x4 mat = *this;
            mat.Transpose();
            return mat;
        }

        bool Inverse() noexcept {
            float inv[16];
            float det;

            inv[0] = m[5]  * m[10] * m[15] - 
                     m[5]  * m[11] * m[14] - 
                     m[9]  * m[6]  * m[15] + 
                     m[9]  * m[7]  * m[14] +
                     m[13] * m[6]  * m[11] - 
                     m[13] * m[7]  * m[10];

            inv[4] = -m[4]  * m[10] * m[15] + 
                      m[4]  * m[11] * m[14] + 
                      m[8]  * m[6]  * m[15] - 
                      m[8]  * m[7]  * m[14] - 
                      m[12] * m[6]  * m[11] + 
                      m[12] * m[7]  * m[10];

            inv[8] = m[4]  * m[9] * m[15] - 
                     m[4]  * m[11] * m[13] - 
                     m[8]  * m[5]  * m[15] + 
                     m[8]  * m[7]  * m[13] + 
                     m[12] * m[5]  * m[11] - 
                     m[12] * m[7]  * m[9];

            inv[12] = -m[4]  * m[9] * m[14] + 
                       m[4]  * m[10] * m[13] + 
                       m[8]  * m[5]  * m[14] - 
                       m[8]  * m[6]  * m[13] - 
                       m[12] * m[5]  * m[10] + 
                       m[12] * m[6]  * m[9];

            inv[1] = -m[1]  * m[10] * m[15] + 
                      m[1]  * m[11] * m[14] + 
                      m[9]  * m[2]  * m[15] - 
                      m[9]  * m[3]  * m[14] - 
                      m[13] * m[2]  * m[11] + 
                      m[13] * m[3]  * m[10];

            inv[5] = m[0]  * m[10] * m[15] - 
                     m[0]  * m[11] * m[14] - 
                     m[8]  * m[2]  * m[15] + 
                     m[8]  * m[3]  * m[14] + 
                     m[12] * m[2]  * m[11] - 
                     m[12] * m[3]  * m[10];

            inv[9] = -m[0]  * m[9] * m[15] + 
                      m[0]  * m[11] * m[13] + 
                      m[8]  * m[1]  * m[15] - 
                      m[8]  * m[3]  * m[13] - 
                      m[12] * m[1]  * m[11] + 
                      m[12] * m[3]  * m[9];

            inv[13] = m[0]  * m[9] * m[14] - 
                      m[0]  * m[10] * m[13] - 
                      m[8]  * m[1]  * m[14] + 
                      m[8]  * m[2]  * m[13] + 
                      m[12] * m[1]  * m[10] - 
                      m[12] * m[2]  * m[9];

            inv[2] = m[1]  * m[6] * m[15] - 
                     m[1]  * m[7] * m[14] - 
                     m[5]  * m[2] * m[15] + 
                     m[5]  * m[3] * m[14] + 
                     m[13] * m[2] * m[7] - 
                     m[13] * m[3] * m[6];

            inv[6] = -m[0]  * m[6] * m[15] + 
                      m[0]  * m[7] * m[14] + 
                      m[4]  * m[2] * m[15] - 
                      m[4]  * m[3] * m[14] - 
                      m[12] * m[2] * m[7] + 
                      m[12] * m[3] * m[6];

            inv[10] = m[0]  * m[5] * m[15] - 
                      m[0]  * m[7] * m[13] - 
                      m[4]  * m[1] * m[15] + 
                      m[4]  * m[3] * m[13] + 
                      m[12] * m[1] * m[7] - 
                      m[12] * m[3] * m[5];

            inv[14] = -m[0]  * m[5] * m[14] + 
                       m[0]  * m[6] * m[13] + 
                       m[4]  * m[1] * m[14] - 
                       m[4]  * m[2] * m[13] - 
                       m[12] * m[1] * m[6] + 
                       m[12] * m[2] * m[5];

            inv[3] = -m[1] * m[6] * m[11] + 
                      m[1] * m[7] * m[10] + 
                      m[5] * m[2] * m[11] - 
                      m[5] * m[3] * m[10] - 
                      m[9] * m[2] * m[7] + 
                      m[9] * m[3] * m[6];

            inv[7] = m[0] * m[6] * m[11] - 
                     m[0] * m[7] * m[10] - 
                     m[4] * m[2] * m[11] + 
                     m[4] * m[3] * m[10] + 
                     m[8] * m[2] * m[7] - 
                     m[8] * m[3] * m[6];

            inv[11] = -m[0] * m[5] * m[11] + 
                       m[0] * m[7] * m[9] + 
                       m[4] * m[1] * m[11] - 
                       m[4] * m[3] * m[9] - 
                       m[8] * m[1] * m[7] + 
                       m[8] * m[3] * m[5];

            inv[15] = m[0] * m[5] * m[10] - 
                      m[0] * m[6] * m[9] - 
                      m[4] * m[1] * m[10] + 
                      m[4] * m[2] * m[9] + 
                      m[8] * m[1] * m[6] - 
                      m[8] * m[2] * m[5];

            det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

            if (det == 0.0f) {
                return false;
            }

            float invDet = 1.0f / det;
            for (int i = 0; i < 16; i++) {
                m[i] = inv[i] * invDet;
            }

            return true;
        }

        [[nodiscard]] Matrix4x4 Inversed() const noexcept {
            Matrix4x4 mat = *this;
            mat.Inverse();
            return mat;
        }

        [[nodiscard]] Vector3 GetPosition() const noexcept {
            return Vector3(m[12], m[13], m[14]);
        }

        [[nodiscard]] Vector3 GetScale() const noexcept {
            float sx = std::sqrt(m[0]*m[0] + m[1]*m[1] + m[2]*m[2]);
            float sy = std::sqrt(m[4]*m[4] + m[5]*m[5] + m[6]*m[6]);
            float sz = std::sqrt(m[8]*m[8] + m[9]*m[9] + m[10]*m[10]);
            return Vector3(sx, sy, sz);
        }
    };

} // namespace eng::math

// Export to global namespace for backward compatibility
using Matrix4x4 = eng::math::Matrix4x4;
