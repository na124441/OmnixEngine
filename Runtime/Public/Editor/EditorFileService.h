#pragma once

#include <string>

class SceneManager;
class Scene;

namespace eng::runtime {
    class EditorDirtyState;
    class EditorSelection;
    struct RuntimeContext;

    class EditorFileService {
    public:
        EditorFileService(RuntimeContext& context, EditorDirtyState& dirtyState, EditorSelection& selection);

        bool NewScene();
        bool OpenScene();
        bool SaveScene();
        bool SaveSceneAs();
        bool ReloadScene();
        bool ValidateScene();

    private:
        SceneManager* GetSceneManager() const;
        void AddDefaultSceneLighting(SceneManager& sceneManager);
        bool SaveSceneToPath(SceneManager& sceneManager, const std::string& path);
        bool ValidateInMemoryScene(Scene& scene);
        void NotifySuccess(const char* message) const;
        void NotifyError(const char* message) const;

        RuntimeContext& m_Context;
        EditorDirtyState& m_DirtyState;
        EditorSelection& m_Selection;
    };
}
