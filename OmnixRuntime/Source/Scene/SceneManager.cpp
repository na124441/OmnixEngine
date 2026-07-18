#include "Scene/SceneManager.h"
#include "Scene/Scene.h"
#include "Scene/SceneObject.h"
#include "Scene/SceneLoader.h"
#include "Scene/SceneSerializer.h"
#include "Scene/SceneValidator.h"
#include "Scene/PrefabRegistry.h"
#include "Runtime/World/ZoneEntityComponent.h"
#include "Runtime/World/ZoneMembershipComponent.h"
#include "Runtime/World/GroundSectionComponent.h"
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
    m_Coordinator->RegisterComponent<PlayerStartComponent>();
    m_Coordinator->RegisterComponent<CharacterControllerComponent>();
    m_Coordinator->RegisterComponent<InputComponent>();
    m_Coordinator->RegisterComponent<TriggerComponent>();
    m_Coordinator->RegisterComponent<InteractableComponent>();
    m_Coordinator->RegisterComponent<ObjectiveComponent>();
    m_Coordinator->RegisterComponent<SimpleStateComponent>();
    m_Coordinator->RegisterComponent<ActivatableComponent>();
    m_Coordinator->RegisterComponent<DoorComponent>();
    m_Coordinator->RegisterComponent<CheckpointComponent>();
    m_Coordinator->RegisterComponent<DirectionalLightComponent>();
    m_Coordinator->RegisterComponent<PointLightComponent>();
    m_Coordinator->RegisterComponent<SkyLightComponent>();
    m_Coordinator->RegisterComponent<SpotLightComponent>();
    m_Coordinator->RegisterComponent<eng::runtime::ZoneEntityComponent>();
    m_Coordinator->RegisterComponent<eng::runtime::ZoneMembershipComponent>();
    m_Coordinator->RegisterComponent<eng::runtime::GroundSectionComponent>();
    m_Coordinator->RegisterComponent<BoundsComponent>();

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
        // Run SceneValidator first!
        SceneValidator validator;
        m_LastValidationReport = validator.ValidateSceneFile(targetSceneName, m_AssetRegistry, &PrefabRegistry::Get());
        
        if (m_LastValidationReport.HasErrors()) {
            std::cerr << "[SceneManager] ERROR: Scene validation failed for file '" 
                      << targetSceneName << "':\n" << m_LastValidationReport.ToString() << std::endl;
            m_ShowValidationFailedModal = true;
            throw std::runtime_error("Scene validation failed");
        }

        std::cout << "[SceneManager] Scene validation passed successfully!" << std::endl;

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

    if (m_Coordinator) {
        // 1. Add Sun Light (DirectionalLightComponent)
        Entity directional = m_Coordinator->CreateEntity();
        m_Coordinator->AddComponent<NameComponent>(directional, NameComponent("Sun Light"));
        TransformComponent dirTransform;
        dirTransform.dirty = true;
        m_Coordinator->AddComponent<TransformComponent>(directional, dirTransform);
        DirectionalLightComponent dirLight;
        dirLight.color = {1.0f, 0.96f, 0.88f};
        dirLight.intensity = 3.0f;
        dirLight.enabled = true;
        m_Coordinator->AddComponent<DirectionalLightComponent>(directional, dirLight);

        // 2. Add Sky Light (SkyLightComponent)
        Entity ambient = m_Coordinator->CreateEntity();
        m_Coordinator->AddComponent<NameComponent>(ambient, NameComponent("Sky Light"));
        m_Coordinator->AddComponent<TransformComponent>(ambient, TransformComponent());
        SkyLightComponent skyLight;
        skyLight.color = {0.35f, 0.40f, 0.48f};
        skyLight.intensity = 0.45f;
        skyLight.enabled = true;
        m_Coordinator->AddComponent<SkyLightComponent>(ambient, skyLight);

        // 3. Add Main Camera (CameraComponent)
        Entity cameraEnt = m_Coordinator->CreateEntity();
        m_Coordinator->AddComponent<NameComponent>(cameraEnt, NameComponent("Main Camera"));
        TransformComponent camTransform;
        camTransform.position = Vector3(0.0f, 2.0f, 10.0f);
        camTransform.dirty = true;
        m_Coordinator->AddComponent<TransformComponent>(cameraEnt, camTransform);
        CameraComponent cam;
        cam.exposure = 1.0f;
        cam.isPrimary = true;
        m_Coordinator->AddComponent<CameraComponent>(cameraEnt, cam);

        SyncECSToScene();
    }
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

            // Sync GroundSectionComponent
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<eng::runtime::GroundSectionComponent>())) {
                foundObj->SetGroundSection(coordinator.GetComponent<eng::runtime::GroundSectionComponent>(entity));
            } else {
                foundObj->ClearGroundSection();
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

            // Sync PlayerStartComponent
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<PlayerStartComponent>())) {
                foundObj->SetPlayerStart(coordinator.GetComponent<PlayerStartComponent>(entity));
            } else {
                foundObj->ClearPlayerStart();
            }

            // Sync CharacterControllerComponent
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<CharacterControllerComponent>())) {
                foundObj->SetCharacterController(coordinator.GetComponent<CharacterControllerComponent>(entity));
            } else {
                foundObj->ClearCharacterController();
            }

            // Sync CameraComponent
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<CameraComponent>())) {
                foundObj->SetCameraComponent(coordinator.GetComponent<CameraComponent>(entity));
            } else {
                foundObj->ClearCameraComponent();
            }

            // Sync InputComponent
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<InputComponent>())) {
                foundObj->SetInputComponent(coordinator.GetComponent<InputComponent>(entity));
            } else {
                foundObj->ClearInputComponent();
            }

            // Sync TriggerComponent
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<TriggerComponent>())) {
                foundObj->SetTrigger(coordinator.GetComponent<TriggerComponent>(entity));
            } else {
                foundObj->ClearTrigger();
            }

            // Sync InteractableComponent
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<InteractableComponent>())) {
                foundObj->SetInteractable(coordinator.GetComponent<InteractableComponent>(entity));
            } else {
                foundObj->ClearInteractable();
            }
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<ObjectiveComponent>())) {
                foundObj->SetObjective(coordinator.GetComponent<ObjectiveComponent>(entity));
            } else {
                foundObj->ClearObjective();
            }
            // Sync AudioSourceComponent
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<AudioSourceComponent>())) {
                foundObj->SetAudioSource(coordinator.GetComponent<AudioSourceComponent>(entity));
            } else {
                foundObj->ClearAudioSource();
            }

            // Sync SimpleStateComponent
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<SimpleStateComponent>())) {
                foundObj->SetSimpleState(coordinator.GetComponent<SimpleStateComponent>(entity));
            } else {
                foundObj->ClearSimpleState();
            }

            // Sync ActivatableComponent
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<ActivatableComponent>())) {
                foundObj->SetActivatable(coordinator.GetComponent<ActivatableComponent>(entity));
            } else {
                foundObj->ClearActivatable();
            }

            // Sync DoorComponent
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<DoorComponent>())) {
                foundObj->SetDoor(coordinator.GetComponent<DoorComponent>(entity));
            } else {
                foundObj->ClearDoor();
            }

            // Sync CheckpointComponent
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<CheckpointComponent>())) {
                foundObj->SetCheckpoint(coordinator.GetComponent<CheckpointComponent>(entity));
            } else {
                foundObj->ClearCheckpoint();
            }

            // Sync DirectionalLightComponent
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<DirectionalLightComponent>())) {
                foundObj->SetDirectionalLight(coordinator.GetComponent<DirectionalLightComponent>(entity));
            } else {
                foundObj->ClearDirectionalLight();
            }

            // Sync PointLightComponent
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<PointLightComponent>())) {
                foundObj->SetPointLight(coordinator.GetComponent<PointLightComponent>(entity));
            } else {
                foundObj->ClearPointLight();
            }

            // Sync SkyLightComponent
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<SkyLightComponent>())) {
                foundObj->SetSkyLight(coordinator.GetComponent<SkyLightComponent>(entity));
            } else {
                foundObj->ClearSkyLight();
            }

            // Sync SpotLightComponent
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<SpotLightComponent>())) {
                foundObj->SetSpotLight(coordinator.GetComponent<SpotLightComponent>(entity));
            } else {
                foundObj->ClearSpotLight();
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

            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<eng::runtime::GroundSectionComponent>())) {
                newObj->SetGroundSection(coordinator.GetComponent<eng::runtime::GroundSectionComponent>(entity));
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

            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<PlayerStartComponent>())) {
                newObj->SetPlayerStart(coordinator.GetComponent<PlayerStartComponent>(entity));
            }

            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<CharacterControllerComponent>())) {
                newObj->SetCharacterController(coordinator.GetComponent<CharacterControllerComponent>(entity));
            }

            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<CameraComponent>())) {
                newObj->SetCameraComponent(coordinator.GetComponent<CameraComponent>(entity));
            }

            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<InputComponent>())) {
                newObj->SetInputComponent(coordinator.GetComponent<InputComponent>(entity));
            }

            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<TriggerComponent>())) {
                newObj->SetTrigger(coordinator.GetComponent<TriggerComponent>(entity));
            }

            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<InteractableComponent>())) {
                newObj->SetInteractable(coordinator.GetComponent<InteractableComponent>(entity));
            }
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<ObjectiveComponent>())) {
                newObj->SetObjective(coordinator.GetComponent<ObjectiveComponent>(entity));
            }
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<AudioSourceComponent>())) {
                newObj->SetAudioSource(coordinator.GetComponent<AudioSourceComponent>(entity));
            }
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<SimpleStateComponent>())) {
                newObj->SetSimpleState(coordinator.GetComponent<SimpleStateComponent>(entity));
            }
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<ActivatableComponent>())) {
                newObj->SetActivatable(coordinator.GetComponent<ActivatableComponent>(entity));
            }
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<DoorComponent>())) {
                newObj->SetDoor(coordinator.GetComponent<DoorComponent>(entity));
            }
            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<CheckpointComponent>())) {
                newObj->SetCheckpoint(coordinator.GetComponent<CheckpointComponent>(entity));
            }

            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<DirectionalLightComponent>())) {
                newObj->SetDirectionalLight(coordinator.GetComponent<DirectionalLightComponent>(entity));
            }

            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<PointLightComponent>())) {
                newObj->SetPointLight(coordinator.GetComponent<PointLightComponent>(entity));
            }

            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<SkyLightComponent>())) {
                newObj->SetSkyLight(coordinator.GetComponent<SkyLightComponent>(entity));
            }

            if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<SpotLightComponent>())) {
                newObj->SetSpotLight(coordinator.GetComponent<SpotLightComponent>(entity));
            }

            activeScene->AddSceneObject(newObj);
        }
    }
}

//============================================================================
// END OF FILE
//===============================================================