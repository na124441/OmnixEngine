//============================================================================
// Transform.h - Transform Component (UPDATED with ComputeWorldMatrix)
//
// Handles position, rotation, and scale in 3D space
// Supports local and world space transformations with matrix math
//
// Created: November 25, 2025
//============================================================================

#pragma once

#include "Scene/Vector3.h"
#include "Scene/Quaternion.h"
#include "Scene/Matrix4x4.h"

/**
 * @brief Transform - Position, rotation, and scale component
 *
 * Manages local and world-space transformations.
 * Supports parent-child transform hierarchy with matrix math.
 */
class Transform {
public:
    //========================================================================
    // CONSTRUCTION
    //========================================================================

    /**
     * @brief Constructor - Initialize to identity transform
     */
    Transform();

    //========================================================================
    // LOCAL SPACE SETTERS
    //========================================================================

    /**
     * @brief Set local position
     * @param pos Position vector
     */
    void SetPosition(const Vector3& pos);

    /**
     * @brief Set local rotation
     * @param rot Rotation quaternion
     */
    void SetRotation(const Quaternion& rot);

    /**
     * @brief Set local scale
     * @param scale Scale vector
     */
    void SetScale(const Vector3& scale);

    //========================================================================
    // LOCAL SPACE GETTERS
    //========================================================================

    /**
     * @brief Get local position
     * @return Position vector
     */
    const Vector3& GetPosition() const;

    /**
     * @brief Get local rotation
     * @return Rotation quaternion
     */
    const Quaternion& GetRotation() const;

    /**
     * @brief Get local scale
     * @return Scale vector
     */
    const Vector3& GetScale() const;

    //========================================================================
    // WORLD SPACE GETTERS
    //========================================================================

    /**
     * @brief Get world position
     * @return World position vector
     */
    const Vector3& GetWorldPosition() const;

    /**
     * @brief Get world rotation
     * @return World rotation quaternion
     */
    const Quaternion& GetWorldRotation() const;

    /**
     * @brief Get world scale
     * @return World scale vector
     */
    const Vector3& GetWorldScale() const;

    //========================================================================
    // MATRIX ACCESS
    //========================================================================

    /**
     * @brief Get local transformation matrix
     * @return Local TRS matrix
     */
    const Matrix4x4& GetLocalMatrix() const;

    /**
     * @brief Get world transformation matrix
     * @return World transform matrix
     */
    const Matrix4x4& GetWorldMatrix() const;

    //========================================================================
    // CORE ALGORITHM: ComputeWorldMatrix
    //========================================================================

    /**
     * @brief ComputeWorldMatrix - Main transform algorithm
     *
     * Algorithm (from specification):
     * - If no parent:
     *     worldMatrix = localMatrix
     * - Else:
     *     worldMatrix = parent.worldMatrix × localMatrix
     * - Propagate to children
     *
     * @param parentTransform Parent's transform (nullptr if root)
     */
    void ComputeWorldMatrix(Transform* parentTransform);

    /**
     * @brief Update world transform from parent (legacy method)
     * Calls ComputeWorldMatrix internally
     * @param parentTransform Parent's transform (nullptr if root)
     */
    void UpdateWorldTransform(Transform* parentTransform);

    //========================================================================
    // HELPER METHODS
    //========================================================================

    /**
     * @brief Translate by offset
     * @param offset Translation offset
     */
    void Translate(const Vector3& offset);

    /**
     * @brief Rotate by quaternion
     * @param rotation Rotation to apply
     */
    void Rotate(const Quaternion& rotation);

    /**
     * @brief Scale by factor
     * @param scaleFactor Scale multiplier
     */
    void ScaleBy(const Vector3& scaleFactor);

    /**
     * @brief Recompute local matrix from position/rotation/scale
     */
    void UpdateLocalMatrix();

private:
    //========================================================================
    // LOCAL SPACE (relative to parent)
    //========================================================================

    Vector3 localPosition_;
    Quaternion localRotation_;
    Vector3 localScale_;
    Matrix4x4 localMatrix_;       // Cached local TRS matrix

    //========================================================================
    // WORLD SPACE (absolute in scene)
    //========================================================================

    Vector3 worldPosition_;
    Quaternion worldRotation_;
    Vector3 worldScale_;
    Matrix4x4 worldMatrix_;       // Cached world transform matrix

    //========================================================================
    // CACHE FLAGS
    //========================================================================

    bool localMatrixDirty_;       // True if local matrix needs recalculation
    bool worldTransformDirty_;    // True if world transform needs recalculation
};

//============================================================================
// END OF FILE
//===============================================================