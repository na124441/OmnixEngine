//============================================================================
// ECSComponents.h - Complete ECS Component Definitions
//
// All component types used by the SceneManager + your custom components
//
// Created: November 25, 2025
//============================================================================

#pragma once

#include <string>
#include <cstdint>
#include "../Scene/Vector3.h"
#include "../Scene/Quaternion.h"
#include "../Scene/Matrix4x4.h"

//============================================================================
// CORE SCENE COMPONENTS
//============================================================================

/**
 * @brief TagComponent - Simple name/label for entities
 */
struct TagComponent {
    std::string tag;

    TagComponent() : tag("Unnamed") {}
    TagComponent(const std::string& name) : tag(name) {}
};

/**
 * @brief LayerComponent - Organizational layer (for rendering, physics filtering)
 */
struct LayerComponent {
    uint32_t layer;
    std::string layerName;

    LayerComponent() : layer(0), layerName("Default") {}
    LayerComponent(uint32_t l, const std::string& name = "Default")
        : layer(l), layerName(name) {}
};

/**
 * @brief HealthComponent - Simple health tracking
 */
struct HealthComponent {
    float current;
    float max;

    HealthComponent() : current(100.0f), max(100.0f) {}
    HealthComponent(float h) : current(h), max(h) {}
};

//============================================================================
// TRANSFORM & HIERARCHY
//============================================================================

/**
 * @brief TransformComponent - Position, rotation, scale in 3D space
 */
struct TransformComponent {
    Vector3 position;
    Quaternion rotation;
    Vector3 scale;
    Matrix4x4 worldMatrix;
    bool dirty;  // Needs recalculation?

    TransformComponent()
        : position(0, 0, 0)
        , rotation(0, 0, 0, 1)
        , scale(1, 1, 1)
        , dirty(true)
    {}
};

//============================================================================
// RENDERING COMPONENTS
//============================================================================

/**
 * @brief MeshRendererComponent - Mesh rendering data
 */
struct MeshRendererComponent {
    uint32_t meshID;          // Mesh asset ID
    uint32_t materialID;      // Material asset ID
    bool visible;             // Render this mesh?
    bool castShadows;         // Cast shadows?
    bool receiveShadows;      // Receive shadows?

    MeshRendererComponent()
        : meshID(0)
        , materialID(0)
        , visible(true)
        , castShadows(true)
        , receiveShadows(true)
    {}
};

/**
 * @brief CameraComponent - Camera view settings
 */
struct CameraComponent {
    enum class ProjectionType {
        Perspective,
        Orthographic
    };

    ProjectionType projectionType;
    float fov;                  // Field of view (perspective)
    float aspectRatio;          // Width / Height
    float nearPlane;            // Near clipping plane
    float farPlane;             // Far clipping plane
    float orthographicSize;     // Size for orthographic
    bool isPrimary;             // Is this the main camera?

    CameraComponent()
        : projectionType(ProjectionType::Perspective)
        , fov(60.0f)
        , aspectRatio(16.0f / 9.0f)
        , nearPlane(0.1f)
        , farPlane(1000.0f)
        , orthographicSize(10.0f)
        , isPrimary(true)
    {}
};

/**
 * @brief LightComponent - Light source
 */
struct LightComponent {
    enum class LightType {
        Directional,
        Point,
        Spot
    };

    LightType type;
    Vector3 color;
    float intensity;
    float range;            // For point/spot lights
    float spotAngle;        // For spot lights
    bool castShadows;

    LightComponent()
        : type(LightType::Directional)
        , color(1, 1, 1)
        , intensity(1.0f)
        , range(10.0f)
        , spotAngle(45.0f)
        , castShadows(true)
    {}
};

//============================================================================
// PHYSICS COMPONENTS
//============================================================================

/**
 * @brief RigidBodyComponent - Physics body
 */
struct RigidBodyComponent {
    float mass;
    Vector3 velocity;
    Vector3 angularVelocity;
    bool useGravity;
    bool isKinematic;       // Moved by script, not physics
    float drag;             // Linear drag
    float angularDrag;      // Rotational drag
    bool freezePositionX = false;
    bool freezePositionY = false;
    bool freezePositionZ = false;
    bool freezeRotationX = false;
    bool freezeRotationY = false;
    bool freezeRotationZ = false;

    RigidBodyComponent()
        : mass(1.0f)
        , velocity(0, 0, 0)
        , angularVelocity(0, 0, 0)
        , useGravity(true)
        , isKinematic(false)
        , drag(0.0f)
        , angularDrag(0.05f)
    {}
};

/**
 * @brief ColliderType - Types of colliders
 */
enum class ColliderType {
    Box,
    Sphere,
    Capsule,
    Mesh
};

/**
 * @brief ColliderComponent - Collision shape
 */
struct ColliderComponent {
    ColliderType type;
    Vector3 center;         // Local offset
    Vector3 size;           // For box
    float radius;           // For sphere/capsule
    float height;           // For capsule
    bool isTrigger;         // Trigger events without physical collision

    ColliderComponent()
        : type(ColliderType::Box)
        , center(0, 0, 0)
        , size(1, 1, 1)
        , radius(0.5f)
        , height(2.0f)
        , isTrigger(false)
    {}
};

//============================================================================
// AUDIO COMPONENTS
//============================================================================

/**
 * @brief AudioSourceComponent - Audio playback
 */
struct AudioSourceComponent {
    uint32_t audioClipID;   // Audio asset ID
    bool isPlaying;
    bool loop;
    float volume;
    float pitch;
    bool spatialize;        // 3D positional audio?
    float minDistance;      // Minimum distance for attenuation
    float maxDistance;      // Maximum distance

    AudioSourceComponent()
        : audioClipID(0)
        , isPlaying(false)
        , loop(false)
        , volume(1.0f)
        , pitch(1.0f)
        , spatialize(true)
        , minDistance(1.0f)
        , maxDistance(500.0f)
    {}
};

//============================================================================
// ANIMATION COMPONENTS
//============================================================================

/**
 * @brief AnimatorComponent - Animation state machine
 */
struct AnimatorComponent {
    uint32_t animatorControllerID;  // Controller asset ID
    uint32_t currentAnimationID;    // Current playing animation
    float currentTime;              // Playback time
    float speed;                    // Playback speed multiplier
    bool isPlaying;

    AnimatorComponent()
        : animatorControllerID(0)
        , currentAnimationID(0)
        , currentTime(0.0f)
        , speed(1.0f)
        , isPlaying(false)
    {}
};

//============================================================================
// SCRIPTING COMPONENTS
//============================================================================

/**
 * @brief ScriptComponent - Attached script behavior
 */
struct ScriptComponent {
    uint32_t scriptID;      // Script asset ID
    std::string scriptName; // Script class name
    bool enabled;

    ScriptComponent()
        : scriptID(0)
        , scriptName("")
        , enabled(true)
    {}
};

//============================================================================
// PLAYER COMPONENTS
//============================================================================

/**
 * @brief PlayerControllerComponent - Marks an entity as the player
 */
struct PlayerControllerComponent {
    float moveSpeed;
    float lookSensitivity;

    PlayerControllerComponent()
        : moveSpeed(5.0f)
        , lookSensitivity(0.1f)
    {}
};

#include "Runtime/Public/AssetHandle.h"

//============================================================================
// NAME & IDENTIFICATION
//============================================================================

/**
 * @brief NameComponent - Assigns a readable name to an entity
 */
struct NameComponent {
    std::string name;

    NameComponent() : name("Entity") {}
    NameComponent(const std::string& n) : name(n) {}
};

//============================================================================
// ASSET ASSIGNMENT COMPONENTS
//============================================================================

struct RenderableMeshComponent {
    AssetHandle meshAssetHandle;

    RenderableMeshComponent() = default;
    explicit RenderableMeshComponent(AssetHandle handle) : meshAssetHandle(handle) {}
};

struct MaterialComponent {
    AssetHandle materialAssetHandle;

    MaterialComponent() = default;
    explicit MaterialComponent(AssetHandle handle) : materialAssetHandle(handle) {}
};

struct StaticBodyComponent {
    bool enabled = true;
    uint32_t collisionLayer = 1;
    uint32_t collisionMask = 0xFFFFFFFF;
};

struct BoxColliderComponent {
    Vector3 size = { 1.0f, 1.0f, 1.0f };
    Vector3 offset = { 0.0f, 0.0f, 0.0f };
    bool isTrigger = false;
    bool debugDraw = true;
};

struct SphereColliderComponent {
    float radius = 0.5f;
    Vector3 offset = { 0.0f, 0.0f, 0.0f };
    bool isTrigger = false;
    bool debugDraw = true;
};

struct CapsuleColliderComponent {
    float radius = 0.5f;
    float height = 2.0f;
    Vector3 offset = { 0.0f, 0.0f, 0.0f };
    bool isTrigger = false;
    bool debugDraw = true;
};

//============================================================================
// END OF FILE