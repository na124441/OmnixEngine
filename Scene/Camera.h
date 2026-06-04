//============================================================================
// Camera.h - Camera Component (FIXED)
//
// Handles view and projection matrix computation
// Implements GetViewProjection algorithm
//
// Created: November 25, 2025
//============================================================================

#pragma once

#include "Vector3.h"
#include "Matrix4x4.h"

/**
 * @brief Camera - View and projection matrix computation
 *
 * Provides view and projection matrices for rendering.
 * Can be attached to a SceneObject for transform-based positioning.
 */
class Camera {
public:
    /**
     * @brief Camera projection type
     */
    enum class ProjectionType {
        Perspective,
        Orthographic
    };

    //========================================================================
    // CONSTRUCTION
    //========================================================================

    /**
     * @brief Constructor - Initialize with default perspective
     */
    Camera();

    //========================================================================
    // CORE ALGORITHM: GetViewProjection
    //========================================================================

    /**
     * @brief GetViewProjection - Main camera algorithm
     *
     * Algorithm (from specification):
     * 1. Compute view matrix from world transform
     * 2. Compute projection (perspective or orthographic)
     * 3. Return view × projection
     *
     * @param worldMatrix World transform matrix of the camera
     * @return Combined view-projection matrix
     */
    Matrix4x4 GetViewProjection(const Matrix4x4& worldMatrix);

    //========================================================================
    // MATRIX COMPUTATION
    //========================================================================

    /**
     * @brief Compute view matrix from world transform
     * @param worldMatrix Camera's world transform
     * @return View matrix
     */
    Matrix4x4 ComputeViewMatrix(const Matrix4x4& worldMatrix);

    /**
     * @brief Compute projection matrix
     * @return Projection matrix (perspective or orthographic)
     */
    Matrix4x4 ComputeProjectionMatrix();

    /**
     * @brief Get view matrix
     * @param worldMatrix Camera's world transform
     * @return View matrix
     */
    Matrix4x4 GetViewMatrix(const Matrix4x4& worldMatrix);

    /**
     * @brief Get projection matrix
     * @return Projection matrix
     */
    Matrix4x4 GetProjectionMatrix();

    //========================================================================
    // PERSPECTIVE SETTINGS
    //========================================================================

    /**
     * @brief Set perspective projection
     * @param fov Field of view in degrees
     * @param aspectRatio Width / Height
     * @param nearPlane Near clipping plane distance
     * @param farPlane Far clipping plane distance
     */
    void SetPerspective(float fov, float aspectRatio, float nearPlane, float farPlane);

    /**
     * @brief Get field of view
     * @return FOV in degrees
     */
    float GetFieldOfView() const;

    /**
     * @brief Set field of view
     * @param fov FOV in degrees
     */
    void SetFieldOfView(float fov);

    //========================================================================
    // ORTHOGRAPHIC SETTINGS
    //========================================================================

    /**
     * @brief Set orthographic projection
     * @param left Left boundary
     * @param right Right boundary
     * @param bottom Bottom boundary
     * @param top Top boundary
     * @param nearPlane Near clipping plane
     * @param farPlane Far clipping plane
     */
    void SetOrthographic(float left, float right, float bottom, float top,
                         float nearPlane, float farPlane);

    //========================================================================
    // PROJECTION TYPE
    //========================================================================

    /**
     * @brief Set projection type
     * @param type Perspective or Orthographic
     */
    void SetProjectionType(ProjectionType type);

    /**
     * @brief Get projection type
     * @return Current projection type
     */
    ProjectionType GetProjectionType() const;

    //========================================================================
    // CLIPPING PLANES
    //========================================================================

    /**
     * @brief Set near clipping plane
     * @param nearPlane Distance to near plane
     */
    void SetNearPlane(float nearPlane);

    /**
     * @brief Set far clipping plane
     * @param farPlane Distance to far plane
     */
    void SetFarPlane(float farPlane);

    /**
     * @brief Get near clipping plane
     * @return Near plane distance
     */
    float GetNearPlane() const;

    /**
     * @brief Get far clipping plane
     * @return Far plane distance
     */
    float GetFarPlane() const;

    //========================================================================
    // ASPECT RATIO
    //========================================================================

    /**
     * @brief Set aspect ratio
     * @param aspectRatio Width / Height
     */
    void SetAspectRatio(float aspectRatio);

    /**
     * @brief Get aspect ratio
     * @return Aspect ratio
     */
    float GetAspectRatio() const;

private:
    //========================================================================
    // PRIVATE HELPER METHODS
    //========================================================================

    /**
     * @brief Create perspective projection matrix
     * @param fov Field of view in degrees
     * @param aspect Aspect ratio
     * @param near Near plane
     * @param far Far plane
     * @return Perspective projection matrix
     */
    Matrix4x4 CreatePerspectiveMatrix(float fov, float aspect, float near, float far);

    /**
     * @brief Create orthographic projection matrix
     * @param left Left boundary
     * @param right Right boundary
     * @param bottom Bottom boundary
     * @param top Top boundary
     * @param near Near plane
     * @param far Far plane
     * @return Orthographic projection matrix
     */
    Matrix4x4 CreateOrthographicMatrix(float left, float right, float bottom,
                                      float top, float near, float far);

    //========================================================================
    // MEMBER VARIABLES
    //========================================================================

    // Projection type
    ProjectionType projectionType_;

    // Perspective parameters
    float fieldOfView_;       // In degrees
    float aspectRatio_;       // Width / Height

    // Orthographic parameters
    float orthoLeft_;
    float orthoRight_;
    float orthoBottom_;
    float orthoTop_;

    // Clipping planes (shared)
    float nearPlane_;
    float farPlane_;

    // Cached matrices
    Matrix4x4 viewMatrix_;
    Matrix4x4 projectionMatrix_;
    Matrix4x4 viewProjectionMatrix_;

    // Dirty flags
    bool projectionDirty_;
};

//============================================================================
// END OF FILE
//================================================================