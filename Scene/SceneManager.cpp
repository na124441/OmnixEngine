#include "SceneManager.h"
#include "SceneLoader.h"
#include "SceneSerializer.h"
#include "Scene.h"
#include "SceneObject.h"
#include "../ECS/Coordinator.h"
#include "../ECS/ECSComponents.h"
#include <iostream>
#include <stdexcept>

//============================================================================
// CONSTRUCTOR / DESTRUCTOR
//============================================================================

SceneManager::SceneManager(Coordinator* coordinator)
    : state(TransitionState::Running)
    , activeScene(nullptr)
    , loadingScene(nullptr)
    , targetSceneName("")
    , transitionsEnabled(false)
    , loadingComplete(false)
{
    if (coordinator) {
        m_Coordinator = coordinator;
        m_OwnsCoordinator = false;
    } else {
        m_OwnedCoordinator = std::make_unique<Coordinator>();
        m_OwnedCoordinator->Init();
        m_Coordinator = m_OwnedCoordinator.get();
        m_OwnsCoordinator = true;
        InitializeECS();
    }
    std::cout << "[SceneManager] Initialized" << std::endl;
}

SceneManager::~SceneManager() {
    // Cleanup active scene on shutdown
    if (activeScene) {
        UnloadActiveScene();
    }
    std::cout << "[SceneManager] Destroyed" << std::endl;
}

void SceneManager::SetCoordinator(Coordinator* coordinator) {
    m_Coordinator = coordinator;
}

//============================================================================
// ✅ NEW: ECS INTEGRATION
//============================================================================

void SceneManager::InitializeECS() {
    std::cout << "[SceneManager] Initializing ECS..." << std::endl;

    m_Coordinator->Init();

    // Register components
    m_Coordinator->RegisterComponent<TransformComponent>();
    m_Coordinator->RegisterComponent<MeshRendererComponent>();
    m_Coordinator->RegisterComponent<CameraComponent>();
    m_Coordinator->RegisterComponent<LightComponent>();
    m_Coordinator->RegisterComponent<RigidBodyComponent>();
    m_Coordinator->RegisterComponent<ColliderComponent>();
    m_Coordinator->RegisterComponent<AudioSourceComponent>();
    m_Coordinator->RegisterComponent<AnimatorComponent>();
    m_Coordinator->RegisterComponent<ScriptComponent>();
    m_Coordinator->RegisterComponent<TagComponent>();
    m_Coordinator->RegisterComponent<LayerComponent>();
    m_Coordinator->RegisterComponent<NameComponent>();
    m_Coordinator->RegisterComponent<StaticBodyComponent>();
    m_Coordinator->RegisterComponent<BoxColliderComponent>();
    m_Coordinator->RegisterComponent<SphereColliderComponent>();
    m_Coordinator->RegisterComponent<CapsuleColliderComponent>();

    std::cout << "[SceneManager] ECS initialized successfully" << std::endl;
}

Coordinator& SceneManager::GetCoordinator() {
    return *m_Coordinator;
}

//============================================================================
// PUBLIC INTERFACE - ALGORITHM IMPLEMENTATION
//============================================================================

void SceneManager::LoadScene(const std::string& sceneName) {
    std::cout << "[SceneManager] ==== LOAD SCENE REQUEST ====" << std::endl;
    std::cout << "[SceneManager] Requested: " << sceneName << std::endl;

    targetSceneName = sceneName;
    state = TransitionState::Loading;

    if (transitionsEnabled) {
        std::cout << "[SceneManager] Visual transitions enabled (TransitionManager TODO)" << std::endl;
    }

    loadingComplete = false;
    std::cout << "[SceneManager] State changed: LOADING" << std::endl;
}

void SceneManager::Update(float dt) {
    if (state == TransitionState::Loading) {
        ProcessLoading();
    }

    if (state == TransitionState::ReadyToSwitch) {
        SwitchScene();
    }

    if (state == TransitionState::Running && activeScene) {
        activeScene->Update(dt);
    }
}

Scene* SceneManager::GetActiveScene() const {
    return activeScene;
}

void SceneManager::SetActiveScene(Scene* scene) {
    activeScene = scene;
}

bool SceneManager::IsLoading() const {
    return state == TransitionState::Loading;
}

SceneManager::TransitionState SceneManager::GetTransitionState() const {
    return state;
}

void SceneManager::SetTransitionsEnabled(bool enabled) {
    transitionsEnabled = enabled;
    std::cout << "[SceneManager] Visual transitions: "
              << (enabled ? "ENABLED" : "DISABLED") << std::endl;
}

//============================================================================
// PRIVATE IMPLEMENTATION - CORE ALGORITHMS
//============================================================================

void SceneManager::ProcessLoading() {
    if (state != TransitionState::Loading) {
        return;
    }

    if (loadingComplete) {
        return;
    }

    std::cout << "[SceneManager] ==== PROCESSING LOAD ====" << std::endl;
    std::cout << "[SceneManager] Loading: " << targetSceneName << std::endl;

    try {
        loadingScene = SceneLoader::LoadFromFile(targetSceneName);

        if (!loadingScene) {
            throw std::runtime_error("SceneLoader returned nullptr");
        }

        loadingComplete = true;
        state = TransitionState::ReadyToSwitch;
        std::cout << "[SceneManager] Scene loaded successfully!" << std::endl;
        std::cout << "[SceneManager] State changed: READY_TO_SWITCH" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[SceneManager] ERROR: Failed to load scene '"
                  << targetSceneName << "': " << e.what() << std::endl;
        state = TransitionState::Running;
        loadingScene = nullptr;
        loadingComplete = false;
    }
}

void SceneManager::SwitchScene() {
    if (state != TransitionState::ReadyToSwitch) {
        return;
    }

    if (!loadingScene) {
        std::cerr << "[SceneManager] ERROR: Cannot switch - loadingScene is null" << std::endl;
        state = TransitionState::Running;
        return;
    }

    std::cout << "[SceneManager] ==== SWITCHING SCENE ====" << std::endl;

    if (activeScene) {
        UnloadActiveScene();
    }

    activeScene = loadingScene;
    loadingScene = nullptr;
    loadingComplete = false;
    std::cout << "[SceneManager] New active scene: " << activeScene->GetName() << std::endl;

    RegisterSceneEntities(activeScene);
    activeScene->Initialize();

    if (transitionsEnabled) {
        std::cout << "[SceneManager] Visual transition: Fade in (TransitionManager TODO)" << std::endl;
    }

    state = TransitionState::Running;
    std::cout << "[SceneManager] State changed: RUNNING" << std::endl;
    std::cout << "[SceneManager] ==== SCENE SWITCH COMPLETE ====" << std::endl;
}

void SceneManager::UnloadActiveScene() {
    if (!activeScene) {
        return;
    }

    std::cout << "[SceneManager] Unloading scene: " << activeScene->GetName() << std::endl;

    // Destroy all active entities in the coordinator to ensure clean state and prevent accumulation
    if (m_Coordinator) {
        std::vector<Entity> entitiesToDestroy;
        for (Entity entity : m_Coordinator->GetActiveEntities()) {
            entitiesToDestroy.push_back(entity);
        }
        for (Entity entity : entitiesToDestroy) {
            if (entity != 0 && m_Coordinator->IsEntityAlive(entity)) {
                m_Coordinator->DestroyEntity(entity);
            }
        }
    }

    activeScene->Cleanup();
    delete activeScene;
    activeScene = nullptr;
    std::cout << "[SceneManager] Scene unloaded successfully" << std::endl;
}

void SceneManager::RegisterSceneEntities(Scene* scene) {
    if (!scene) {
        return;
    }

    std::cout << "[SceneManager] Registering scene entities with ECS..." << std::endl;

    const auto& sceneObjects = scene->GetAllSceneObjects();
    for (const auto& sceneObject : sceneObjects) {
        if (sceneObject) {
            // ✅ UPDATED: Initialize with ECS
            sceneObject->InitializeWithECS(*m_Coordinator);

            std::cout << "[SceneManager] - Registered Entity (SceneObject ID: "
                      << sceneObject->GetID()
                      << ", ECS Entity: " << sceneObject->GetECSEntity()
                      << ", Name: " << sceneObject->GetName() << ")" << std::endl;
        }
    }

    std::cout << "[SceneManager] Registered " << sceneObjects.size()
              << " entities with ECS" << std::endl;
}

//============================================================================
// ADDITIONAL UTILITY METHODS
//============================================================================

SceneObject* SceneManager::GetSceneObjectByName(const std::string& name) const {
    if (!activeScene) {
        return nullptr;
    }

    auto obj = activeScene->FindObjectByName(name);
    return obj.get();
}

SceneObject* SceneManager::GetSceneObjectByID(uint32_t entityID) const {
    if (!activeScene) {
        return nullptr;
    }

    auto obj = activeScene->FindObjectByID(entityID);
    return obj.get();
}

void SceneManager::ReloadCurrentScene() {
    if (!activeScene) {
        std::cerr << "[SceneManager] ERROR: No active scene to reload" << std::endl;
        return;
    }

    std::string currentScenePath = activeScene->GetFilePath();
    if (currentScenePath.empty()) {
        std::cerr << "[SceneManager] ERROR: Active scene has no file path" << std::endl;
        return;
    }

    std::cout << "[SceneManager] Reloading current scene: " << currentScenePath << std::endl;
    LoadScene(currentScenePath);
}

//============================================================================
// DEBUG / LOGGING UTILITIES
//============================================================================

void SceneManager::PrintDebugInfo() const {
    std::cout << "\n[SceneManager] ==== DEBUG INFO ====" << std::endl;

    std::cout << "State: ";
    switch (state) {
        case TransitionState::Running: std::cout << "RUNNING"; break;
        case TransitionState::Loading: std::cout << "LOADING"; break;
        case TransitionState::ReadyToSwitch: std::cout << "READY_TO_SWITCH"; break;
    }
    std::cout << std::endl;

    if (activeScene) {
        std::cout << "Active Scene: " << activeScene->GetName() << std::endl;
        std::cout << " - Objects: " << activeScene->GetAllSceneObjects().size() << std::endl;
        std::cout << " - File: " << activeScene->GetFilePath() << std::endl;
    } else {
        std::cout << "Active Scene: [NONE]" << std::endl;
    }

    if (loadingScene) {
        std::cout << "Loading Scene: " << loadingScene->GetName() << std::endl;
    }

    if (!targetSceneName.empty()) {
        std::cout << "Target Scene: " << targetSceneName << std::endl;
    }

    std::cout << "Transitions Enabled: " << (transitionsEnabled ? "YES" : "NO") << std::endl;
    std::cout << "===========================\n" << std::endl;
}

void SceneManager::CreateNewScene(const std::string& name) {
    if (activeScene) {
        UnloadActiveScene();
    }
    activeScene = new Scene(name);
    activeScene->SetFilePath("");
    state = TransitionState::Running;
    std::cout << "[SceneManager] Created new scene: " << name << std::endl;
}

bool SceneManager::SaveActiveScene(const std::string& filePath) {
    if (!activeScene) {
        activeScene = new Scene("EditorScene");
    }

    // Sync coordinator changes into Scene graph before saving
    SyncECSToScene();

    activeScene->SetFilePath(filePath);
    bool ok = SceneSerializer::SaveScene(activeScene, filePath);
    if (ok) {
        std::cout << "[SceneManager] Successfully saved active scene to: " << filePath << std::endl;
    } else {
        std::cerr << "[SceneManager] ERROR: Failed to save active scene to: " << filePath << std::endl;
    }
    return ok;
}

void SceneManager::SyncECSToScene() {
    if (!activeScene || !m_Coordinator) return;

    auto& coordinator = *m_Coordinator;
    const auto& activeEntities = coordinator.GetActiveEntities();

    // 1. Find and remove SceneObjects whose ECS entities are dead
    std::vector<std::shared_ptr<SceneObject>> objectsToRemove;
    for (const auto& obj : activeScene->GetAllSceneObjects()) {
        Entity entity = obj->GetECSEntity();
        if (entity != 0 && !coordinator.IsEntityAlive(entity)) {
            objectsToRemove.push_back(obj);
        }
    }
    for (const auto& obj : objectsToRemove) {
        activeScene->RemoveSceneObject(obj);
    }

    // Call Scene Update with dt=0 to process pending deallocations/deletions in Scene
    activeScene->Update(0.0f);

    // 2. Update existing SceneObjects or create new ones for active ECS entities
    for (Entity entity : activeEntities) {
        if (entity == 0 || !coordinator.IsEntityAlive(entity)) continue;

        // Try to find matching SceneObject by ECS Entity
        std::shared_ptr<SceneObject> foundObj = nullptr;
        for (const auto& candidate : activeScene->GetAllSceneObjects()) {
            if (candidate->GetECSEntity() == entity) {
                foundObj = candidate;
                break;
            }
        }

        if (foundObj) {
            // Update name from ECS NameComponent if present
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<NameComponent>())) {
                foundObj->SetName(coordinator.GetComponent<NameComponent>(entity).name);
            }

            // Update transform from ECS TransformComponent if present
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<TransformComponent>())) {
                const auto& tc = coordinator.GetComponent<TransformComponent>(entity);
                foundObj->transform.SetPosition(tc.position);
                foundObj->transform.SetRotation(tc.rotation);
                foundObj->transform.SetScale(tc.scale);
            }

            // Sync RenderableMeshComponent
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<RenderableMeshComponent>())) {
                const auto& rm = coordinator.GetComponent<RenderableMeshComponent>(entity);
                foundObj->SetRenderableMesh(rm.meshAssetHandle);
            } else {
                foundObj->ClearRenderableMesh();
            }

            // Sync MaterialComponent
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<MaterialComponent>())) {
                const auto& mc = coordinator.GetComponent<MaterialComponent>(entity);
                foundObj->SetMaterial(mc.materialAssetHandle);
            } else {
                foundObj->ClearMaterial();
            }

            // Sync StaticBodyComponent
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<StaticBodyComponent>())) {
                foundObj->SetStaticBody(coordinator.GetComponent<StaticBodyComponent>(entity));
            } else {
                foundObj->ClearStaticBody();
            }

            // Sync BoxColliderComponent
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<BoxColliderComponent>())) {
                foundObj->SetBoxCollider(coordinator.GetComponent<BoxColliderComponent>(entity));
            } else {
                foundObj->ClearBoxCollider();
            }

            // Sync SphereColliderComponent
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<SphereColliderComponent>())) {
                foundObj->SetSphereCollider(coordinator.GetComponent<SphereColliderComponent>(entity));
            } else {
                foundObj->ClearSphereCollider();
            }

            // Sync CapsuleColliderComponent
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<CapsuleColliderComponent>())) {
                foundObj->SetCapsuleCollider(coordinator.GetComponent<CapsuleColliderComponent>(entity));
            } else {
                foundObj->ClearCapsuleCollider();
            }
        } else {
            // Create a new SceneObject for this entity
            std::string name = "Entity_" + std::to_string(entity);
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<NameComponent>())) {
                name = coordinator.GetComponent<NameComponent>(entity).name;
            }

            auto newObj = std::make_shared<SceneObject>(name);
            newObj->SetECSEntity(entity);

            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<TransformComponent>())) {
                const auto& tc = coordinator.GetComponent<TransformComponent>(entity);
                newObj->transform.SetPosition(tc.position);
                newObj->transform.SetRotation(tc.rotation);
                newObj->transform.SetScale(tc.scale);
            }

            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<RenderableMeshComponent>())) {
                newObj->SetRenderableMesh(coordinator.GetComponent<RenderableMeshComponent>(entity).meshAssetHandle);
            }

            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<MaterialComponent>())) {
                newObj->SetMaterial(coordinator.GetComponent<MaterialComponent>(entity).materialAssetHandle);
            }

            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<StaticBodyComponent>())) {
                newObj->SetStaticBody(coordinator.GetComponent<StaticBodyComponent>(entity));
            }

            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<BoxColliderComponent>())) {
                newObj->SetBoxCollider(coordinator.GetComponent<BoxColliderComponent>(entity));
            }

            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<SphereColliderComponent>())) {
                newObj->SetSphereCollider(coordinator.GetComponent<SphereColliderComponent>(entity));
            }

            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<CapsuleColliderComponent>())) {
                newObj->SetCapsuleCollider(coordinator.GetComponent<CapsuleColliderComponent>(entity));
            }

            activeScene->AddSceneObject(newObj);
        }
    }
}

//============================================================================
// END OF FILE
//===============================================================