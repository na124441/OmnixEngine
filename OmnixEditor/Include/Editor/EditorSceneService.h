#pragma once

#include "ECS/ECSconfig.h"
#include "Runtime/AssetHandle.h"

#include <string>

class SceneManager;

namespace eng::runtime {
    class World;
    class EditorDirtyState;
    class EditorNotificationService;
    class EditorSelection;

    class EditorSceneService {
    public:
        EditorSceneService(SceneManager* sceneManager,
                           World* world,
                           EditorDirtyState* dirtyState,
                           EditorSelection* selection,
                           EditorNotificationService* notifications);

        bool EnsureActiveScene();

        Entity CreateEmptyObject(const std::string& name = "Empty Entity");
        Entity CreatePlayerStart();
        Entity CreateMeshObject(const std::string& name, AssetHandle meshHandle);
        Entity CreateDirectionalLight();
        Entity CreatePointLight();
        Entity CreateSkyLight();
        Entity CreateSpotLight();

        bool DeleteObject(Entity entity);
        Entity DuplicateObject(Entity entity);
        bool RenameObject(Entity entity, const std::string& newName);

        void CreateDefaultEditorSceneContent();
        void SyncAfterMutation(const char* reason);

    private:
        void NotifySuccess(const std::string& message);
        void NotifyWarning(const std::string& message);
        void NotifyError(const std::string& message);

        SceneManager* m_SceneManager = nullptr;
        World* m_World = nullptr;
        EditorDirtyState* m_DirtyState = nullptr;
        EditorSelection* m_Selection = nullptr;
        EditorNotificationService* m_Notifications = nullptr;
    };
}
