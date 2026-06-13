#include "Runtime/Public/Editor/EditorSceneService.h"

#include "Core/World.h"
#include "ECS/Coordinator.h"
#include "ECS/ECSComponents.h"
#include "Runtime/Public/Editor/EditorDirtyState.h"
#include "Runtime/Public/Editor/EditorNotificationService.h"
#include "Runtime/Public/Editor/EditorSelection.h"
#include "Runtime/Public/Editor/EditorEntityCommands.h"
#include "Scene/Scene.h"
#include "Scene/SceneManager.h"

namespace eng::runtime {

    EditorSceneService::EditorSceneService(SceneManager* sceneManager,
                                           World* world,
                                           EditorDirtyState* dirtyState,
                                           EditorSelection* selection,
                                           EditorNotificationService* notifications)
        : m_SceneManager(sceneManager)
        , m_World(world)
        , m_DirtyState(dirtyState)
        , m_Selection(selection)
        , m_Notifications(notifications) {
    }

    bool EditorSceneService::EnsureActiveScene() {
        if (!m_SceneManager) {
            NotifyError("Scene operation failed: SceneManager is unavailable.");
            return false;
        }

        if (!m_SceneManager->GetActiveScene()) {
            m_SceneManager->CreateNewScene("Untitled");
            NotifySuccess("Created Untitled scene.");
        }

        return true;
    }

    Entity EditorSceneService::CreateEmptyObject(const std::string& name) {
        if (!EnsureActiveScene() || !m_World || !m_DirtyState || !m_Selection) {
            return 0;
        }

        auto& coordinator = m_World->getCoordinator();
        Entity entity = EditorEntityCommands::CreateEmpty(coordinator, *m_DirtyState, *m_Selection);
        if (entity != 0 && coordinator.IsEntityAlive(entity) && coordinator.GetSignature(entity).test(coordinator.GetComponentType<NameComponent>())) {
            coordinator.GetComponent<NameComponent>(entity).name = name;
        }
        SyncAfterMutation("create empty object");
        NotifySuccess("Created entity: " + name);
        return entity;
    }

    Entity EditorSceneService::CreateMeshObject(const std::string& name, AssetHandle meshHandle) {
        if (!EnsureActiveScene() || !m_World || !m_DirtyState || !m_Selection) {
            return 0;
        }

        auto& coordinator = m_World->getCoordinator();
        Entity entity = coordinator.CreateEntity();
        coordinator.AddComponent<NameComponent>(entity, NameComponent(name));
        coordinator.AddComponent<TransformComponent>(entity, TransformComponent());
        coordinator.AddComponent<MeshRendererComponent>(entity, MeshRendererComponent());
        coordinator.AddComponent<RenderableMeshComponent>(entity, RenderableMeshComponent(meshHandle));

        m_Selection->Select(entity);
        SyncAfterMutation("create mesh object");
        NotifySuccess("Created mesh entity: " + name);
        return entity;
    }

    Entity EditorSceneService::CreatePlayerStart() {
        if (!EnsureActiveScene() || !m_World || !m_DirtyState || !m_Selection) {
            return 0;
        }
        Entity entity = EditorEntityCommands::CreatePlayerStart(m_World->getCoordinator(), *m_DirtyState, *m_Selection);
        SyncAfterMutation("create player start");
        NotifySuccess("Created PlayerStart.");
        return entity;
    }

    Entity EditorSceneService::CreateDirectionalLight() {
        if (!EnsureActiveScene() || !m_World || !m_DirtyState || !m_Selection) {
            return 0;
        }
        Entity entity = EditorEntityCommands::CreateDirectionalLight(m_World->getCoordinator(), *m_DirtyState, *m_Selection);
        SyncAfterMutation("create directional light");
        NotifySuccess("Created Directional Light.");
        return entity;
    }

    Entity EditorSceneService::CreatePointLight() {
        if (!EnsureActiveScene() || !m_World || !m_DirtyState || !m_Selection) {
            return 0;
        }
        Entity entity = EditorEntityCommands::CreatePointLight(m_World->getCoordinator(), *m_DirtyState, *m_Selection);
        SyncAfterMutation("create point light");
        NotifySuccess("Created Point Light.");
        return entity;
    }

    Entity EditorSceneService::CreateSkyLight() {
        if (!EnsureActiveScene() || !m_World || !m_DirtyState || !m_Selection) {
            return 0;
        }
        Entity entity = EditorEntityCommands::CreateSkyLight(m_World->getCoordinator(), *m_DirtyState, *m_Selection);
        SyncAfterMutation("create sky light");
        NotifySuccess("Created Sky Light.");
        return entity;
    }

    Entity EditorSceneService::CreateSpotLight() {
        if (!EnsureActiveScene() || !m_World || !m_DirtyState || !m_Selection) {
            return 0;
        }
        Entity entity = EditorEntityCommands::CreateSpotLight(m_World->getCoordinator(), *m_DirtyState, *m_Selection);
        SyncAfterMutation("create spot light");
        NotifySuccess("Created Spot Light.");
        return entity;
    }

    bool EditorSceneService::DeleteObject(Entity entity) {
        if (!EnsureActiveScene() || !m_World || !m_DirtyState || !m_Selection) {
            return false;
        }
        auto& coordinator = m_World->getCoordinator();
        if (entity == 0 || !coordinator.IsEntityAlive(entity)) {
            NotifyWarning("Delete skipped: no valid selected entity.");
            return false;
        }

        EditorEntityCommands::Delete(coordinator, entity, *m_DirtyState, *m_Selection);
        SyncAfterMutation("delete object");
        NotifySuccess("Deleted entity.");
        return true;
    }

    Entity EditorSceneService::DuplicateObject(Entity entity) {
        if (!EnsureActiveScene() || !m_World || !m_DirtyState || !m_Selection) {
            return 0;
        }
        auto& coordinator = m_World->getCoordinator();
        if (entity == 0 || !coordinator.IsEntityAlive(entity)) {
            NotifyWarning("Duplicate skipped: no valid selected entity.");
            return 0;
        }

        Entity duplicate = EditorEntityCommands::Duplicate(coordinator, entity, *m_DirtyState, *m_Selection);
        SyncAfterMutation("duplicate object");
        NotifySuccess("Duplicated entity.");
        return duplicate;
    }

    bool EditorSceneService::RenameObject(Entity entity, const std::string& newName) {
        if (!EnsureActiveScene() || !m_World || newName.empty()) {
            return false;
        }
        auto& coordinator = m_World->getCoordinator();
        if (entity == 0 || !coordinator.IsEntityAlive(entity)) {
            NotifyWarning("Rename skipped: no valid selected entity.");
            return false;
        }

        if (coordinator.GetSignature(entity).test(coordinator.GetComponentType<NameComponent>())) {
            coordinator.GetComponent<NameComponent>(entity).name = newName;
        } else {
            coordinator.AddComponent<NameComponent>(entity, NameComponent(newName));
        }
        SyncAfterMutation("rename object");
        NotifySuccess("Renamed entity: " + newName);
        return true;
    }

    void EditorSceneService::CreateDefaultEditorSceneContent() {
        if (!EnsureActiveScene()) {
            return;
        }
        Scene* activeScene = m_SceneManager->GetActiveScene();
        if (activeScene && !activeScene->GetAllSceneObjects().empty()) {
            return;
        }

        Entity sun = CreateDirectionalLight();
        if (sun != 0 && m_World) {
            auto& coordinator = m_World->getCoordinator();
            if (coordinator.GetSignature(sun).test(coordinator.GetComponentType<NameComponent>())) {
                coordinator.GetComponent<NameComponent>(sun).name = "Sun Light";
            }
        }
        CreateSkyLight();
        if (m_Selection) {
            m_Selection->Clear();
        }
        SyncAfterMutation("create default editor scene content");
    }

    void EditorSceneService::SyncAfterMutation(const char* reason) {
        if (!m_SceneManager) {
            NotifyError("Scene sync failed: SceneManager is unavailable.");
            return;
        }

        m_SceneManager->SyncECSToScene();
        if (m_DirtyState) {
            m_DirtyState->MarkSceneDirty();
        }
        NotifySuccess(std::string("Scene hierarchy synced: ") + (reason ? reason : "mutation"));
    }

    void EditorSceneService::NotifySuccess(const std::string& message) {
        if (m_Notifications) m_Notifications->Success(message);
    }

    void EditorSceneService::NotifyWarning(const std::string& message) {
        if (m_Notifications) m_Notifications->Warning(message);
    }

    void EditorSceneService::NotifyError(const std::string& message) {
        if (m_Notifications) m_Notifications->Error(message);
    }
}
