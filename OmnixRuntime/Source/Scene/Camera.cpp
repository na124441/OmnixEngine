//============================================================================
// Camera.cpp - Implementation
//
// Camera component implementation with view-projection computation
// Implements GetViewProjection algorithm
//
// Created: November 25, 2025
//============================================================================

#include "Scene/Camera.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//============================================================================
// CONSTRUCTION
//============================================================================

Camera::Camera()
    : projectionType_(ProjectionType::Perspective)
    , fieldOfView_(60.0f)
    , aspectRatio_(16.0f / 9.0f)
    , orthoLeft_(-10.0f)
    , orthoRight_(10.0f)
    , orthoBottom_(-10.0f)
    , orthoTop_(10.0f)
    , nearPlane_(0.1f)
    , farPlane_(1000.0f)
    , projectionDirty_(true)
{
    viewMatrix_.SetIdentity();
    projectionMatrix_.SetIdentity();
    viewProjectionMatrix_.SetIdentity();
}

//============================================================================
// CORE ALGORITHM: GetViewProjection
//============================================================================

/**
 * @brief GetViewProjection - Main camera algorithm
 *
 * Algorithm (from specification):
 * 1. Compute view matrix from world transform
 * 2. Compute projection (perspective or orthographic)
 * 3. Return view × projection
 */
Matrix4x4 Camera::GetViewProjection(const Matrix4x4& worldMatrix) {
    // STEP 1: Compute view matrix from world transform
    viewMatrix_ = ComputeViewMatrix(worldMatrix);

    // STEP 2: Compute projection (perspective or orthographic)
    if (projectionDirty_) {
        projectionMatrix_ = ComputeProjectionMatrix();
        projectionDirty_ = false;
    }

    // STEP 3: Return view × projection
    viewProjectionMatrix_ = projectionMatrix_ * viewMatrix_;

    return viewProjectionMatrix_;
}

//============================================================================
// MATRIX COMPUTATION
//============================================================================

/**
 * @brief ComputeViewMatrix - Compute view matrix from world transform
 *
 * View matrix is the inverse of the camera's world transform.
 * This transforms world space to camera space.
 */
Matrix4x4 Camera::ComputeViewMatrix(const Matrix4x4& worldMatrix) {
    // View matrix = inverse of world matrix
    // For now, simplified implementation
    // TODO: Implement proper matrix inverse

    // Extract position from world matrix
    Vector3 position = worldMatrix.GetPosition();

    // Create simple look-at style view matrix
    // Assumes camera looks down -Z axis
    Matrix4x4 view;
    view.SetIdentity();

    // Translate by negative camera position
    view.m[12] = -position.x;
    view.m[13] = -position.y;
    view.m[14] = -position.z;

    return view;
}

/**
 * @brief ComputeProjectionMatrix - Compute projection matrix
 */
Matrix4x4 Camera::ComputeProjectionMatrix() {
    if (projectionType_ == ProjectionType::Perspective) {
        return CreatePerspectiveMatrix(fieldOfView_, aspectRatio_, nearPlane_, farPlane_);
    } else {
        return CreateOrthographicMatrix(orthoLeft_, orthoRight_, orthoBottom_,
                                       orthoTop_, nearPlane_, farPlane_);
    }
}

Matrix4x4 Camera::GetViewMatrix(const Matrix4x4& worldMatrix) {
    return ComputeViewMatrix(worldMatrix);
}

Matrix4x4 Camera::GetProjectionMatrix() {
    if (projectionDirty_) {
        projectionMatrix_ = ComputeProjectionMatrix();
        projectionDirty_ = false;
    }
    return projectionMatrix_;
}

//============================================================================
// PERSPECTIVE PROJECTION
//============================================================================

/**
 * @brief Create perspective projection matrix
 *
 * Standard OpenGL-style perspective projection.
 */
Matrix4x4 Camera::CreatePerspectiveMatrix(float fov, float aspect, float near, float far) {
    Matrix4x4 mat;

    float tanHalfFov = tanf((fov * M_PI / 180.0f) / 2.0f);

    mat.m[0] = 1.0f / (aspect * tanHalfFov);
    mat.m[1] = 0.0f;
    mat.m[2] = 0.0f;
    mat.m[3] = 0.0f;

    mat.m[4] = 0.0f;
    mat.m[5] = 1.0f / tanHalfFov;
    mat.m[6] = 0.0f;
    mat.m[7] = 0.0f;

    mat.m[8] = 0.0f;
    mat.m[9] = 0.0f;
    mat.m[10] = -(far + near) / (far - near);
    mat.m[11] = -1.0f;

    mat.m[12] = 0.0f;
    mat.m[13] = 0.0f;
    mat.m[14] = -(2.0f * far * near) / (far - near);
    mat.m[15] = 0.0f;

    return mat;
}

void Camera::SetPerspective(float fov, float aspectRatio, float nearPlane, float farPlane) {
    fieldOfView_ = fov;
    aspectRatio_ = aspectRatio;
    nearPlane_ = nearPlane;
    farPlane_ = farPlane;
    projectionType_ = ProjectionType::Perspective;
    projectionDirty_ = true;
}

float Camera::GetFieldOfView() const {
    return fieldOfView_;
}

void Camera::SetFieldOfView(float fov) {
    fieldOfView_ = fov;
    projectionDirty_ = true;
}

//============================================================================
// ORTHOGRAPHIC PROJECTION
//============================================================================

/**
 * @brief Create orthographic projection matrix
 *
 * Standard OpenGL-style orthographic projection.
 */
Matrix4x4 Camera::CreateOrthographicMatrix(float left, float right, float bottom,
                                          float top, float near, float far) {
    Matrix4x4 mat;

    mat.m[0] = 2.0f / (right - left);
    mat.m[1] = 0.0f;
    mat.m[2] = 0.0f;
    mat.m[3] = 0.0f;

    mat.m[4] = 0.0f;
    mat.m[5] = 2.0f / (top - bottom);
    mat.m[6] = 0.0f;
    mat.m[7] = 0.0f;

    mat.m[8] = 0.0f;
    mat.m[9] = 0.0f;
    mat.m[10] = -2.0f / (far - near);
    mat.m[11] = 0.0f;

    mat.m[12] = -(right + left) / (right - left);
    mat.m[13] = -(top + bottom) / (top - bottom);
    mat.m[14] = -(far + near) / (far - near);
    mat.m[15] = 1.0f;

    return mat;
}

void Camera::SetOrthographic(float left, float right, float bottom, float top,
                            float nearPlane, float farPlane) {
    orthoLeft_ = left;
    orthoRight_ = right;
    orthoBottom_ = bottom;
    orthoTop_ = top;
    nearPlane_ = nearPlane;
    farPlane_ = farPlane;
    projectionType_ = ProjectionType::Orthographic;
    projectionDirty_ = true;
}

//============================================================================
// PROJECTION TYPE
//============================================================================

void Camera::SetProjectionType(ProjectionType type) {
    projectionType_ = type;
    projectionDirty_ = true;
}

Camera::ProjectionType Camera::GetProjectionType() const {
    return projectionType_;
}

//============================================================================
// CLIPPING PLANES
//============================================================================

void Camera::SetNearPlane(float nearPlane) {
    nearPlane_ = nearPlane;
    projectionDirty_ = true;
}

void Camera::SetFarPlane(float farPlane) {
    farPlane_ = farPlane;
    projectionDirty_ = true;
}

float Camera::GetNearPlane() const {
    return nearPlane_;
}

float Camera::GetFarPlane() const {
    return farPlane_;
}

//============================================================================
// ASPECT RATIO
//============================================================================

void Camera::SetAspectRatio(float aspectRatio) {
    aspectRatio_ = aspectRatio;
    projectionDirty_ = true;
}

float Camera::GetAspectRatio() const {
    return aspectRatio_;
}

//============================================================================
// HELPER METHODS (PRIVATE)
//============================================================================

// Note: CreatePerspectiveMatrix and CreateOrthographicMatrix are implemented
// as private helper methods above, but could also be static utility functions

//============================================================================
// END OF FILE
//================================================================