//============================================================================
// Transform.cpp - Implementation (UPDATED with ComputeWorldMatrix)
//
// Transform component implementation with matrix-based hierarchy
// Implements ComputeWorldMatrix algorithm
//
// Created: November 25, 2025
//============================================================================

#include "Transform.h"

//============================================================================
// CONSTRUCTION
//============================================================================

Transform::Transform()
    : localPosition_(0.0f, 0.0f, 0.0f)
    , localRotation_(0.0f, 0.0f, 0.0f, 1.0f)  // Identity quaternion
    , localScale_(1.0f, 1.0f, 1.0f)
    , worldPosition_(0.0f, 0.0f, 0.0f)
    , worldRotation_(0.0f, 0.0f, 0.0f, 1.0f)
    , worldScale_(1.0f, 1.0f, 1.0f)
    , localMatrixDirty_(true)
    , worldTransformDirty_(true)
{
    localMatrix_.SetIdentity();
    worldMatrix_.SetIdentity();
}

//============================================================================
// LOCAL SPACE SETTERS
//============================================================================

void Transform::SetPosition(const Vector3& pos) {
    localPosition_ = pos;
    localMatrixDirty_ = true;
    worldTransformDirty_ = true;
}

void Transform::SetRotation(const Quaternion& rot) {
    localRotation_ = rot;
    localMatrixDirty_ = true;
    worldTransformDirty_ = true;
}

void Transform::SetScale(const Vector3& scale) {
    localScale_ = scale;
    localMatrixDirty_ = true;
    worldTransformDirty_ = true;
}

//============================================================================
// LOCAL SPACE GETTERS
//============================================================================

const Vector3& Transform::GetPosition() const {
    return localPosition_;
}

const Quaternion& Transform::GetRotation() const {
    return localRotation_;
}

const Vector3& Transform::GetScale() const {
    return localScale_;
}

//============================================================================
// WORLD SPACE GETTERS
//============================================================================

const Vector3& Transform::GetWorldPosition() const {
    return worldPosition_;
}

const Quaternion& Transform::GetWorldRotation() const {
    return worldRotation_;
}

const Vector3& Transform::GetWorldScale() const {
    return worldScale_;
}

//============================================================================
// MATRIX ACCESS
//============================================================================

const Matrix4x4& Transform::GetLocalMatrix() const {
    return localMatrix_;
}

const Matrix4x4& Transform::GetWorldMatrix() const {
    return worldMatrix_;
}

//============================================================================
// CORE ALGORITHM: ComputeWorldMatrix
//============================================================================

/**
 * @brief ComputeWorldMatrix - Main transform algorithm
 *
 * Algorithm (from specification):
 * - If no parent:
 *     worldMatrix = localMatrix
 * - Else:
 *     worldMatrix = parent.worldMatrix × localMatrix
 * - Propagate to children (handled by SceneObject)
 */
void Transform::ComputeWorldMatrix(Transform* parentTransform) {
    // Step 1: Update local matrix if dirty
    if (localMatrixDirty_) {
        UpdateLocalMatrix();
    }

    // Step 2: Compute world matrix
    if (parentTransform == nullptr) {
        // NO PARENT - Root object
        // worldMatrix = localMatrix
        worldMatrix_ = localMatrix_;

    } else {
        // HAS PARENT
        // worldMatrix = parent.worldMatrix × localMatrix
        worldMatrix_ = parentTransform->GetWorldMatrix() * localMatrix_;
    }

    // Step 3: Extract world position/rotation/scale from matrix
    worldPosition_ = worldMatrix_.GetPosition();
    worldScale_ = worldMatrix_.GetScale();
    // worldRotation_ extraction would require matrix-to-quaternion conversion
    worldRotation_ = localRotation_;  // Simplified for now

    worldTransformDirty_ = false;

    // Note: Propagation to children is handled by SceneObject::UpdateChildren()
}

/**
 * @brief UpdateWorldTransform - Legacy method that calls ComputeWorldMatrix
 */
void Transform::UpdateWorldTransform(Transform* parentTransform) {
    ComputeWorldMatrix(parentTransform);
}

//============================================================================
// HELPER METHODS
//============================================================================

void Transform::Translate(const Vector3& offset) {
    localPosition_ = localPosition_ + offset;
    localMatrixDirty_ = true;
    worldTransformDirty_ = true;
}

void Transform::Rotate(const Quaternion& rotation) {
    // TODO: Proper quaternion multiplication
    // localRotation_ = localRotation_ * rotation;
    localRotation_ = rotation;
    localMatrixDirty_ = true;
    worldTransformDirty_ = true;
}

void Transform::ScaleBy(const Vector3& scaleFactor) {
    localScale_ = Vector3(
        localScale_.x * scaleFactor.x,
        localScale_.y * scaleFactor.y,
        localScale_.z * scaleFactor.z
    );
    localMatrixDirty_ = true;
    worldTransformDirty_ = true;
}

/**
 * @brief UpdateLocalMatrix - Recompute local TRS matrix
 *
 * Creates matrix from position, rotation, and scale:
 * localMatrix = Translation × Rotation × Scale
 */
void Transform::UpdateLocalMatrix() {
    // Create TRS matrix
    localMatrix_ = Matrix4x4::TRS(localPosition_, localRotation_, localScale_);

    localMatrixDirty_ = false;
}

//============================================================================
// END OF FILE
//===============================================================