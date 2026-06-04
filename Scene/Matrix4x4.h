//============================================================================
// Matrix4x4.h - 4x4 Matrix for Transform Math
//
// Handles matrix operations for transform hierarchies
// Used by Transform::ComputeWorldMatrix()
//
// Created: November 25, 2025
//============================================================================

#pragma once
#include <cmath>
#include "Vector3.h"
#include "Quaternion.h"

/**
 * @brief Matrix4x4 - 4x4 transformation matrix
 *
 * Column-major layout (like OpenGL):
 * [m00 m04 m08 m12]
 * [m01 m05 m09 m13]
 * [m02 m06 m10 m14]
 * [m03 m07 m11 m15]
 */
struct Matrix4x4 {
    float m[16];  // Column-major storage

    //========================================================================
    // CONSTRUCTION
    //========================================================================

    /**
     * @brief Default constructor - creates identity matrix
     */
    Matrix4x4() {
        SetIdentity();
    }

    /**
     * @brief Set to identity matrix
     */
    void SetIdentity() {
        m[0]  = 1.0f; m[4]  = 0.0f; m[8]  = 0.0f; m[12] = 0.0f;
        m[1]  = 0.0f; m[5]  = 1.0f; m[9]  = 0.0f; m[13] = 0.0f;
        m[2]  = 0.0f; m[6]  = 0.0f; m[10] = 1.0f; m[14] = 0.0f;
        m[3]  = 0.0f; m[7]  = 0.0f; m[11] = 0.0f; m[15] = 1.0f;
    }

    //========================================================================
    // MATRIX OPERATIONS
    //========================================================================

    /**
     * @brief Matrix multiplication: result = this * other
     * @param other Matrix to multiply with
     * @return Result matrix
     */
    Matrix4x4 operator*(const Matrix4x4& other) const {
        Matrix4x4 result;

        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                result.m[col * 4 + row] =
                    m[0 * 4 + row] * other.m[col * 4 + 0] +
                    m[1 * 4 + row] * other.m[col * 4 + 1] +
                    m[2 * 4 + row] * other.m[col * 4 + 2] +
                    m[3 * 4 + row] * other.m[col * 4 + 3];
            }
        }

        return result;
    }

    /**
     * @brief Assignment operator
     */
    Matrix4x4& operator=(const Matrix4x4& other) {
        for (int i = 0; i < 16; ++i) {
            m[i] = other.m[i];
        }
        return *this;
    }

    //========================================================================
    // TRANSFORM CREATION
    //========================================================================

    /**
     * @brief Create translation matrix
     * @param position Translation vector
     * @return Translation matrix
     */
    static Matrix4x4 Translation(const Vector3& position) {
        Matrix4x4 mat;
        mat.m[12] = position.x;
        mat.m[13] = position.y;
        mat.m[14] = position.z;
        return mat;
    }

    /**
     * @brief Create scale matrix
     * @param scale Scale vector
     * @return Scale matrix
     */
    static Matrix4x4 Scale(const Vector3& scale) {
        Matrix4x4 mat;
        mat.m[0]  = scale.x;
        mat.m[5]  = scale.y;
        mat.m[10] = scale.z;
        return mat;
    }

    /**
     * @brief Create rotation matrix from quaternion (simplified)
     * @param rotation Quaternion rotation
     * @return Rotation matrix
     */
    static Matrix4x4 Rotation(const Quaternion& rotation) {
        Matrix4x4 mat;

        // Proper quaternion-to-matrix conversion (column-major)
        mat.m[0]  = 1.0f - 2.0f * (rotation.y * rotation.y + rotation.z * rotation.z);
        mat.m[1]  = 2.0f * (rotation.x * rotation.y + rotation.z * rotation.w);
        mat.m[2]  = 2.0f * (rotation.x * rotation.z - rotation.y * rotation.w);
        mat.m[3]  = 0.0f;

        mat.m[4]  = 2.0f * (rotation.x * rotation.y - rotation.z * rotation.w);
        mat.m[5]  = 1.0f - 2.0f * (rotation.x * rotation.x + rotation.z * rotation.z);
        mat.m[6]  = 2.0f * (rotation.y * rotation.z + rotation.x * rotation.w);
        mat.m[7]  = 0.0f;

        mat.m[8]  = 2.0f * (rotation.x * rotation.z + rotation.y * rotation.w);
        mat.m[9]  = 2.0f * (rotation.y * rotation.z - rotation.x * rotation.w);
        mat.m[10] = 1.0f - 2.0f * (rotation.x * rotation.x + rotation.y * rotation.y);
        mat.m[11] = 0.0f;

        mat.m[12] = 0.0f;
        mat.m[13] = 0.0f;
        mat.m[14] = 0.0f;
        mat.m[15] = 1.0f;

        return mat;
    }

    /**
     * @brief Create TRS matrix (Translation * Rotation * Scale)
     * @param position Translation
     * @param rotation Rotation
     * @param scale Scale
     * @return Combined TRS matrix
     */
    static Matrix4x4 TRS(const Vector3& position, const Quaternion& rotation, const Vector3& scale) {
        Matrix4x4 t = Translation(position);
        Matrix4x4 r = Rotation(rotation);
        Matrix4x4 s = Scale(scale);

        // Combine: T * R * S
        return t * r * s;
    }

    //========================================================================
    // EXTRACTION
    //========================================================================

    /**
     * @brief Extract position from matrix
     * @return Position vector
     */
    Vector3 GetPosition() const {
        return Vector3(m[12], m[13], m[14]);
    }

    /**
     * @brief Extract scale from matrix (approximate)
     * @return Scale vector
     */
    Vector3 GetScale() const {
        // Length of basis vectors
        float sx = sqrtf(m[0]*m[0] + m[1]*m[1] + m[2]*m[2]);
        float sy = sqrtf(m[4]*m[4] + m[5]*m[5] + m[6]*m[6]);
        float sz = sqrtf(m[8]*m[8] + m[9]*m[9] + m[10]*m[10]);
        return Vector3(sx, sy, sz);
    }
};

//============================================================================
// END OF FILE
//===============================================================// Created by nayan on 11/25/2025.
//

#ifndef OMNIXENGINE_MATRIX4X4_H
#define OMNIXENGINE_MATRIX4X4_H

#endif //OMNIXENGINE_MATRIX4X4_H