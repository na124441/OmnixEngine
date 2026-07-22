//============================================================================
// SceneObject.cpp - Complete with ECS Integration
//============================================================================

#include "Scene/SceneObject.h"
#include "Scene/IDPool.h"
#include "../ECS/Coordinator.h"
#include "../ECS/ECSComponents.h"
#include "ECS/EntityHierarchySystem.h"
#include "Runtime/World/GroundSectionComponent.h"
#include <iostream>
//============================================================================
// ✅ NEW: ECS INTEGRATION
//============================================================================

void SceneObject::InitializeWithECS(Coordinator& coordinator) {
    if (m_ECSEntity != 0 && coordinator.IsEntityAlive(m_ECSEntity)) {
        std::cout << "[SceneObject] '" << name_
                  << "' already bound to ECS Entity " << m_ECSEntity << std::endl;
        return;
    }

    std::cout << "[SceneObject] Initializing '" << name_
              << "' with ECS..." << std::endl;

    // Create ECS entity
    m_ECSEntity = coordinator.CreateEntity();

    // 1. Add Name component
    coordinator.AddComponent(m_ECSEntity, NameComponent(name_));

    // 2. Add Transform component
    TransformComponent tc;
    tc.position = transform.GetPosition();
    tc.rotation = transform.GetRotation();
    tc.scale = transform.GetScale();
    coordinator.AddComponent(m_ECSEntity, tc);

    // 3. Add other core components
    coordinator.AddComponent(m_ECSEntity, TagComponent(name_));
    coordinator.AddComponent(m_ECSEntity, LayerComponent());

    // 4. Add RenderableMeshComponent if assigned
    if (m_HasRenderableMesh) {
        coordinator.AddComponent(m_ECSEntity, RenderableMeshComponent(m_MeshAssetHandle));
        coordinator.AddComponent(m_ECSEntity, MeshRendererComponent());

        // Attach a BoundsComponent so this renderable participates in the
        // bounds system. Local bounds default to a unit cube; the
        // BoundsUpdateSystem will compute world-space bounds each frame.
        BoundsComponent bc;
        bc.localMin = { -0.5f, -0.5f, -0.5f };
        bc.localMax = {  0.5f,  0.5f,  0.5f };
        bc.dirty    = true;
        coordinator.AddComponent(m_ECSEntity, bc);
    }

    // 5. Add MaterialComponent if assigned
    if (m_HasMaterial) {
        coordinator.AddComponent(m_ECSEntity, MaterialComponent(m_MaterialAssetHandle));
    }

    // 6. Add StaticBodyComponent if assigned
    if (m_HasStaticBody) {
        coordinator.AddComponent(m_ECSEntity, m_StaticBody);
    }

    // Add GroundSectionComponent if assigned
    if (m_HasGroundSection) {
        coordinator.AddComponent(m_ECSEntity, m_GroundSection);
    }

    // 7. Add BoxColliderComponent if assigned
    if (m_HasBoxCollider) {
        coordinator.AddComponent(m_ECSEntity, m_BoxCollider);
    }

    // 8. Add SphereColliderComponent if assigned
    if (m_HasSphereCollider) {
        coordinator.AddComponent(m_ECSEntity, m_SphereCollider);
    }

    // 9. Add CapsuleColliderComponent if assigned
    if (m_HasCapsuleCollider) {
        coordinator.AddComponent(m_ECSEntity, m_CapsuleCollider);
    }

    // 10. Add PlayerStartComponent if assigned
    if (m_HasPlayerStart) {
        coordinator.AddComponent(m_ECSEntity, m_PlayerStart);
    }

    // 11. Add CharacterControllerComponent if assigned
    if (m_HasCharacterController) {
        coordinator.AddComponent(m_ECSEntity, m_CharacterController);
    }

    // 12. Add CameraComponent if assigned
    if (m_HasCameraComponent) {
        coordinator.AddComponent(m_ECSEntity, m_CameraComponent);
    }

    // 13. Add InputComponent if assigned
    if (m_HasInputComponent) {
        coordinator.AddComponent(m_ECSEntity, m_InputComponent);
    }

    // 14. Add TriggerComponent if assigned
    if (m_HasTrigger) {
        coordinator.AddComponent(m_ECSEntity, m_Trigger);
    }

    // 15. Add InteractableComponent if assigned
    if (m_HasInteractable) {
        coordinator.AddComponent(m_ECSEntity, m_Interactable);
    }

    // Add ObjectiveComponent if assigned
    if (m_HasObjective) {
        coordinator.AddComponent(m_ECSEntity, m_Objective);
    }

    // Add AudioSourceComponent if assigned
    if (m_HasAudioSource) {
        coordinator.AddComponent(m_ECSEntity, m_AudioSource);
    }

    if (m_HasSimpleState) {
        coordinator.AddComponent(m_ECSEntity, m_SimpleState);
    }
    if (m_HasActivatable) {
        coordinator.AddComponent(m_ECSEntity, m_Activatable);
    }
    if (m_HasDoor) {
        coordinator.AddComponent(m_ECSEntity, m_Door);
    }
    if (m_HasCheckpoint) {
        coordinator.AddComponent(m_ECSEntity, m_Checkpoint);
    }

    // 16. Add DirectionalLightComponent if assigned
    if (m_HasDirectionalLight) {
        coordinator.AddComponent(m_ECSEntity, m_DirectionalLight);
    }

    // 17. Add PointLightComponent if assigned
    if (m_HasPointLight) {
        coordinator.AddComponent(m_ECSEntity, m_PointLight);
    }

    // 18. Add SkyLightComponent if assigned
    if (m_HasSkyLight) {
        coordinator.AddComponent(m_ECSEntity, m_SkyLight);
    }

    // 19. Add SpotLightComponent if assigned
    if (m_HasSpotLight) {
        coordinator.AddComponent(m_ECSEntity, m_SpotLight);
    }

    // 20. Add ReflectionProbeComponent if assigned
    if (m_HasReflectionProbe) {
        coordinator.AddComponent(m_ECSEntity, m_ReflectionProbe);
    }

    // 5. Add HierarchyComponent if registered in coordinator
    if (coordinator.IsComponentRegistered<HierarchyComponent>()) {
        if (parent_) {
            eng::runtime::EntityHierarchySystem::AttachChild(parent_->GetID(), m_ECSEntity, coordinator);
        } else {
            coordinator.AddComponent(m_ECSEntity, HierarchyComponent(0xFFFFFFFF, 0));
        }
    }

    std::cout << "[SceneObject] '" << name_
              << "' registered (ECS Entity: " << m_ECSEntity << ")" << std::endl;
}

//============================================================================
// CONSTRUCTION / DESTRUCTION
//============================================================================

SceneObject::SceneObject(const std::string& name)
    : name_(name)
    , active_(true)
    , parent_(nullptr)
    , initialized_(false)
    , m_ECSEntity(0)  // ✅ ADDED
{
    entityID_ = IDPool::Get().RequestID();
    std::cout << "[SceneObject] Created: " << name_
              << " (ID: " << entityID_ << ")" << std::endl;
}

SceneObject::~SceneObject() {
    std::cout << "[SceneObject] Destroying: " << name_
              << " (ID: " << entityID_ << ")" << std::endl;

    Cleanup();

    if (parent_) {
        parent_->RemoveChild(this);
    }

    children_.clear();
    IDPool::Get().RecycleID(entityID_);
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
    if (!child) {
        std::cerr << "[SceneObject] WARNING: Attempted to add null child" << std::endl;
        return;
    }

    if (child == this) {
        std::cerr << "[SceneObject] ERROR: Cannot add self as child" << std::endl;
        return;
    }

    auto it = std::find(children_.begin(), children_.end(), child);
    if (it != children_.end()) {
        std::cout << "[SceneObject] WARNING: " << child->GetName()
                  << " is already a child of " << name_ << std::endl;
        return;
    }

    std::cout << "[SceneObject] Adding child: " << child->GetName()
              << " to parent: " << name_ << std::endl;

    if (child->parent_) {
        child->parent_->RemoveChild(child);
    }

    child->parent_ = this;
    children_.push_back(child);
    child->RecomputeTransformInheritance();
    child->RebindEntityIDHierarchy();

    std::cout << "[SceneObject] - Hierarchy updated. " << name_
              << " now has " << children_.size() << " children" << std::endl;
}

void SceneObject::AddChild(std::shared_ptr<SceneObject> child) {
    if (!child) return;
    m_OwnedChildren.push_back(child);
    AddChild(child.get());
}

//============================================================================
// HIERARCHY MANAGEMENT
//============================================================================

void SceneObject::RemoveChild(SceneObject* child) {
    if (!child) return;

    auto it = std::find(children_.begin(), children_.end(), child);
    if (it != children_.end()) {
        std::cout << "[SceneObject] Removing child: " << child->GetName()
                  << " from parent: " << name_ << std::endl;
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
    if (entityID_ != 0) {
        IDPool::Get().RecycleID(entityID_);
    }
    entityID_ = id;
}

const std::string& SceneObject::GetName() const {
    return name_;
}

void SceneObject::SetName(const std::string& name) {
    name_ = name;
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

    std::cout << "[SceneObject] Initializing: " << name_ << std::endl;

    for (auto* child : children_) {
        if (child) {
            child->Initialize();
        }
    }

    initialized_ = true;
}

void SceneObject::Cleanup() {
    if (!initialized_) return;

    std::cout << "[SceneObject] Cleaning up: " << name_ << std::endl;

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
    if (parent_) {
        std::cout << "[SceneObject] - Rebinding EntityID " << entityID_
                  << " to parent EntityID " << parent_->GetID() << std::endl;
    } else {
        std::cout << "[SceneObject] - Unbinding EntityID " << entityID_
                  << " (now root)" << std::endl;
    }

    for (auto* child : children_) {
        if (child) {
            child->RebindEntityIDHierarchy();
        }
    }
}

//============================================================================
// END OF FILE
//================================================================