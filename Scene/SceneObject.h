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
class Coordinator;
struct StagedComponents;

/**
 * @brief SceneObject - Hierarchical scene graph node and lightweight ECS handle
 */
class SceneObject {
public:
    //========================================================================
    // CONSTRUCTION / DESTRUCTION
    //========================================================================
    explicit SceneObject(const std::string& name);
    SceneObject(const std::string& name, Entity entity, Coordinator* coordinator = nullptr);
    ~SceneObject();

    void InitializeWithECS(Coordinator& coordinator);
    void BindECS(Coordinator* coordinator, Entity entity);
    void SetCoordinator(Coordinator* coordinator);
    Coordinator* GetCoordinator() const { return m_Coordinator; }
    Entity GetECSEntity() const { return m_ECSEntity; }
    void SetECSEntity(Entity entity);

    //========================================================================
    // GENERIC COMPONENT ACCESS (Direct forward to Coordinator)
    //========================================================================
    template<typename T>
    bool HasComponent() const {
        if (!m_Coordinator || m_ECSEntity == 0) return false;
        return m_Coordinator->HasComponent<T>(m_ECSEntity);
    }

    template<typename T>
    T& GetComponent() {
        return m_Coordinator->GetComponent<T>(m_ECSEntity);
    }

    template<typename T>
    const T& GetComponent() const {
        return m_Coordinator->GetComponent<T>(m_ECSEntity);
    }

    template<typename T>
    void AddComponent(const T& component) {
        if (m_Coordinator && m_ECSEntity != 0) {
            m_Coordinator->AddComponent<T>(m_ECSEntity, component);
        }
    }

    template<typename T>
    void RemoveComponent() {
        if (m_Coordinator && m_ECSEntity != 0) {
            m_Coordinator->RemoveComponent<T>(m_ECSEntity);
        }
    }

    //========================================================================
    // COMPONENT HELPERS (Authoritative forwarding to Coordinator)
    //========================================================================
    bool HasRenderableMesh() const;
    void SetRenderableMesh(AssetHandle handle);
    void ClearRenderableMesh();
    AssetHandle GetMeshAssetHandle() const;

    bool HasMaterial() const;
    void SetMaterial(AssetHandle handle);
    void ClearMaterial();
    AssetHandle GetMaterialAssetHandle() const;

    bool HasStaticBody() const;
    void SetStaticBody(const StaticBodyComponent& comp);
    void ClearStaticBody();
    const StaticBodyComponent& GetStaticBody() const;

    bool HasBoxCollider() const;
    void SetBoxCollider(const BoxColliderComponent& comp);
    void ClearBoxCollider();
    const BoxColliderComponent& GetBoxCollider() const;

    bool HasSphereCollider() const;
    void SetSphereCollider(const SphereColliderComponent& comp);
    void ClearSphereCollider();
    const SphereColliderComponent& GetSphereCollider() const;

    bool HasCapsuleCollider() const;
    void SetCapsuleCollider(const CapsuleColliderComponent& comp);
    void ClearCapsuleCollider();
    const CapsuleColliderComponent& GetCapsuleCollider() const;

    bool HasPlayerStart() const;
    void SetPlayerStart(const PlayerStartComponent& comp);
    void ClearPlayerStart();
    const PlayerStartComponent& GetPlayerStart() const;

    bool HasCharacterController() const;
    void SetCharacterController(const CharacterControllerComponent& comp);
    void ClearCharacterController();
    const CharacterControllerComponent& GetCharacterController() const;

    bool HasCameraComponent() const;
    void SetCameraComponent(const CameraComponent& comp);
    void ClearCameraComponent();
    const CameraComponent& GetCameraComponent() const;

    bool HasInputComponent() const;
    void SetInputComponent(const InputComponent& comp);
    void ClearInputComponent();
    const InputComponent& GetInputComponent() const;

    bool HasTrigger() const;
    void SetTrigger(const TriggerComponent& comp);
    void ClearTrigger();
    const TriggerComponent& GetTrigger() const;

    bool HasInteractable() const;
    void SetInteractable(const InteractableComponent& comp);
    void ClearInteractable();
    const InteractableComponent& GetInteractable() const;

    bool HasObjective() const;
    void SetObjective(const ObjectiveComponent& comp);
    void ClearObjective();
    const ObjectiveComponent& GetObjective() const;

    bool HasAudioSource() const;
    void SetAudioSource(const AudioSourceComponent& comp);
    void ClearAudioSource();
    const AudioSourceComponent& GetAudioSource() const;

    bool HasSimpleState() const;
    void SetSimpleState(const SimpleStateComponent& comp);
    void ClearSimpleState();
    const SimpleStateComponent& GetSimpleState() const;

    bool HasActivatable() const;
    void SetActivatable(const ActivatableComponent& comp);
    void ClearActivatable();
    const ActivatableComponent& GetActivatable() const;

    bool HasDoor() const;
    void SetDoor(const DoorComponent& comp);
    void ClearDoor();
    const DoorComponent& GetDoor() const;

    bool HasCheckpoint() const;
    void SetCheckpoint(const CheckpointComponent& comp);
    void ClearCheckpoint();
    const CheckpointComponent& GetCheckpoint() const;

    bool HasDirectionalLight() const;
    void SetDirectionalLight(const DirectionalLightComponent& comp);
    void ClearDirectionalLight();
    const DirectionalLightComponent& GetDirectionalLight() const;

    bool HasPointLight() const;
    void SetPointLight(const PointLightComponent& comp);
    void ClearPointLight();
    const PointLightComponent& GetPointLight() const;

    bool HasSkyLight() const;
    void SetSkyLight(const SkyLightComponent& comp);
    void ClearSkyLight();
    const SkyLightComponent& GetSkyLight() const;

    bool HasSpotLight() const;
    void SetSpotLight(const SpotLightComponent& comp);
    void ClearSpotLight();
    const SpotLightComponent& GetSpotLight() const;

    //========================================================================
    // CORE ALGORITHMS
    //========================================================================
    void Update(float deltaTime);
    void AddChild(SceneObject* child);
    void RemoveChild(SceneObject* child);
    void SetParent(SceneObject* newParent);
    SceneObject* GetParent() const;
    const std::vector<SceneObject*>& GetChildren() const;
    bool HasChildren() const;
    size_t GetChildCount() const;
    SceneObject* FindChild(const std::string& name) const;

    //========================================================================
    // OBJECT PROPERTIES
    //========================================================================
    uint32_t GetID() const;
    void SetID(uint32_t id);
    const std::string& GetName() const;
    void SetName(const std::string& name);
    bool IsActive() const;
    void SetActive(bool active);

    //========================================================================
    // LIFECYCLE
    //========================================================================
    void Initialize();
    void Cleanup();

    //========================================================================
    // TRANSFORM (Bound to ECS TransformComponent)
    //========================================================================
    Transform transform;

private:
    void UpdateTransformHierarchy();
    void UpdateChildren(float deltaTime);
    void RecomputeTransformInheritance();
    void RebindEntityIDHierarchy();

    StagedComponents& EnsureStaged();

    uint32_t entityID_;
    std::string name_;
    bool active_;
    SceneObject* parent_;
    std::vector<SceneObject*> children_;
    bool initialized_;

    Entity m_ECSEntity;
    Coordinator* m_Coordinator;
    std::unique_ptr<StagedComponents> m_Staged;
};

//============================================================================
// END OF FILE
//===============================================================