//============================================================================
// Scene.cpp - Production Implementation
//
// Container and updater for the game world
// Implements AddObject and Update algorithms
//
// Created: November 25, 2025
//============================================================================

#include "Scene.h"
#include "SceneObject.h"
#include "IDPool.h"
#include "../ECS/Coordinator.h"

#include <iostream>
#include <algorithm>

//============================================================================
// CONSTRUCTION / DESTRUCTION
//============================================================================

Scene::Scene(const std::string& name)
    : name_(name)
    , ambientLightColor_(0.2f, 0.2f, 0.3f)
    , ambientLightIntensity_(0.5f)
    , gravity_(0.0f, -9.81f, 0.0f)
{
    std::cout << "[Scene] Created scene: " << name_ << std::endl;
}

Scene::~Scene() {
    std::cout << "[Scene] Destroying scene: " << name_ << std::endl;
    Cleanup();
}

//============================================================================
// CORE ALGORITHM: AddObject
//============================================================================

/**
 * @brief AddSceneObject - Add object to scene
 *
 * Algorithm (from specification):
 * 1. Assign EntityID from pool if needed
 * 2. Insert into object map
 * 3. If no parent, add to root list
 * 4. Send EntityID + components to ECS Coordinator
 */
void Scene::AddSceneObject(std::shared_ptr<SceneObject> obj) {
    if (!obj) {
        std::cerr << "[Scene] WARNING: Attempted to add null object" << std::endl;
        return;
    }

    // STEP 1: Assign EntityID from pool if needed
    // (Typically done in SceneObject constructor, but verify here)
    if (obj->GetID() == 0) {
        // Request ID from pool
        uint32_t newID = IDPool::Get().RequestID();
        // obj->SetID(newID);  // TODO: Add SetID() to SceneObject if needed
        std::cout << "[Scene] Assigned EntityID: " << newID << " to " << obj->GetName() << std::endl;
    }

    std::cout << "[Scene] Adding object: " << obj->GetName()
              << " (ID: " << obj->GetID() << ")" << std::endl;

    // STEP 2: Insert into object map
    allObjects_.push_back(obj);

    // Update lookup maps
    nameMap_[obj->GetName()] = obj;
    idMap_[obj->GetID()] = obj;

    // STEP 3: If no parent, add to root list
    if (obj->GetParent() == nullptr) {
        rootObjects_.push_back(obj);
        std::cout << "[Scene]   - Added to root objects list" << std::endl;
    }

    // STEP 4: Send EntityID + components to ECS Coordinator
    RegisterObjectWithECS(obj);
}

//============================================================================
// CORE ALGORITHM: Update
//============================================================================

/**
 * @brief Update - Main scene update loop
 *
 * Algorithm (from specification):
 * 1. Update transform hierarchy (local → world)
 * 2. Update each root SceneObject (calls object.Update())
 * 3. After SceneObjects finish, ECS Coordinator runs systems:
 *    - Physics
 *    - Rendering
 *    - Animation
 *    - Input
 *    - Audio
 * 4. Process destructions or late-additions
 */
void Scene::Update(float deltaTime) {
    // STEP 1: Update transform hierarchy (local → world)
    UpdateTransformHierarchy();

    // STEP 2: Update each root SceneObject
    UpdateRootObjects(deltaTime);

    // STEP 3: ECS Coordinator runs systems
    RunECSSystems(deltaTime);

    // STEP 4: Process destructions or late-additions
    ProcessDestructions();
    ProcessLateAdditions();
}

//============================================================================
// LIFECYCLE METHODS
//============================================================================

void Scene::Initialize() {
    std::cout << "[Scene] Initializing scene: " << name_ << std::endl;

    // Initialize all scene objects
    for (auto& obj : allObjects_) {
        if (obj) {
            // obj->Initialize();  // TODO: Add Initialize() to SceneObject
        }
    }

    std::cout << "[Scene] Scene initialized with " << allObjects_.size() << " objects" << std::endl;
}

void Scene::Cleanup() {
    std::cout << "[Scene] Cleaning up scene: " << name_ << std::endl;

    // Cleanup all scene objects
    for (auto& obj : allObjects_) {
        if (obj) {
            // obj->Cleanup();  // TODO: Add Cleanup() to SceneObject
        }
    }

    // Clear all containers
    allObjects_.clear();
    rootObjects_.clear();
    nameMap_.clear();
    idMap_.clear();
    pendingAdditions_.clear();
    pendingDeletions_.clear();

    std::cout << "[Scene] Scene cleanup complete" << std::endl;
}

//============================================================================
// OBJECT ACCESS
//============================================================================

const std::vector<std::shared_ptr<SceneObject>>& Scene::GetAllSceneObjects() const {
    return allObjects_;
}

const std::vector<std::shared_ptr<SceneObject>>& Scene::GetRootObjects() const {
    return rootObjects_;
}

std::shared_ptr<SceneObject> Scene::FindObjectByName(const std::string& name) const {
    auto it = nameMap_.find(name);
    if (it != nameMap_.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<SceneObject> Scene::FindObjectByID(uint32_t entityID) const {
    auto it = idMap_.find(entityID);
    if (it != idMap_.end()) {
        return it->second;
    }
    return nullptr;
}

void Scene::RemoveSceneObject(std::shared_ptr<SceneObject> obj) {
    if (!obj) return;

    std::cout << "[Scene] Removing object: " << obj->GetName() << std::endl;

    // Add to pending deletions (process at end of frame)
    pendingDeletions_.push_back(obj);
}

//============================================================================
// SCENE METADATA
//============================================================================

const std::string& Scene::GetName() const {
    return name_;
}

void Scene::SetFilePath(const std::string& path) {
    filePath_ = path;
}

const std::string& Scene::GetFilePath() const {
    return filePath_;
}

void Scene::SetDefaultCamera(const std::string& cameraName) {
    defaultCamera_ = cameraName;
}

const std::string& Scene::GetDefaultCamera() const {
    return defaultCamera_;
}

//============================================================================
// ENVIRONMENT SETTINGS
//============================================================================

void Scene::SetAmbientLight(const Vector3& color, float intensity) {
    ambientLightColor_ = color;
    ambientLightIntensity_ = intensity;
}

const Vector3& Scene::GetAmbientLightColor() const {
    return ambientLightColor_;
}

float Scene::GetAmbientLightIntensity() const {
    return ambientLightIntensity_;
}

void Scene::SetGravity(const Vector3& gravity) {
    gravity_ = gravity;
}

const Vector3& Scene::GetGravity() const {
    return gravity_;
}

//============================================================================
// INTERNAL UPDATE HELPERS
//============================================================================

/**
 * @brief UpdateTransformHierarchy - Update all transforms (local → world)
 */
void Scene::UpdateTransformHierarchy() {
    // Update root objects first (they have no parent)
    for (auto& rootObj : rootObjects_) {
        if (rootObj) {
            // Update transform from local to world space
            rootObj->transform.UpdateWorldTransform(nullptr);

            // Recursively update children
            // TODO: Add UpdateChildrenTransforms() to SceneObject
            // rootObj->UpdateChildrenTransforms();
        }
    }
}

/**
 * @brief UpdateRootObjects - Update all root objects (and their children)
 */
void Scene::UpdateRootObjects(float deltaTime) {
    for (auto& rootObj : rootObjects_) {
        if (rootObj) {
            // Call object's Update() which recursively updates children
            rootObj->Update(deltaTime);
        }
    }
}

/**
 * @brief RunECSSystems - Trigger ECS Coordinator to run all systems
 */
void Scene::RunECSSystems(float deltaTime) {
    // TODO: Uncomment when ECS Coordinator is integrated

    // auto& ecs = ECSCoordinator::Get();

    // Run systems in order:
    // std::cout << "[Scene] Running ECS systems..." << std::endl;

    // 1. Physics
    // ecs.GetSystem<PhysicsSystem>()->Update(deltaTime);

    // 2. Animation
    // ecs.GetSystem<AnimationSystem>()->Update(deltaTime);

    // 3. Input
    // ecs.GetSystem<InputSystem>()->Update(deltaTime);

    // 4. Audio
    // ecs.GetSystem<AudioSystem>()->Update(deltaTime);

    // 5. Rendering (usually last)
    // ecs.GetSystem<RenderingSystem>()->Update(deltaTime);

    // Placeholder log
    // std::cout << "[Scene] ECS systems updated (placeholder)" << std::endl;
}

/**
 * @brief ProcessDestructions - Delete pending objects
 */
void Scene::ProcessDestructions() {
    if (pendingDeletions_.empty()) return;

    std::cout << "[Scene] Processing " << pendingDeletions_.size() << " object deletions..." << std::endl;

    for (auto& obj : pendingDeletions_) {
        if (!obj) continue;

        // Remove from ECS
        // TODO: Uncomment when ECS is integrated
        // ECSCoordinator::Get().DestroyEntity(obj->GetID());

        // Remove from allObjects
        auto it = std::find(allObjects_.begin(), allObjects_.end(), obj);
        if (it != allObjects_.end()) {
            allObjects_.erase(it);
        }

        // Remove from rootObjects if present
        auto rootIt = std::find(rootObjects_.begin(), rootObjects_.end(), obj);
        if (rootIt != rootObjects_.end()) {
            rootObjects_.erase(rootIt);
        }

        // Remove from lookup maps
        nameMap_.erase(obj->GetName());
        idMap_.erase(obj->GetID());

        std::cout << "[Scene]   - Deleted: " << obj->GetName() << std::endl;
    }

    pendingDeletions_.clear();
}

/**
 * @brief ProcessLateAdditions - Add pending objects
 */
void Scene::ProcessLateAdditions() {
    if (pendingAdditions_.empty()) return;

    std::cout << "[Scene] Processing " << pendingAdditions_.size() << " late additions..." << std::endl;

    for (auto& obj : pendingAdditions_) {
        if (obj) {
            AddSceneObject(obj);
        }
    }

    pendingAdditions_.clear();
}

/**
 * @brief UpdateLookupMaps - Rebuild lookup maps
 */
void Scene::UpdateLookupMaps() {
    nameMap_.clear();
    idMap_.clear();

    for (auto& obj : allObjects_) {
        if (obj) {
            nameMap_[obj->GetName()] = obj;
            idMap_[obj->GetID()] = obj;
        }
    }
}

/**
 * @brief RegisterObjectWithECS - Send components to ECS Coordinator
 */
void Scene::RegisterObjectWithECS(std::shared_ptr<SceneObject> obj) {
    if (!obj) return;

    std::cout << "[Scene]   - Registering with ECS (EntityID: " << obj->GetID() << ")" << std::endl;

    // TODO: Uncomment when ECS Coordinator is integrated

    // auto& ecs = ECSCoordinator::Get();

    // Register entity
    // ecs.RegisterEntity(obj->GetID());

    // Get all components from SceneObject and add to ECS
    // auto components = obj->GetComponents();
    // for (auto& component : components) {
    //     ecs.AddComponent(obj->GetID(), component);
    // }

    std::cout << "[Scene]   - ECS registration complete (placeholder)" << std::endl;
}

Scene* Scene::Clone(Coordinator& srcCoordinator, Coordinator& destCoordinator, std::unordered_map<Entity, Entity>& outEntityMap) const {
    auto clonedScene = new Scene(name_ + "_Clone");
    clonedScene->SetFilePath(filePath_);
    clonedScene->SetDefaultCamera(defaultCamera_);
    clonedScene->SetAmbientLight(ambientLightColor_, ambientLightIntensity_);
    clonedScene->SetGravity(gravity_);

    // 1. Sort objects by ID for deterministic order
    std::vector<std::shared_ptr<SceneObject>> sortedObjects = allObjects_;
    std::sort(sortedObjects.begin(), sortedObjects.end(), [](const auto& a, const auto& b) {
        if (!a) return false;
        if (!b) return true;
        return a->GetID() < b->GetID();
    });

    std::unordered_map<SceneObject*, std::shared_ptr<SceneObject>> objectMapping;
    std::unordered_map<uint32_t, uint32_t> sceneObjectIDMap;

    // 2. Clone each SceneObject and its ECS counterpart
    for (const auto& oldObj : sortedObjects) {
        if (!oldObj) continue;

        auto clonedObj = std::make_shared<SceneObject>(oldObj->GetName());
        clonedObj->transform.SetPosition(oldObj->transform.GetPosition());
        clonedObj->transform.SetRotation(oldObj->transform.GetRotation());
        clonedObj->transform.SetScale(oldObj->transform.GetScale());
        
        if (oldObj->m_HasRenderableMesh) {
            clonedObj->SetRenderableMesh(oldObj->m_MeshAssetHandle);
        }
        if (oldObj->m_HasMaterial) {
            clonedObj->SetMaterial(oldObj->m_MaterialAssetHandle);
        }
        if (oldObj->m_HasStaticBody) {
            clonedObj->SetStaticBody(oldObj->m_StaticBody);
        }
        if (oldObj->m_HasBoxCollider) {
            clonedObj->SetBoxCollider(oldObj->m_BoxCollider);
        }
        if (oldObj->m_HasSphereCollider) {
            clonedObj->SetSphereCollider(oldObj->m_SphereCollider);
        }
        if (oldObj->m_HasCapsuleCollider) {
            clonedObj->SetCapsuleCollider(oldObj->m_CapsuleCollider);
        }
        if (oldObj->m_HasPlayerStart) {
            clonedObj->SetPlayerStart(oldObj->m_PlayerStart);
        }
        if (oldObj->m_HasCharacterController) {
            clonedObj->SetCharacterController(oldObj->m_CharacterController);
        }
        if (oldObj->m_HasCameraComponent) {
            clonedObj->SetCameraComponent(oldObj->m_CameraComponent);
        }
        if (oldObj->m_HasInputComponent) {
            clonedObj->SetInputComponent(oldObj->m_InputComponent);
        }
        if (oldObj->m_HasTrigger) {
            clonedObj->SetTrigger(oldObj->m_Trigger);
        }
        if (oldObj->m_HasInteractable) {
            clonedObj->SetInteractable(oldObj->m_Interactable);
        }
        if (oldObj->m_HasDirectionalLight) {
            clonedObj->SetDirectionalLight(oldObj->m_DirectionalLight);
        }
        if (oldObj->m_HasPointLight) {
            clonedObj->SetPointLight(oldObj->m_PointLight);
        }
        if (oldObj->m_HasAmbientLight) {
            clonedObj->SetAmbientLight(oldObj->m_AmbientLight);
        }
        if (oldObj->m_HasSpotLight) {
            clonedObj->SetSpotLight(oldObj->m_SpotLight);
        }
        clonedObj->SetActive(oldObj->IsActive());

        objectMapping[oldObj.get()] = clonedObj;
        sceneObjectIDMap[oldObj->GetID()] = clonedObj->GetID();

        Entity oldECSEntity = oldObj->GetECSEntity();
        if (oldECSEntity != 0 && srcCoordinator.IsEntityAlive(oldECSEntity)) {
            Entity newECSEntity = destCoordinator.CreateEntity();
            clonedObj->SetECSEntity(newECSEntity);
            outEntityMap[oldECSEntity] = newECSEntity;

            // Copy all component data
            auto signature = srcCoordinator.GetSignature(oldECSEntity);

            if (signature.test(srcCoordinator.GetComponentType<NameComponent>())) {
                destCoordinator.AddComponent(newECSEntity, srcCoordinator.GetComponent<NameComponent>(oldECSEntity));
            }
            if (signature.test(srcCoordinator.GetComponentType<TransformComponent>())) {
                destCoordinator.AddComponent(newECSEntity, srcCoordinator.GetComponent<TransformComponent>(oldECSEntity));
            }
            if (signature.test(srcCoordinator.GetComponentType<RigidBodyComponent>())) {
                destCoordinator.AddComponent(newECSEntity, srcCoordinator.GetComponent<RigidBodyComponent>(oldECSEntity));
            }
            if (signature.test(srcCoordinator.GetComponentType<ColliderComponent>())) {
                destCoordinator.AddComponent(newECSEntity, srcCoordinator.GetComponent<ColliderComponent>(oldECSEntity));
            }
            if (signature.test(srcCoordinator.GetComponentType<RenderableMeshComponent>())) {
                destCoordinator.AddComponent(newECSEntity, srcCoordinator.GetComponent<RenderableMeshComponent>(oldECSEntity));
            }
            if (signature.test(srcCoordinator.GetComponentType<MaterialComponent>())) {
                destCoordinator.AddComponent(newECSEntity, srcCoordinator.GetComponent<MaterialComponent>(oldECSEntity));
            }
            if (signature.test(srcCoordinator.GetComponentType<StaticBodyComponent>())) {
                destCoordinator.AddComponent(newECSEntity, srcCoordinator.GetComponent<StaticBodyComponent>(oldECSEntity));
            }
            if (signature.test(srcCoordinator.GetComponentType<BoxColliderComponent>())) {
                destCoordinator.AddComponent(newECSEntity, srcCoordinator.GetComponent<BoxColliderComponent>(oldECSEntity));
            }
            if (signature.test(srcCoordinator.GetComponentType<SphereColliderComponent>())) {
                destCoordinator.AddComponent(newECSEntity, srcCoordinator.GetComponent<SphereColliderComponent>(oldECSEntity));
            }
            if (signature.test(srcCoordinator.GetComponentType<CapsuleColliderComponent>())) {
                destCoordinator.AddComponent(newECSEntity, srcCoordinator.GetComponent<CapsuleColliderComponent>(oldECSEntity));
            }
            if (signature.test(srcCoordinator.GetComponentType<TagComponent>())) {
                destCoordinator.AddComponent(newECSEntity, srcCoordinator.GetComponent<TagComponent>(oldECSEntity));
            }
            if (signature.test(srcCoordinator.GetComponentType<LayerComponent>())) {
                destCoordinator.AddComponent(newECSEntity, srcCoordinator.GetComponent<LayerComponent>(oldECSEntity));
            }
            if (signature.test(srcCoordinator.GetComponentType<CameraComponent>())) {
                destCoordinator.AddComponent(newECSEntity, srcCoordinator.GetComponent<CameraComponent>(oldECSEntity));
            }
            if (signature.test(srcCoordinator.GetComponentType<HealthComponent>())) {
                destCoordinator.AddComponent(newECSEntity, srcCoordinator.GetComponent<HealthComponent>(oldECSEntity));
            }
            if (signature.test(srcCoordinator.GetComponentType<PlayerControllerComponent>())) {
                destCoordinator.AddComponent(newECSEntity, srcCoordinator.GetComponent<PlayerControllerComponent>(oldECSEntity));
            }
            if (signature.test(srcCoordinator.GetComponentType<PlayerStartComponent>())) {
                destCoordinator.AddComponent(newECSEntity, srcCoordinator.GetComponent<PlayerStartComponent>(oldECSEntity));
            }
            if (signature.test(srcCoordinator.GetComponentType<CharacterControllerComponent>())) {
                destCoordinator.AddComponent(newECSEntity, srcCoordinator.GetComponent<CharacterControllerComponent>(oldECSEntity));
            }
            if (signature.test(srcCoordinator.GetComponentType<InputComponent>())) {
                destCoordinator.AddComponent(newECSEntity, srcCoordinator.GetComponent<InputComponent>(oldECSEntity));
            }
            if (signature.test(srcCoordinator.GetComponentType<TriggerComponent>())) {
                destCoordinator.AddComponent(newECSEntity, srcCoordinator.GetComponent<TriggerComponent>(oldECSEntity));
            }
            if (signature.test(srcCoordinator.GetComponentType<InteractableComponent>())) {
                destCoordinator.AddComponent(newECSEntity, srcCoordinator.GetComponent<InteractableComponent>(oldECSEntity));
            }
            if (signature.test(srcCoordinator.GetComponentType<DirectionalLightComponent>())) {
                destCoordinator.AddComponent(newECSEntity, srcCoordinator.GetComponent<DirectionalLightComponent>(oldECSEntity));
            }
            if (signature.test(srcCoordinator.GetComponentType<PointLightComponent>())) {
                destCoordinator.AddComponent(newECSEntity, srcCoordinator.GetComponent<PointLightComponent>(oldECSEntity));
            }
            if (signature.test(srcCoordinator.GetComponentType<AmbientLightComponent>())) {
                destCoordinator.AddComponent(newECSEntity, srcCoordinator.GetComponent<AmbientLightComponent>(oldECSEntity));
            }
            if (signature.test(srcCoordinator.GetComponentType<SpotLightComponent>())) {
                destCoordinator.AddComponent(newECSEntity, srcCoordinator.GetComponent<SpotLightComponent>(oldECSEntity));
            }
        }
    }

    // 3. Rebuild parent-child hierarchy in the cloned SceneObjects
    for (const auto& oldObj : sortedObjects) {
        if (!oldObj) continue;
        auto clonedObj = objectMapping[oldObj.get()];
        if (!clonedObj) continue;

        if (oldObj->GetParent()) {
            auto clonedParent = objectMapping[oldObj->GetParent()];
            if (clonedParent) {
                clonedObj->SetParent(clonedParent.get());
                clonedParent->AddChild(clonedObj.get());
            }
        }
    }

    // 4. Populate clonedScene's allObjects_ and rootObjects_
    for (const auto& oldObj : sortedObjects) {
        if (!oldObj) continue;
        auto clonedObj = objectMapping[oldObj.get()];
        if (clonedObj) {
            clonedScene->allObjects_.push_back(clonedObj);
            clonedScene->idMap_[clonedObj->GetID()] = clonedObj;
            clonedScene->nameMap_[clonedObj->GetName()] = clonedObj;
            
            if (clonedObj->GetParent() == nullptr) {
                clonedScene->rootObjects_.push_back(clonedObj);
            }
        }
    }

    // 5. Remap component entity references
    for (const auto& pair : outEntityMap) {
        Entity newEntity = pair.second;
        // Supports remapping entity references inside components if added in the future
    }

    return clonedScene;
}

bool Scene::CompareScene(const Scene& other, const std::unordered_map<Entity, Entity>& entityMap, Coordinator& coordinator) const {
    if (allObjects_.size() != other.allObjects_.size()) {
        std::cout << "[SceneCompare] Object count mismatch: " << allObjects_.size() << " vs " << other.allObjects_.size() << std::endl;
        return false;
    }

    for (const auto& oldObj : allObjects_) {
        if (!oldObj) continue;

        auto clonedObj = other.FindObjectByName(oldObj->GetName());
        if (!clonedObj) {
            std::cout << "[SceneCompare] Cloned object not found by name: " << oldObj->GetName() << std::endl;
            return false;
        }

        Vector3 oldPos = oldObj->transform.GetPosition();
        Vector3 newPos = clonedObj->transform.GetPosition();
        if (std::abs(oldPos.x - newPos.x) > 0.001f || std::abs(oldPos.y - newPos.y) > 0.001f || std::abs(oldPos.z - newPos.z) > 0.001f) {
            std::cout << "[SceneCompare] Transform position mismatch on " << oldObj->GetName() << std::endl;
            return false;
        }

        if (oldObj->m_HasRenderableMesh != clonedObj->m_HasRenderableMesh ||
            (oldObj->m_HasRenderableMesh && oldObj->m_MeshAssetHandle.value != clonedObj->m_MeshAssetHandle.value)) {
            std::cout << "[SceneCompare] Renderable mesh handles mismatched on " << oldObj->GetName() << std::endl;
            return false;
        }

        if (oldObj->m_HasStaticBody != clonedObj->m_HasStaticBody ||
            (oldObj->m_HasStaticBody && oldObj->m_StaticBody.enabled != clonedObj->m_StaticBody.enabled)) {
            std::cout << "[SceneCompare] StaticBody presence or enabled state mismatch on " << oldObj->GetName() << std::endl;
            return false;
        }

        if (oldObj->m_HasBoxCollider != clonedObj->m_HasBoxCollider ||
            (oldObj->m_HasBoxCollider && (oldObj->m_BoxCollider.size.x != clonedObj->m_BoxCollider.size.x ||
                                          oldObj->m_BoxCollider.offset.x != clonedObj->m_BoxCollider.offset.x))) {
            std::cout << "[SceneCompare] BoxCollider presence or sizes mismatch on " << oldObj->GetName() << std::endl;
            return false;
        }

        if (oldObj->m_HasSphereCollider != clonedObj->m_HasSphereCollider ||
            (oldObj->m_HasSphereCollider && (oldObj->m_SphereCollider.radius != clonedObj->m_SphereCollider.radius ||
                                             oldObj->m_SphereCollider.offset.x != clonedObj->m_SphereCollider.offset.x))) {
            std::cout << "[SceneCompare] SphereCollider presence or radius mismatch on " << oldObj->GetName() << std::endl;
            return false;
        }

        if (oldObj->m_HasCapsuleCollider != clonedObj->m_HasCapsuleCollider ||
            (oldObj->m_HasCapsuleCollider && (oldObj->m_CapsuleCollider.radius != clonedObj->m_CapsuleCollider.radius ||
                                              oldObj->m_CapsuleCollider.height != clonedObj->m_CapsuleCollider.height))) {
            std::cout << "[SceneCompare] CapsuleCollider presence or size mismatch on " << oldObj->GetName() << std::endl;
            return false;
        }

        if ((oldObj->GetParent() == nullptr) != (clonedObj->GetParent() == nullptr)) {
            std::cout << "[SceneCompare] Parent presence mismatch on " << oldObj->GetName() << std::endl;
            return false;
        }
        if (oldObj->GetParent() && clonedObj->GetParent()) {
            if (oldObj->GetParent()->GetName() != clonedObj->GetParent()->GetName()) {
                std::cout << "[SceneCompare] Parent name mismatch on " << oldObj->GetName() << ": " 
                          << oldObj->GetParent()->GetName() << " vs " << clonedObj->GetParent()->GetName() << std::endl;
                return false;
            }
        }
        if (oldObj->GetChildren().size() != clonedObj->GetChildren().size()) {
            std::cout << "[SceneCompare] Children count mismatch on " << oldObj->GetName() << std::endl;
            return false;
        }
    }

    std::cout << "[SceneCompare] Diagnostics: Successfully validated " << allObjects_.size() << " cloned nodes and their hierarchy." << std::endl;
    return true;
}

//============================================================================
// END OF FILE
//===============================================================//
// Created by nayan on 11/20/2025.
//