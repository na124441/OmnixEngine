#include "SceneManager.h"
#include "Scene.h"
#include "SceneObject.h"
#include "SceneLoader.h"
#include "SceneSerializer.h"
#include "SceneValidator.h"
#include "PrefabRegistry.h"
#include "Runtime/Public/World/ZoneEntityComponent.h"
#include "Runtime/Public/Gameplay/PlayerStateComponent.h"
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
    m_Coordinator->RegisterComponent<RenderableMeshComponent>();
    m_Coordinator->RegisterComponent<MaterialComponent>();
    m_Coordinator->RegisterComponent<HealthComponent>();
    m_Coordinator->RegisterComponent<PlayerControllerComponent>();
    m_Coordinator->RegisterComponent<eng::runtime::PlayerStateComponent>();
    m_Coordinator->RegisterComponent<eng::runtime::PlayerTagComponent>();
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

        auto dirObj = std::make_shared<SceneObject>("Sun Light", directional, m_Coordinator);
        activeScene->AddSceneObject(dirObj);

        // 2. Add Sky Light (SkyLightComponent)
        Entity ambient = m_Coordinator->CreateEntity();
        m_Coordinator->AddComponent<NameComponent>(ambient, NameComponent("Sky Light"));
        m_Coordinator->AddComponent<TransformComponent>(ambient, TransformComponent());
        SkyLightComponent skyLight;
        skyLight.color = {0.35f, 0.40f, 0.48f};
        skyLight.intensity = 0.45f;
        skyLight.enabled = true;
        m_Coordinator->AddComponent<SkyLightComponent>(ambient, skyLight);

        auto skyObj = std::make_shared<SceneObject>("Sky Light", ambient, m_Coordinator);
        activeScene->AddSceneObject(skyObj);

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

        auto camObj = std::make_shared<SceneObject>("Main Camera", cameraEnt, m_Coordinator);
        activeScene->AddSceneObject(camObj);

        // 4. Add Ground Plane (with builtin://plane, StaticBodyComponent, BoxColliderComponent)
        if (m_Coordinator->IsComponentRegistered<StaticBodyComponent>() &&
            m_Coordinator->IsComponentRegistered<BoxColliderComponent>() &&
            m_Coordinator->IsComponentRegistered<RenderableMeshComponent>()) {
            Entity groundEnt = m_Coordinator->CreateEntity();
            m_Coordinator->AddComponent<NameComponent>(groundEnt, NameComponent("Ground Plane"));
            TransformComponent groundTrans;
            groundTrans.position = Vector3(0.0f, 0.0f, 0.0f);
            groundTrans.scale = Vector3(20.0f, 1.0f, 20.0f);
            groundTrans.dirty = true;
            m_Coordinator->AddComponent<TransformComponent>(groundEnt, groundTrans);
            m_Coordinator->AddComponent<MeshRendererComponent>(groundEnt, MeshRendererComponent());
            m_Coordinator->AddComponent<RenderableMeshComponent>(groundEnt, RenderableMeshComponent(AssetHandle(0xC00B0002ULL)));
            StaticBodyComponent groundBody;
            groundBody.enabled = true;
            m_Coordinator->AddComponent<StaticBodyComponent>(groundEnt, groundBody);
            BoxColliderComponent groundCollider;
            groundCollider.size = Vector3(20.0f, 0.2f, 20.0f);
            groundCollider.offset = Vector3(0.0f, -0.1f, 0.0f);
            m_Coordinator->AddComponent<BoxColliderComponent>(groundEnt, groundCollider);

            auto groundObj = std::make_shared<SceneObject>("Ground Plane", groundEnt, m_Coordinator);
            activeScene->AddSceneObject(groundObj);
        }

        // 5. Add Physics Cube (with builtin://cube, RigidBodyComponent, BoxColliderComponent)
        if (m_Coordinator->IsComponentRegistered<RigidBodyComponent>() &&
            m_Coordinator->IsComponentRegistered<BoxColliderComponent>() &&
            m_Coordinator->IsComponentRegistered<RenderableMeshComponent>()) {
            Entity cubeEnt = m_Coordinator->CreateEntity();
            m_Coordinator->AddComponent<NameComponent>(cubeEnt, NameComponent("Physics Cube"));
            TransformComponent cubeTrans;
            cubeTrans.position = Vector3(0.0f, 3.0f, 0.0f);
            cubeTrans.scale = Vector3(1.0f, 1.0f, 1.0f);
            cubeTrans.dirty = true;
            m_Coordinator->AddComponent<TransformComponent>(cubeEnt, cubeTrans);
            m_Coordinator->AddComponent<MeshRendererComponent>(cubeEnt, MeshRendererComponent());
            m_Coordinator->AddComponent<RenderableMeshComponent>(cubeEnt, RenderableMeshComponent(AssetHandle(0xC00B0001ULL)));
            RigidBodyComponent cubeBody;
            cubeBody.mass = 1.0f;
            cubeBody.useGravity = true;
            m_Coordinator->AddComponent<RigidBodyComponent>(cubeEnt, cubeBody);
            BoxColliderComponent cubeCollider;
            cubeCollider.size = Vector3(1.0f, 1.0f, 1.0f);
            m_Coordinator->AddComponent<BoxColliderComponent>(cubeEnt, cubeCollider);

            auto cubeObj = std::make_shared<SceneObject>("Physics Cube", cubeEnt, m_Coordinator);
            activeScene->AddSceneObject(cubeObj);
        }

        // 6. Add Player Start
        if (m_Coordinator->IsComponentRegistered<PlayerStartComponent>()) {
            Entity playerStartEnt = m_Coordinator->CreateEntity();
            m_Coordinator->AddComponent<NameComponent>(playerStartEnt, NameComponent("Player Start"));
            TransformComponent playerTrans;
            playerTrans.position = Vector3(0.0f, 1.0f, 6.0f);
            playerTrans.scale = Vector3(1.0f, 1.0f, 1.0f);
            playerTrans.dirty = true;
            m_Coordinator->AddComponent<TransformComponent>(playerStartEnt, playerTrans);
            PlayerStartComponent playerStart;
            playerStart.active = true;
            m_Coordinator->AddComponent<PlayerStartComponent>(playerStartEnt, playerStart);

            auto playerStartObj = std::make_shared<SceneObject>("Player Start", playerStartEnt, m_Coordinator);
            activeScene->AddSceneObject(playerStartObj);
        }
    }
}

bool SceneManager::SaveActiveScene(const std::string& filePath) {
    if (!activeScene) {
        activeScene = new Scene("EditorScene");
    }

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
    // No-op: SceneObject directly forwards all queries and mutations to the ECS Coordinator.
}

//============================================================================
// END OF FILE
//===============================================================