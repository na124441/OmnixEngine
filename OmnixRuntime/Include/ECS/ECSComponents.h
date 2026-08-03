//============================================================================
// ECSComponents.h - Complete ECS Component Definitions
//
// All component types used by the SceneManager + your custom components
//
// Created: November 25, 2025
//============================================================================

#pragma once

#include <string>
#include <vector>
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

/**
 * @brief HierarchyComponent - Pure ECS Parent/Child Hierarchy Linkage
 */
struct HierarchyComponent {
    uint32_t parent;                 // Parent Entity ID (0xFFFFFFFF / INVALID_ENTITY if root)
    std::vector<uint32_t> children;  // List of child Entity IDs
    uint32_t depth;                  // Tree depth level (0 for root)

    HierarchyComponent()
        : parent(0xFFFFFFFF)
        , depth(0)
    {}

    HierarchyComponent(uint32_t parentEntity, uint32_t depthLevel = 0)
        : parent(parentEntity)
        , depth(depthLevel)
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
    Vector3 localOffset;        // Eye offset height (e.g. 1.6m)
    float exposure = 1.0f;

    CameraComponent()
        : projectionType(ProjectionType::Perspective)
        , fov(60.0f)
        , aspectRatio(16.0f / 9.0f)
        , nearPlane(0.1f)
        , farPlane(1000.0f)
        , orthographicSize(10.0f)
        , isPrimary(true)
        , localOffset{0.0f, 1.6f, 0.0f}
        , exposure(1.0f)
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
    std::string ClipPath = "";
    bool PlayOnStart = false;
    bool Loop = false;
    float Volume = 1.0f;
    bool IsPlaying = false;

    AudioSourceComponent() = default;
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

#include "Runtime/AssetHandle.h"

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

struct PlayerStartComponent {
    bool active = true;
};

struct CharacterControllerComponent {
    float moveSpeed = 5.0f;
    float sprintSpeed = 8.0f;
    float mouseSensitivity = 0.12f;

    float gravity = -20.0f;
    float jumpVelocity = 7.0f;

    float capsuleRadius = 0.35f;
    float capsuleHeight = 1.8f;

    float groundCheckDistance = 0.12f;
    float skinWidth = 0.03f;

    Vector3 velocity = {0.0f, 0.0f, 0.0f};
    bool isGrounded = false;
    bool enableJump = false;

    float yaw = -90.0f;
    float pitch = 0.0f;
};

struct InputComponent {
    bool enabled = true;
};

enum class TriggerShapeType : uint8_t
{
    Box = 0,
    Sphere,
    Capsule
};

struct TriggerComponent
{
    bool enabled = true;
    TriggerShapeType shapeType = TriggerShapeType::Box;
    Vector3 boxSize = {1.0f, 1.0f, 1.0f};
    float sphereRadius = 0.5f;
    float capsuleRadius = 0.5f;
    float capsuleHeight = 2.0f;
    Vector3 offset = {0.0f, 0.0f, 0.0f};
    bool fireEnter = true;
    bool fireStay = false;
    bool fireExit = true;
    std::string eventName = "Trigger";
};

#include "Gameplay/Components/InteractableComponent.h"
using InteractableComponent = eng::runtime::InteractableComponent;
using InteractionType = eng::runtime::InteractionType;

#include "Gameplay/Components/ObjectiveComponent.h"
using ObjectiveComponent = eng::runtime::ObjectiveComponent;
using ObjectiveCompletionMode = eng::runtime::ObjectiveCompletionMode;

#include "Gameplay/StateObjects/SimpleStateComponent.h"
using SimpleStateComponent = eng::runtime::SimpleStateComponent;
using SimpleObjectState = eng::runtime::SimpleObjectState;

#include "Gameplay/StateObjects/ActivatableComponent.h"
using ActivatableComponent = eng::runtime::ActivatableComponent;

#include "Gameplay/StateObjects/DoorComponent.h"
using DoorComponent = eng::runtime::DoorComponent;
using DoorOpenMode = eng::runtime::DoorOpenMode;

#include "Gameplay/Checkpoints/CheckpointComponent.h"
using CheckpointComponent = eng::runtime::CheckpointComponent;


struct DirectionalLightComponent
{
    bool enabled = true;
    Vector3 color = {1.0f, 0.96f, 0.88f};
    float intensity = 3.0f;
    bool castShadows = false;
    float shadowBias = 0.0015f;
    float shadowSlopeBias = 0.003f;
    float shadowNormalBias = 0.05f;
    float shadowStrength = 1.0f;
    int shadowResolution = 2048;
    int pcfKernelSize = 3;
    float shadowDistance = 75.0f;
    float temperature = 6500.0f;
    uint32_t layerMask = 0xFFFFFFFF;
};

struct PointLightComponent
{
    bool enabled = true;
    Vector3 color = {1.0f, 0.85f, 0.55f};
    float intensity = 5.0f;
    float radius = 10.0f;
    bool castShadows = false;
    float temperature = 6500.0f;
    uint32_t layerMask = 0xFFFFFFFF;
    float sourceRadius = 0.0f;
};

struct SkyLightComponent
{
    bool enabled = true;
    Vector3 color = {0.45f, 0.50f, 0.58f};
    float intensity = 0.55f;
    std::string environmentPath = "";
    float rotation = 0.0f;
    float diffuseIntensity = 1.0f;
    float specularIntensity = 1.0f;
    float exposureOffset = 0.0f;
    int mode = 0; // 0 = Procedural, 1 = HDR Cubemap
};

struct SpotLightComponent
{
    bool enabled = true;
    Vector3 color = {1.0f, 0.9f, 0.75f};
    float intensity = 8.0f;
    float range = 15.0f;
    float innerConeAngle = 15.0f;
    float outerConeAngle = 30.0f;
    bool castShadows = false;
    float temperature = 6500.0f;
    uint32_t layerMask = 0xFFFFFFFF;
    float sourceRadius = 0.0f;
};

struct ReflectionProbeComponent
{
    bool enabled = true;
    Vector3 position = {0.0f, 0.0f, 0.0f};
    Vector3 boxMin = {-10.0f, -10.0f, -10.0f};
    Vector3 boxMax = {10.0f, 10.0f, 10.0f};
    float blendDistance = 1.0f;
    float intensity = 1.0f;
    uint32_t priority = 0;
    bool isBox = true;
    std::string capturePath = "";
};

//============================================================================
// BOUNDS COMPONENTS
//============================================================================

struct BoundsComponent {
    Vector3 localMin;
    Vector3 localMax;
    Vector3 worldMin;
    Vector3 worldMax;
    bool dirty;

    // Bounding Sphere
    bool hasSphere;
    Vector3 sphereCenter;
    float sphereRadius;
    Vector3 worldSphereCenter;
    float worldSphereRadius;

    BoundsComponent()
        : localMin(0.0f, 0.0f, 0.0f)
        , localMax(0.0f, 0.0f, 0.0f)
        , worldMin(0.0f, 0.0f, 0.0f)
        , worldMax(0.0f, 0.0f, 0.0f)
        , dirty(true)
        , hasSphere(false)
        , sphereCenter(0.0f, 0.0f, 0.0f)
        , sphereRadius(0.0f)
        , worldSphereCenter(0.0f, 0.0f, 0.0f)
        , worldSphereRadius(0.0f)
    {}
};

//============================================================================
// END OF FILE
