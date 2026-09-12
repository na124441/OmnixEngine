//============================================================================
// SceneObject.cpp - Lightweight Entity Handle Implementation
//============================================================================

#include "SceneObject.h"
#include "IDPool.h"
#include "../ECS/Coordinator.h"
#include "../ECS/ECSComponents.h"
#include <iostream>
#include <algorithm>

struct StagedComponents {
    bool hasMesh = false;
    AssetHandle meshHandle{0};

    bool hasMaterial = false;
    AssetHandle materialHandle{0};

    bool hasStaticBody = false;
    StaticBodyComponent staticBody{};

    bool hasBoxCollider = false;
    BoxColliderComponent boxCollider{};

    bool hasSphereCollider = false;
    SphereColliderComponent sphereCollider{};

    bool hasCapsuleCollider = false;
    CapsuleColliderComponent capsuleCollider{};

    bool hasPlayerStart = false;
    PlayerStartComponent playerStart{};

    bool hasCharacterController = false;
    CharacterControllerComponent characterController{};

    bool hasCamera = false;
    CameraComponent camera{};

    bool hasInput = false;
    InputComponent input{};

    bool hasTrigger = false;
    TriggerComponent trigger{};

    bool hasInteractable = false;
    InteractableComponent interactable{};

    bool hasObjective = false;
    ObjectiveComponent objective{};

    bool hasAudioSource = false;
    AudioSourceComponent audioSource{};

    bool hasSimpleState = false;
    SimpleStateComponent simpleState{};

    bool hasActivatable = false;
    ActivatableComponent activatable{};

    bool hasDoor = false;
    DoorComponent door{};

    bool hasCheckpoint = false;
    CheckpointComponent checkpoint{};

    bool hasDirLight = false;
    DirectionalLightComponent dirLight{};

    bool hasPointLight = false;
    PointLightComponent pointLight{};

    bool hasSkyLight = false;
    SkyLightComponent skyLight{};

    bool hasSpotLight = false;
    SpotLightComponent spotLight{};
};

StagedComponents& SceneObject::EnsureStaged() {
    if (!m_Staged) {
        m_Staged = std::make_unique<StagedComponents>();
    }
    return *m_Staged;
}

//============================================================================
// CONSTRUCTION / DESTRUCTION
//============================================================================

SceneObject::SceneObject(const std::string& name)
    : name_(name)
    , active_(true)
    , parent_(nullptr)
    , initialized_(false)
    , m_ECSEntity(0)
    , m_Coordinator(nullptr)
{
    entityID_ = IDPool::Get().RequestID();
}

SceneObject::SceneObject(const std::string& name, Entity entity, Coordinator* coordinator)
    : name_(name)
    , active_(true)
    , parent_(nullptr)
    , initialized_(false)
    , m_ECSEntity(entity)
    , m_Coordinator(coordinator)
{
    entityID_ = (entity != 0) ? entity : IDPool::Get().RequestID();
    if (m_Coordinator && m_ECSEntity != 0) {
        transform.BindECS(m_Coordinator, m_ECSEntity);
    }
}

SceneObject::~SceneObject() {
    transform.UnbindECS();
    Cleanup();

    if (parent_) {
        parent_->RemoveChild(this);
    }

    children_.clear();
    if (entityID_ != 0) {
        IDPool::Get().RecycleID(entityID_);
    }
}

void SceneObject::BindECS(Coordinator* coordinator, Entity entity) {
    m_Coordinator = coordinator;
    m_ECSEntity = entity;
    entityID_ = static_cast<uint32_t>(entity);
    if (m_Coordinator && m_ECSEntity != 0) {
        transform.BindECS(m_Coordinator, m_ECSEntity);
    }
}

void SceneObject::SetCoordinator(Coordinator* coordinator) {
    m_Coordinator = coordinator;
    if (m_Coordinator && m_ECSEntity != 0) {
        transform.BindECS(m_Coordinator, m_ECSEntity);
    }
}

void SceneObject::SetECSEntity(Entity entity) {
    m_ECSEntity = entity;
    entityID_ = static_cast<uint32_t>(entity);
    if (m_Coordinator && m_ECSEntity != 0) {
        transform.BindECS(m_Coordinator, m_ECSEntity);
    }
}

void SceneObject::InitializeWithECS(Coordinator& coordinator) {
    m_Coordinator = &coordinator;

    if (m_ECSEntity == 0 || !coordinator.IsEntityAlive(m_ECSEntity)) {
        m_ECSEntity = coordinator.CreateEntity();
        entityID_ = m_ECSEntity;
    }

    transform.BindECS(m_Coordinator, m_ECSEntity);

    // 1. Add Name component
    if (!coordinator.HasComponent<NameComponent>(m_ECSEntity)) {
        coordinator.AddComponent(m_ECSEntity, NameComponent(name_));
    }

    // 2. Add Transform component
    if (!coordinator.HasComponent<TransformComponent>(m_ECSEntity)) {
        TransformComponent tc;
        tc.position = transform.GetPosition();
        tc.rotation = transform.GetRotation();
        tc.scale = transform.GetScale();
        coordinator.AddComponent(m_ECSEntity, tc);
    }

    // 3. Add Tag and Layer
    if (!coordinator.HasComponent<TagComponent>(m_ECSEntity)) {
        coordinator.AddComponent(m_ECSEntity, TagComponent(name_));
    }
    if (!coordinator.HasComponent<LayerComponent>(m_ECSEntity)) {
        coordinator.AddComponent(m_ECSEntity, LayerComponent());
    }

    // 4. Commit any staged components
    if (m_Staged) {
        if (m_Staged->hasMesh) {
            coordinator.AddComponent(m_ECSEntity, RenderableMeshComponent(m_Staged->meshHandle));
            coordinator.AddComponent(m_ECSEntity, MeshRendererComponent());
            BoundsComponent bc;
            bc.localMin = { -0.5f, -0.5f, -0.5f };
            bc.localMax = {  0.5f,  0.5f,  0.5f };
            bc.dirty    = true;
            coordinator.AddComponent(m_ECSEntity, bc);
        }
        if (m_Staged->hasMaterial) {
            coordinator.AddComponent(m_ECSEntity, MaterialComponent(m_Staged->materialHandle));
        }
        if (m_Staged->hasStaticBody) {
            coordinator.AddComponent(m_ECSEntity, m_Staged->staticBody);
        }
        if (m_Staged->hasBoxCollider) {
            coordinator.AddComponent(m_ECSEntity, m_Staged->boxCollider);
        }
        if (m_Staged->hasSphereCollider) {
            coordinator.AddComponent(m_ECSEntity, m_Staged->sphereCollider);
        }
        if (m_Staged->hasCapsuleCollider) {
            coordinator.AddComponent(m_ECSEntity, m_Staged->capsuleCollider);
        }
        if (m_Staged->hasPlayerStart) {
            coordinator.AddComponent(m_ECSEntity, m_Staged->playerStart);
        }
        if (m_Staged->hasCharacterController) {
            coordinator.AddComponent(m_ECSEntity, m_Staged->characterController);
        }
        if (m_Staged->hasCamera) {
            coordinator.AddComponent(m_ECSEntity, m_Staged->camera);
        }
        if (m_Staged->hasInput) {
            coordinator.AddComponent(m_ECSEntity, m_Staged->input);
        }
        if (m_Staged->hasTrigger) {
            coordinator.AddComponent(m_ECSEntity, m_Staged->trigger);
        }
        if (m_Staged->hasInteractable) {
            coordinator.AddComponent(m_ECSEntity, m_Staged->interactable);
        }
        if (m_Staged->hasObjective) {
            coordinator.AddComponent(m_ECSEntity, m_Staged->objective);
        }
        if (m_Staged->hasAudioSource) {
            coordinator.AddComponent(m_ECSEntity, m_Staged->audioSource);
        }
        if (m_Staged->hasSimpleState) {
            coordinator.AddComponent(m_ECSEntity, m_Staged->simpleState);
        }
        if (m_Staged->hasActivatable) {
            coordinator.AddComponent(m_ECSEntity, m_Staged->activatable);
        }
        if (m_Staged->hasDoor) {
            coordinator.AddComponent(m_ECSEntity, m_Staged->door);
        }
        if (m_Staged->hasCheckpoint) {
            coordinator.AddComponent(m_ECSEntity, m_Staged->checkpoint);
        }
        if (m_Staged->hasDirLight) {
            coordinator.AddComponent(m_ECSEntity, m_Staged->dirLight);
        }
        if (m_Staged->hasPointLight) {
            coordinator.AddComponent(m_ECSEntity, m_Staged->pointLight);
        }
        if (m_Staged->hasSkyLight) {
            coordinator.AddComponent(m_ECSEntity, m_Staged->skyLight);
        }
        if (m_Staged->hasSpotLight) {
            coordinator.AddComponent(m_ECSEntity, m_Staged->spotLight);
        }
        m_Staged.reset();
    }
}

//============================================================================
// COMPONENT HELPERS IMPLEMENTATION
//============================================================================

bool SceneObject::HasRenderableMesh() const {
    if (m_Coordinator && m_ECSEntity != 0) return m_Coordinator->HasComponent<RenderableMeshComponent>(m_ECSEntity);
    return m_Staged ? m_Staged->hasMesh : false;
}

void SceneObject::SetRenderableMesh(AssetHandle handle) {
    if (m_Coordinator && m_ECSEntity != 0) {
        m_Coordinator->AddComponent(m_ECSEntity, RenderableMeshComponent(handle));
        m_Coordinator->AddComponent(m_ECSEntity, MeshRendererComponent());
        BoundsComponent bc;
        bc.localMin = { -0.5f, -0.5f, -0.5f };
        bc.localMax = {  0.5f,  0.5f,  0.5f };
        bc.dirty    = true;
        m_Coordinator->AddComponent(m_ECSEntity, bc);
    } else {
        auto& s = EnsureStaged();
        s.hasMesh = true;
        s.meshHandle = handle;
    }
}

void SceneObject::ClearRenderableMesh() {
    if (m_Coordinator && m_ECSEntity != 0) {
        m_Coordinator->RemoveComponent<RenderableMeshComponent>(m_ECSEntity);
        m_Coordinator->RemoveComponent<MeshRendererComponent>(m_ECSEntity);
    } else if (m_Staged) {
        m_Staged->hasMesh = false;
    }
}

AssetHandle SceneObject::GetMeshAssetHandle() const {
    if (m_Coordinator && m_ECSEntity != 0 && m_Coordinator->HasComponent<RenderableMeshComponent>(m_ECSEntity)) {
        return m_Coordinator->GetComponent<RenderableMeshComponent>(m_ECSEntity).meshAssetHandle;
    }
    return m_Staged ? m_Staged->meshHandle : AssetHandle{0};
}

bool SceneObject::HasMaterial() const {
    if (m_Coordinator && m_ECSEntity != 0) return m_Coordinator->HasComponent<MaterialComponent>(m_ECSEntity);
    return m_Staged ? m_Staged->hasMaterial : false;
}

void SceneObject::SetMaterial(AssetHandle handle) {
    if (m_Coordinator && m_ECSEntity != 0) {
        m_Coordinator->AddComponent(m_ECSEntity, MaterialComponent(handle));
    } else {
        auto& s = EnsureStaged();
        s.hasMaterial = true;
        s.materialHandle = handle;
    }
}

void SceneObject::ClearMaterial() {
    if (m_Coordinator && m_ECSEntity != 0) {
        m_Coordinator->RemoveComponent<MaterialComponent>(m_ECSEntity);
    } else if (m_Staged) {
        m_Staged->hasMaterial = false;
    }
}

AssetHandle SceneObject::GetMaterialAssetHandle() const {
    if (m_Coordinator && m_ECSEntity != 0 && m_Coordinator->HasComponent<MaterialComponent>(m_ECSEntity)) {
        return m_Coordinator->GetComponent<MaterialComponent>(m_ECSEntity).materialAssetHandle;
    }
    return m_Staged ? m_Staged->materialHandle : AssetHandle{0};
}

// StaticBody
bool SceneObject::HasStaticBody() const {
    if (m_Coordinator && m_ECSEntity != 0) return m_Coordinator->HasComponent<StaticBodyComponent>(m_ECSEntity);
    return m_Staged ? m_Staged->hasStaticBody : false;
}
void SceneObject::SetStaticBody(const StaticBodyComponent& comp) {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->AddComponent(m_ECSEntity, comp);
    else { auto& s = EnsureStaged(); s.hasStaticBody = true; s.staticBody = comp; }
}
void SceneObject::ClearStaticBody() {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->RemoveComponent<StaticBodyComponent>(m_ECSEntity);
    else if (m_Staged) m_Staged->hasStaticBody = false;
}
const StaticBodyComponent& SceneObject::GetStaticBody() const {
    static StaticBodyComponent s_default{};
    if (m_Coordinator && m_ECSEntity != 0 && m_Coordinator->HasComponent<StaticBodyComponent>(m_ECSEntity)) {
        return m_Coordinator->GetComponent<StaticBodyComponent>(m_ECSEntity);
    }
    return m_Staged ? m_Staged->staticBody : s_default;
}

// BoxCollider
bool SceneObject::HasBoxCollider() const {
    if (m_Coordinator && m_ECSEntity != 0) return m_Coordinator->HasComponent<BoxColliderComponent>(m_ECSEntity);
    return m_Staged ? m_Staged->hasBoxCollider : false;
}
void SceneObject::SetBoxCollider(const BoxColliderComponent& comp) {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->AddComponent(m_ECSEntity, comp);
    else { auto& s = EnsureStaged(); s.hasBoxCollider = true; s.boxCollider = comp; }
}
void SceneObject::ClearBoxCollider() {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->RemoveComponent<BoxColliderComponent>(m_ECSEntity);
    else if (m_Staged) m_Staged->hasBoxCollider = false;
}
const BoxColliderComponent& SceneObject::GetBoxCollider() const {
    static BoxColliderComponent s_default{};
    if (m_Coordinator && m_ECSEntity != 0 && m_Coordinator->HasComponent<BoxColliderComponent>(m_ECSEntity)) {
        return m_Coordinator->GetComponent<BoxColliderComponent>(m_ECSEntity);
    }
    return m_Staged ? m_Staged->boxCollider : s_default;
}

// SphereCollider
bool SceneObject::HasSphereCollider() const {
    if (m_Coordinator && m_ECSEntity != 0) return m_Coordinator->HasComponent<SphereColliderComponent>(m_ECSEntity);
    return m_Staged ? m_Staged->hasSphereCollider : false;
}
void SceneObject::SetSphereCollider(const SphereColliderComponent& comp) {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->AddComponent(m_ECSEntity, comp);
    else { auto& s = EnsureStaged(); s.hasSphereCollider = true; s.sphereCollider = comp; }
}
void SceneObject::ClearSphereCollider() {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->RemoveComponent<SphereColliderComponent>(m_ECSEntity);
    else if (m_Staged) m_Staged->hasSphereCollider = false;
}
const SphereColliderComponent& SceneObject::GetSphereCollider() const {
    static SphereColliderComponent s_default{};
    if (m_Coordinator && m_ECSEntity != 0 && m_Coordinator->HasComponent<SphereColliderComponent>(m_ECSEntity)) {
        return m_Coordinator->GetComponent<SphereColliderComponent>(m_ECSEntity);
    }
    return m_Staged ? m_Staged->sphereCollider : s_default;
}

// CapsuleCollider
bool SceneObject::HasCapsuleCollider() const {
    if (m_Coordinator && m_ECSEntity != 0) return m_Coordinator->HasComponent<CapsuleColliderComponent>(m_ECSEntity);
    return m_Staged ? m_Staged->hasCapsuleCollider : false;
}
void SceneObject::SetCapsuleCollider(const CapsuleColliderComponent& comp) {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->AddComponent(m_ECSEntity, comp);
    else { auto& s = EnsureStaged(); s.hasCapsuleCollider = true; s.capsuleCollider = comp; }
}
void SceneObject::ClearCapsuleCollider() {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->RemoveComponent<CapsuleColliderComponent>(m_ECSEntity);
    else if (m_Staged) m_Staged->hasCapsuleCollider = false;
}
const CapsuleColliderComponent& SceneObject::GetCapsuleCollider() const {
    static CapsuleColliderComponent s_default{};
    if (m_Coordinator && m_ECSEntity != 0 && m_Coordinator->HasComponent<CapsuleColliderComponent>(m_ECSEntity)) {
        return m_Coordinator->GetComponent<CapsuleColliderComponent>(m_ECSEntity);
    }
    return m_Staged ? m_Staged->capsuleCollider : s_default;
}

// PlayerStart
bool SceneObject::HasPlayerStart() const {
    if (m_Coordinator && m_ECSEntity != 0) return m_Coordinator->HasComponent<PlayerStartComponent>(m_ECSEntity);
    return m_Staged ? m_Staged->hasPlayerStart : false;
}
void SceneObject::SetPlayerStart(const PlayerStartComponent& comp) {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->AddComponent(m_ECSEntity, comp);
    else { auto& s = EnsureStaged(); s.hasPlayerStart = true; s.playerStart = comp; }
}
void SceneObject::ClearPlayerStart() {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->RemoveComponent<PlayerStartComponent>(m_ECSEntity);
    else if (m_Staged) m_Staged->hasPlayerStart = false;
}
const PlayerStartComponent& SceneObject::GetPlayerStart() const {
    static PlayerStartComponent s_default{};
    if (m_Coordinator && m_ECSEntity != 0 && m_Coordinator->HasComponent<PlayerStartComponent>(m_ECSEntity)) {
        return m_Coordinator->GetComponent<PlayerStartComponent>(m_ECSEntity);
    }
    return m_Staged ? m_Staged->playerStart : s_default;
}

// CharacterController
bool SceneObject::HasCharacterController() const {
    if (m_Coordinator && m_ECSEntity != 0) return m_Coordinator->HasComponent<CharacterControllerComponent>(m_ECSEntity);
    return m_Staged ? m_Staged->hasCharacterController : false;
}
void SceneObject::SetCharacterController(const CharacterControllerComponent& comp) {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->AddComponent(m_ECSEntity, comp);
    else { auto& s = EnsureStaged(); s.hasCharacterController = true; s.characterController = comp; }
}
void SceneObject::ClearCharacterController() {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->RemoveComponent<CharacterControllerComponent>(m_ECSEntity);
    else if (m_Staged) m_Staged->hasCharacterController = false;
}
const CharacterControllerComponent& SceneObject::GetCharacterController() const {
    static CharacterControllerComponent s_default{};
    if (m_Coordinator && m_ECSEntity != 0 && m_Coordinator->HasComponent<CharacterControllerComponent>(m_ECSEntity)) {
        return m_Coordinator->GetComponent<CharacterControllerComponent>(m_ECSEntity);
    }
    return m_Staged ? m_Staged->characterController : s_default;
}

// CameraComponent
bool SceneObject::HasCameraComponent() const {
    if (m_Coordinator && m_ECSEntity != 0) return m_Coordinator->HasComponent<CameraComponent>(m_ECSEntity);
    return m_Staged ? m_Staged->hasCamera : false;
}
void SceneObject::SetCameraComponent(const CameraComponent& comp) {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->AddComponent(m_ECSEntity, comp);
    else { auto& s = EnsureStaged(); s.hasCamera = true; s.camera = comp; }
}
void SceneObject::ClearCameraComponent() {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->RemoveComponent<CameraComponent>(m_ECSEntity);
    else if (m_Staged) m_Staged->hasCamera = false;
}
const CameraComponent& SceneObject::GetCameraComponent() const {
    static CameraComponent s_default{};
    if (m_Coordinator && m_ECSEntity != 0 && m_Coordinator->HasComponent<CameraComponent>(m_ECSEntity)) {
        return m_Coordinator->GetComponent<CameraComponent>(m_ECSEntity);
    }
    return m_Staged ? m_Staged->camera : s_default;
}

// InputComponent
bool SceneObject::HasInputComponent() const {
    if (m_Coordinator && m_ECSEntity != 0) return m_Coordinator->HasComponent<InputComponent>(m_ECSEntity);
    return m_Staged ? m_Staged->hasInput : false;
}
void SceneObject::SetInputComponent(const InputComponent& comp) {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->AddComponent(m_ECSEntity, comp);
    else { auto& s = EnsureStaged(); s.hasInput = true; s.input = comp; }
}
void SceneObject::ClearInputComponent() {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->RemoveComponent<InputComponent>(m_ECSEntity);
    else if (m_Staged) m_Staged->hasInput = false;
}
const InputComponent& SceneObject::GetInputComponent() const {
    static InputComponent s_default{};
    if (m_Coordinator && m_ECSEntity != 0 && m_Coordinator->HasComponent<InputComponent>(m_ECSEntity)) {
        return m_Coordinator->GetComponent<InputComponent>(m_ECSEntity);
    }
    return m_Staged ? m_Staged->input : s_default;
}

// TriggerComponent
bool SceneObject::HasTrigger() const {
    if (m_Coordinator && m_ECSEntity != 0) return m_Coordinator->HasComponent<TriggerComponent>(m_ECSEntity);
    return m_Staged ? m_Staged->hasTrigger : false;
}
void SceneObject::SetTrigger(const TriggerComponent& comp) {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->AddComponent(m_ECSEntity, comp);
    else { auto& s = EnsureStaged(); s.hasTrigger = true; s.trigger = comp; }
}
void SceneObject::ClearTrigger() {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->RemoveComponent<TriggerComponent>(m_ECSEntity);
    else if (m_Staged) m_Staged->hasTrigger = false;
}
const TriggerComponent& SceneObject::GetTrigger() const {
    static TriggerComponent s_default{};
    if (m_Coordinator && m_ECSEntity != 0 && m_Coordinator->HasComponent<TriggerComponent>(m_ECSEntity)) {
        return m_Coordinator->GetComponent<TriggerComponent>(m_ECSEntity);
    }
    return m_Staged ? m_Staged->trigger : s_default;
}

// InteractableComponent
bool SceneObject::HasInteractable() const {
    if (m_Coordinator && m_ECSEntity != 0) return m_Coordinator->HasComponent<InteractableComponent>(m_ECSEntity);
    return m_Staged ? m_Staged->hasInteractable : false;
}
void SceneObject::SetInteractable(const InteractableComponent& comp) {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->AddComponent(m_ECSEntity, comp);
    else { auto& s = EnsureStaged(); s.hasInteractable = true; s.interactable = comp; }
}
void SceneObject::ClearInteractable() {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->RemoveComponent<InteractableComponent>(m_ECSEntity);
    else if (m_Staged) m_Staged->hasInteractable = false;
}
const InteractableComponent& SceneObject::GetInteractable() const {
    static InteractableComponent s_default{};
    if (m_Coordinator && m_ECSEntity != 0 && m_Coordinator->HasComponent<InteractableComponent>(m_ECSEntity)) {
        return m_Coordinator->GetComponent<InteractableComponent>(m_ECSEntity);
    }
    return m_Staged ? m_Staged->interactable : s_default;
}

// ObjectiveComponent
bool SceneObject::HasObjective() const {
    if (m_Coordinator && m_ECSEntity != 0) return m_Coordinator->HasComponent<ObjectiveComponent>(m_ECSEntity);
    return m_Staged ? m_Staged->hasObjective : false;
}
void SceneObject::SetObjective(const ObjectiveComponent& comp) {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->AddComponent(m_ECSEntity, comp);
    else { auto& s = EnsureStaged(); s.hasObjective = true; s.objective = comp; }
}
void SceneObject::ClearObjective() {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->RemoveComponent<ObjectiveComponent>(m_ECSEntity);
    else if (m_Staged) m_Staged->hasObjective = false;
}
const ObjectiveComponent& SceneObject::GetObjective() const {
    static ObjectiveComponent s_default{};
    if (m_Coordinator && m_ECSEntity != 0 && m_Coordinator->HasComponent<ObjectiveComponent>(m_ECSEntity)) {
        return m_Coordinator->GetComponent<ObjectiveComponent>(m_ECSEntity);
    }
    return m_Staged ? m_Staged->objective : s_default;
}

// AudioSourceComponent
bool SceneObject::HasAudioSource() const {
    if (m_Coordinator && m_ECSEntity != 0) return m_Coordinator->HasComponent<AudioSourceComponent>(m_ECSEntity);
    return m_Staged ? m_Staged->hasAudioSource : false;
}
void SceneObject::SetAudioSource(const AudioSourceComponent& comp) {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->AddComponent(m_ECSEntity, comp);
    else { auto& s = EnsureStaged(); s.hasAudioSource = true; s.audioSource = comp; }
}
void SceneObject::ClearAudioSource() {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->RemoveComponent<AudioSourceComponent>(m_ECSEntity);
    else if (m_Staged) m_Staged->hasAudioSource = false;
}
const AudioSourceComponent& SceneObject::GetAudioSource() const {
    static AudioSourceComponent s_default{};
    if (m_Coordinator && m_ECSEntity != 0 && m_Coordinator->HasComponent<AudioSourceComponent>(m_ECSEntity)) {
        return m_Coordinator->GetComponent<AudioSourceComponent>(m_ECSEntity);
    }
    return m_Staged ? m_Staged->audioSource : s_default;
}

// SimpleStateComponent
bool SceneObject::HasSimpleState() const {
    if (m_Coordinator && m_ECSEntity != 0) return m_Coordinator->HasComponent<SimpleStateComponent>(m_ECSEntity);
    return m_Staged ? m_Staged->hasSimpleState : false;
}
void SceneObject::SetSimpleState(const SimpleStateComponent& comp) {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->AddComponent(m_ECSEntity, comp);
    else { auto& s = EnsureStaged(); s.hasSimpleState = true; s.simpleState = comp; }
}
void SceneObject::ClearSimpleState() {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->RemoveComponent<SimpleStateComponent>(m_ECSEntity);
    else if (m_Staged) m_Staged->hasSimpleState = false;
}
const SimpleStateComponent& SceneObject::GetSimpleState() const {
    static SimpleStateComponent s_default{};
    if (m_Coordinator && m_ECSEntity != 0 && m_Coordinator->HasComponent<SimpleStateComponent>(m_ECSEntity)) {
        return m_Coordinator->GetComponent<SimpleStateComponent>(m_ECSEntity);
    }
    return m_Staged ? m_Staged->simpleState : s_default;
}

// ActivatableComponent
bool SceneObject::HasActivatable() const {
    if (m_Coordinator && m_ECSEntity != 0) return m_Coordinator->HasComponent<ActivatableComponent>(m_ECSEntity);
    return m_Staged ? m_Staged->hasActivatable : false;
}
void SceneObject::SetActivatable(const ActivatableComponent& comp) {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->AddComponent(m_ECSEntity, comp);
    else { auto& s = EnsureStaged(); s.hasActivatable = true; s.activatable = comp; }
}
void SceneObject::ClearActivatable() {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->RemoveComponent<ActivatableComponent>(m_ECSEntity);
    else if (m_Staged) m_Staged->hasActivatable = false;
}
const ActivatableComponent& SceneObject::GetActivatable() const {
    static ActivatableComponent s_default{};
    if (m_Coordinator && m_ECSEntity != 0 && m_Coordinator->HasComponent<ActivatableComponent>(m_ECSEntity)) {
        return m_Coordinator->GetComponent<ActivatableComponent>(m_ECSEntity);
    }
    return m_Staged ? m_Staged->activatable : s_default;
}

// DoorComponent
bool SceneObject::HasDoor() const {
    if (m_Coordinator && m_ECSEntity != 0) return m_Coordinator->HasComponent<DoorComponent>(m_ECSEntity);
    return m_Staged ? m_Staged->hasDoor : false;
}
void SceneObject::SetDoor(const DoorComponent& comp) {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->AddComponent(m_ECSEntity, comp);
    else { auto& s = EnsureStaged(); s.hasDoor = true; s.door = comp; }
}
void SceneObject::ClearDoor() {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->RemoveComponent<DoorComponent>(m_ECSEntity);
    else if (m_Staged) m_Staged->hasDoor = false;
}
const DoorComponent& SceneObject::GetDoor() const {
    static DoorComponent s_default{};
    if (m_Coordinator && m_ECSEntity != 0 && m_Coordinator->HasComponent<DoorComponent>(m_ECSEntity)) {
        return m_Coordinator->GetComponent<DoorComponent>(m_ECSEntity);
    }
    return m_Staged ? m_Staged->door : s_default;
}

// CheckpointComponent
bool SceneObject::HasCheckpoint() const {
    if (m_Coordinator && m_ECSEntity != 0) return m_Coordinator->HasComponent<CheckpointComponent>(m_ECSEntity);
    return m_Staged ? m_Staged->hasCheckpoint : false;
}
void SceneObject::SetCheckpoint(const CheckpointComponent& comp) {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->AddComponent(m_ECSEntity, comp);
    else { auto& s = EnsureStaged(); s.hasCheckpoint = true; s.checkpoint = comp; }
}
void SceneObject::ClearCheckpoint() {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->RemoveComponent<CheckpointComponent>(m_ECSEntity);
    else if (m_Staged) m_Staged->hasCheckpoint = false;
}
const CheckpointComponent& SceneObject::GetCheckpoint() const {
    static CheckpointComponent s_default{};
    if (m_Coordinator && m_ECSEntity != 0 && m_Coordinator->HasComponent<CheckpointComponent>(m_ECSEntity)) {
        return m_Coordinator->GetComponent<CheckpointComponent>(m_ECSEntity);
    }
    return m_Staged ? m_Staged->checkpoint : s_default;
}

// DirectionalLightComponent
bool SceneObject::HasDirectionalLight() const {
    if (m_Coordinator && m_ECSEntity != 0) return m_Coordinator->HasComponent<DirectionalLightComponent>(m_ECSEntity);
    return m_Staged ? m_Staged->hasDirLight : false;
}
void SceneObject::SetDirectionalLight(const DirectionalLightComponent& comp) {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->AddComponent(m_ECSEntity, comp);
    else { auto& s = EnsureStaged(); s.hasDirLight = true; s.dirLight = comp; }
}
void SceneObject::ClearDirectionalLight() {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->RemoveComponent<DirectionalLightComponent>(m_ECSEntity);
    else if (m_Staged) m_Staged->hasDirLight = false;
}
const DirectionalLightComponent& SceneObject::GetDirectionalLight() const {
    static DirectionalLightComponent s_default{};
    if (m_Coordinator && m_ECSEntity != 0 && m_Coordinator->HasComponent<DirectionalLightComponent>(m_ECSEntity)) {
        return m_Coordinator->GetComponent<DirectionalLightComponent>(m_ECSEntity);
    }
    return m_Staged ? m_Staged->dirLight : s_default;
}

// PointLightComponent
bool SceneObject::HasPointLight() const {
    if (m_Coordinator && m_ECSEntity != 0) return m_Coordinator->HasComponent<PointLightComponent>(m_ECSEntity);
    return m_Staged ? m_Staged->hasPointLight : false;
}
void SceneObject::SetPointLight(const PointLightComponent& comp) {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->AddComponent(m_ECSEntity, comp);
    else { auto& s = EnsureStaged(); s.hasPointLight = true; s.pointLight = comp; }
}
void SceneObject::ClearPointLight() {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->RemoveComponent<PointLightComponent>(m_ECSEntity);
    else if (m_Staged) m_Staged->hasPointLight = false;
}
const PointLightComponent& SceneObject::GetPointLight() const {
    static PointLightComponent s_default{};
    if (m_Coordinator && m_ECSEntity != 0 && m_Coordinator->HasComponent<PointLightComponent>(m_ECSEntity)) {
        return m_Coordinator->GetComponent<PointLightComponent>(m_ECSEntity);
    }
    return m_Staged ? m_Staged->pointLight : s_default;
}

// SkyLightComponent
bool SceneObject::HasSkyLight() const {
    if (m_Coordinator && m_ECSEntity != 0) return m_Coordinator->HasComponent<SkyLightComponent>(m_ECSEntity);
    return m_Staged ? m_Staged->hasSkyLight : false;
}
void SceneObject::SetSkyLight(const SkyLightComponent& comp) {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->AddComponent(m_ECSEntity, comp);
    else { auto& s = EnsureStaged(); s.hasSkyLight = true; s.skyLight = comp; }
}
void SceneObject::ClearSkyLight() {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->RemoveComponent<SkyLightComponent>(m_ECSEntity);
    else if (m_Staged) m_Staged->hasSkyLight = false;
}
const SkyLightComponent& SceneObject::GetSkyLight() const {
    static SkyLightComponent s_default{};
    if (m_Coordinator && m_ECSEntity != 0 && m_Coordinator->HasComponent<SkyLightComponent>(m_ECSEntity)) {
        return m_Coordinator->GetComponent<SkyLightComponent>(m_ECSEntity);
    }
    return m_Staged ? m_Staged->skyLight : s_default;
}

// SpotLightComponent
bool SceneObject::HasSpotLight() const {
    if (m_Coordinator && m_ECSEntity != 0) return m_Coordinator->HasComponent<SpotLightComponent>(m_ECSEntity);
    return m_Staged ? m_Staged->hasSpotLight : false;
}
void SceneObject::SetSpotLight(const SpotLightComponent& comp) {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->AddComponent(m_ECSEntity, comp);
    else { auto& s = EnsureStaged(); s.hasSpotLight = true; s.spotLight = comp; }
}
void SceneObject::ClearSpotLight() {
    if (m_Coordinator && m_ECSEntity != 0) m_Coordinator->RemoveComponent<SpotLightComponent>(m_ECSEntity);
    else if (m_Staged) m_Staged->hasSpotLight = false;
}
const SpotLightComponent& SceneObject::GetSpotLight() const {
    static SpotLightComponent s_default{};
    if (m_Coordinator && m_ECSEntity != 0 && m_Coordinator->HasComponent<SpotLightComponent>(m_ECSEntity)) {
        return m_Coordinator->GetComponent<SpotLightComponent>(m_ECSEntity);
    }
    return m_Staged ? m_Staged->spotLight : s_default;
}

//============================================================================
// CORE ALGORITHM: Update
//============================================================================

void SceneObject::Update(float deltaTime) {
    if (!active_) return;

    UpdateTransformHierarchy();
    UpdateChildren(deltaTime);
}

//============================================================================
// CORE ALGORITHM: AddChild
//============================================================================

void SceneObject::AddChild(SceneObject* child) {
    if (!child || child == this) return;

    auto it = std::find(children_.begin(), children_.end(), child);
    if (it != children_.end()) return;

    if (child->parent_) {
        child->parent_->RemoveChild(child);
    }

    child->parent_ = this;
    children_.push_back(child);
    child->RecomputeTransformInheritance();
    child->RebindEntityIDHierarchy();
}

void SceneObject::RemoveChild(SceneObject* child) {
    if (!child) return;

    auto it = std::find(children_.begin(), children_.end(), child);
    if (it != children_.end()) {
        children_.erase(it);
        child->parent_ = nullptr;
        child->RecomputeTransformInheritance();
    }
}

void SceneObject::SetParent(SceneObject* newParent) {
    if (newParent == parent_) return;

    if (parent_) {
        parent_->RemoveChild(this);
    }

    parent_ = newParent;

    if (newParent) {
        newParent->AddChild(this);
    }
}

SceneObject* SceneObject::GetParent() const {
    return parent_;
}

const std::vector<SceneObject*>& SceneObject::GetChildren() const {
    return children_;
}

bool SceneObject::HasChildren() const {
    return !children_.empty();
}

size_t SceneObject::GetChildCount() const {
    return children_.size();
}

SceneObject* SceneObject::FindChild(const std::string& name) const {
    for (SceneObject* child : children_) {
        if (child && child->GetName() == name) {
            return child;
        }
    }
    return nullptr;
}

//============================================================================
// OBJECT PROPERTIES
//============================================================================

uint32_t SceneObject::GetID() const {
    return entityID_;
}

void SceneObject::SetID(uint32_t id) {
    if (entityID_ != 0 && entityID_ != id) {
        IDPool::Get().RecycleID(entityID_);
    }
    entityID_ = id;
    m_ECSEntity = id;
}

const std::string& SceneObject::GetName() const {
    return name_;
}

void SceneObject::SetName(const std::string& name) {
    name_ = name;
    if (m_Coordinator && m_ECSEntity != 0 && m_Coordinator->HasComponent<NameComponent>(m_ECSEntity)) {
        m_Coordinator->GetComponent<NameComponent>(m_ECSEntity).name = name;
    }
}

bool SceneObject::IsActive() const {
    return active_;
}

void SceneObject::SetActive(bool active) {
    active_ = active;
}

//============================================================================
// LIFECYCLE
//============================================================================

void SceneObject::Initialize() {
    if (initialized_) return;

    for (auto* child : children_) {
        if (child) {
            child->Initialize();
        }
    }

    initialized_ = true;
}

void SceneObject::Cleanup() {
    if (!initialized_) return;

    for (auto* child : children_) {
        if (child) {
            child->Cleanup();
        }
    }

    initialized_ = false;
}

//============================================================================
// INTERNAL HELPERS
//============================================================================

void SceneObject::UpdateTransformHierarchy() {
    if (parent_) {
        transform.UpdateWorldTransform(&parent_->transform);
    } else {
        transform.UpdateWorldTransform(nullptr);
    }
}

void SceneObject::UpdateChildren(float deltaTime) {
    for (auto* child : children_) {
        if (child && child->IsActive()) {
            child->Update(deltaTime);
        }
    }
}

void SceneObject::RecomputeTransformInheritance() {
    UpdateTransformHierarchy();

    for (auto* child : children_) {
        if (child) {
            child->RecomputeTransformInheritance();
        }
    }
}

void SceneObject::RebindEntityIDHierarchy() {
    for (auto* child : children_) {
        if (child) {
            child->RebindEntityIDHierarchy();
        }
    }
}