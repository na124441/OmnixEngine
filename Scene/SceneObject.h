//============================================================================
// SceneObject.h - Hierarchical Scene Node
//
// A hierarchical node with Transform and bound EntityID
// Core building block of the scene graph
//
// Created: November 25, 2025
//============================================================================

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include "../ECS/ECSconfig.h"
#include "../ECS/ECSComponents.h"
#include "Transform.h"
#include "Runtime/Public/AssetHandle.h"

/**
 * @brief SceneObject - Hierarchical scene graph node
 *
 * Responsibilities:
 * - Maintain transform hierarchy (parent-child relationships)
 * - Update transforms (local → world)
 * - Propagate updates to children
 * - Store EntityID for ECS integration
 * - Manage scene graph structure
 *
 * Update Order:
 * 1. Update own transform (local → global)
 * 2. ECS updates components (managed externally)
 * 3. Update children recursively
 */
class SceneObject {
public:
    //========================================================================
    // CONSTRUCTION / DESTRUCTION
    //========================================================================
    void InitializeWithECS(class Coordinator& coordinator);
    Entity GetECSEntity() const { return m_ECSEntity; }
    void SetECSEntity(Entity entity) { m_ECSEntity = entity; }
    /**
     * @brief Constructor
     * @param name Object name
     */
    explicit SceneObject(const std::string& name);

    /**
     * @brief Destructor
     */
    ~SceneObject();

    //========================================================================
    // CORE ALGORITHM: Update
    //========================================================================

    /**
     * @brief Update - Main update loop for this object
     *
     * Algorithm (from specification):
     * 1. Update Transform (local → global)
     * 2. For each attached component in ECS:
     *    - ECS manages its Update, not SceneObject
     * 3. Call Update on each child SceneObject
     *
     * @param deltaTime Time elapsed since last update
     */
    void Update(float deltaTime);

    //========================================================================
    // CORE ALGORITHM: AddChild
    //========================================================================

    /**
     * @brief AddChild - Add child to this object
     *
     * Algorithm (from specification):
     * 1. Set child.parent = this
     * 2. Add to children list
     * 3. Recompute transform inheritance
     * 4. Rebind EntityID hierarchy if needed
     *
     * @param child Child object to add
     */
    void AddChild(SceneObject* child);

    //========================================================================
    // HIERARCHY MANAGEMENT
    //========================================================================

    /**
     * @brief Remove child from this object
     * @param child Child to remove
     */
    void RemoveChild(SceneObject* child);

    /**
     * @brief Set parent of this object
     * @param newParent New parent (nullptr for root)
     */
    void SetParent(SceneObject* newParent);

    /**
     * @brief Get parent object
     * @return Pointer to parent (nullptr if root)
     */
    SceneObject* GetParent() const;

    /**
     * @brief Get all children
     * @return Const reference to children vector
     */
    const std::vector<SceneObject*>& GetChildren() const;

    /**
     * @brief Check if this object has children
     * @return True if has children
     */
    bool HasChildren() const;

    /**
     * @brief Get child count
     * @return Number of children
     */
    size_t GetChildCount() const;

    /**
     * @brief Find child by name
     * @param name Child name
     * @return Pointer to child (nullptr if not found)
     */
    SceneObject* FindChild(const std::string& name) const;

    //========================================================================
    // OBJECT PROPERTIES
    //========================================================================

    /**
     * @brief Get EntityID
     * @return Unique entity identifier
     */
    uint32_t GetID() const;

    /**
     * @brief Set EntityID (use carefully - normally set by Scene)
     * @param id Entity ID
     */
    void SetID(uint32_t id);

    /**
     * @brief Get object name
     * @return Object name string
     */
    const std::string& GetName() const;

    /**
     * @brief Set object name
     * @param name New name
     */
    void SetName(const std::string& name);

    /**
     * @brief Get active state
     * @return True if active
     */
    bool IsActive() const;

    /**
     * @brief Set active state
     * @param active Active state
     */
    void SetActive(bool active);

    //========================================================================
    // LIFECYCLE
    //========================================================================

    /**
     * @brief Initialize - Called when object is added to scene
     */
    void Initialize();

    /**
     * @brief Cleanup - Called when object is removed from scene
     */
    void Cleanup();

    //========================================================================
    // PUBLIC MEMBER - Transform
    //========================================================================

    /**
     * @brief Transform component
     *
     * Public member for easy access:
     * object.transform.SetPosition(...)
     */
    Transform transform;

    bool m_HasRenderableMesh = false;
    AssetHandle m_MeshAssetHandle;

    bool m_HasMaterial = false;
    AssetHandle m_MaterialAssetHandle;

    bool m_HasStaticBody = false;
    StaticBodyComponent m_StaticBody;

    bool m_HasBoxCollider = false;
    BoxColliderComponent m_BoxCollider;

    bool m_HasSphereCollider = false;
    SphereColliderComponent m_SphereCollider;

    bool m_HasCapsuleCollider = false;
    CapsuleColliderComponent m_CapsuleCollider;

    bool m_HasPlayerStart = false;
    PlayerStartComponent m_PlayerStart;

    bool m_HasCharacterController = false;
    CharacterControllerComponent m_CharacterController;

    bool m_HasCameraComponent = false;
    CameraComponent m_CameraComponent;

    bool m_HasInputComponent = false;
    InputComponent m_InputComponent;

    bool m_HasTrigger = false;
    TriggerComponent m_Trigger;

    bool m_HasInteractable = false;
    InteractableComponent m_Interactable;

    bool m_HasObjective = false;
    ObjectiveComponent m_Objective;

    bool m_HasAudioSource = false;
    AudioSourceComponent m_AudioSource;

    bool m_HasSimpleState = false;
    SimpleStateComponent m_SimpleState;

    bool m_HasActivatable = false;
    ActivatableComponent m_Activatable;

    bool m_HasDoor = false;
    DoorComponent m_Door;

    bool m_HasCheckpoint = false;
    CheckpointComponent m_Checkpoint;

    bool m_HasDirectionalLight = false;
    DirectionalLightComponent m_DirectionalLight;

    bool m_HasPointLight = false;
    PointLightComponent m_PointLight;

    bool m_HasSkyLight = false;
    SkyLightComponent m_SkyLight;

    bool m_HasSpotLight = false;
    SpotLightComponent m_SpotLight;

    bool m_HasReflectionProbe = false;
    ReflectionProbeComponent m_ReflectionProbe;

    void SetRenderableMesh(AssetHandle handle) { m_MeshAssetHandle = handle; m_HasRenderableMesh = true; }
    void ClearRenderableMesh() { m_HasRenderableMesh = false; }

    void SetMaterial(AssetHandle handle) { m_MaterialAssetHandle = handle; m_HasMaterial = true; }
    void ClearMaterial() { m_HasMaterial = false; }

    void SetStaticBody(const StaticBodyComponent& comp) { m_StaticBody = comp; m_HasStaticBody = true; }
    void ClearStaticBody() { m_HasStaticBody = false; }

    void SetBoxCollider(const BoxColliderComponent& comp) { m_BoxCollider = comp; m_HasBoxCollider = true; }
    void ClearBoxCollider() { m_HasBoxCollider = false; }

    void SetSphereCollider(const SphereColliderComponent& comp) { m_SphereCollider = comp; m_HasSphereCollider = true; }
    void ClearSphereCollider() { m_HasSphereCollider = false; }

    void SetCapsuleCollider(const CapsuleColliderComponent& comp) { m_CapsuleCollider = comp; m_HasCapsuleCollider = true; }
    void ClearCapsuleCollider() { m_HasCapsuleCollider = false; }

    void SetPlayerStart(const PlayerStartComponent& comp) { m_PlayerStart = comp; m_HasPlayerStart = true; }
    void ClearPlayerStart() { m_HasPlayerStart = false; }

    void SetCharacterController(const CharacterControllerComponent& comp) { m_CharacterController = comp; m_HasCharacterController = true; }
    void ClearCharacterController() { m_HasCharacterController = false; }

    void SetCameraComponent(const CameraComponent& comp) { m_CameraComponent = comp; m_HasCameraComponent = true; }
    void ClearCameraComponent() { m_HasCameraComponent = false; }

    void SetInputComponent(const InputComponent& comp) { m_InputComponent = comp; m_HasInputComponent = true; }
    void ClearInputComponent() { m_HasInputComponent = false; }

    void SetTrigger(const TriggerComponent& comp) { m_Trigger = comp; m_HasTrigger = true; }
    void ClearTrigger() { m_HasTrigger = false; }

    void SetInteractable(const InteractableComponent& comp) { m_Interactable = comp; m_HasInteractable = true; }
    void ClearInteractable() { m_HasInteractable = false; }

    void SetObjective(const ObjectiveComponent& comp) { m_Objective = comp; m_HasObjective = true; }
    void ClearObjective() { m_HasObjective = false; }

    void SetAudioSource(const AudioSourceComponent& comp) { m_AudioSource = comp; m_HasAudioSource = true; }
    void ClearAudioSource() { m_HasAudioSource = false; }

    void SetSimpleState(const SimpleStateComponent& comp) { m_SimpleState = comp; m_HasSimpleState = true; }
    void ClearSimpleState() { m_HasSimpleState = false; }

    void SetActivatable(const ActivatableComponent& comp) { m_Activatable = comp; m_HasActivatable = true; }
    void ClearActivatable() { m_HasActivatable = false; }

    void SetDoor(const DoorComponent& comp) { m_Door = comp; m_HasDoor = true; }
    void ClearDoor() { m_HasDoor = false; }

    void SetCheckpoint(const CheckpointComponent& comp) { m_Checkpoint = comp; m_HasCheckpoint = true; }
    void ClearCheckpoint() { m_HasCheckpoint = false; }

    void SetDirectionalLight(const DirectionalLightComponent& comp) { m_DirectionalLight = comp; m_HasDirectionalLight = true; }
    void ClearDirectionalLight() { m_HasDirectionalLight = false; }

    void SetPointLight(const PointLightComponent& comp) { m_PointLight = comp; m_HasPointLight = true; }
    void ClearPointLight() { m_HasPointLight = false; }

    void SetSkyLight(const SkyLightComponent& comp) { m_SkyLight = comp; m_HasSkyLight = true; }
    void ClearSkyLight() { m_HasSkyLight = false; }

    void SetSpotLight(const SpotLightComponent& comp) { m_SpotLight = comp; m_HasSpotLight = true; }
    void ClearSpotLight() { m_HasSpotLight = false; }

    void SetReflectionProbe(const ReflectionProbeComponent& comp) { m_ReflectionProbe = comp; m_HasReflectionProbe = true; }
    void ClearReflectionProbe() { m_HasReflectionProbe = false; }

private:
    //========================================================================
    // INTERNAL HELPERS
    //========================================================================
    Entity m_ECSEntity;
    /**
     * @brief Update transform with parent's world transform
     */
    void UpdateTransformHierarchy();

    /**
     * @brief Update all children recursively
     * @param deltaTime Time delta
     */
    void UpdateChildren(float deltaTime);

    /**
     * @brief Recompute transform inheritance for this object and children
     */
    void RecomputeTransformInheritance();

    /**
     * @brief Rebind EntityID hierarchy (notify ECS of parent-child relationship)
     */
    void RebindEntityIDHierarchy();

    //========================================================================
    // MEMBER VARIABLES
    //========================================================================

    // Identity
    uint32_t entityID_;       // Unique entity ID
    std::string name_;        // Object name
    bool active_;             // Active state

    // Hierarchy
    SceneObject* parent_;              // Parent object (nullptr if root)
    std::vector<SceneObject*> children_;  // Child objects

    // State flags
    bool initialized_;        // Has Initialize() been called?
};

//============================================================================
// END OF FILE
//===============================================================