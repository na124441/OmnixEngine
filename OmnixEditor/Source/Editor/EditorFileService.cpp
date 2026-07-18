#include "Editor/EditorFileService.h"

#include "Core/Logging/Logger.h"
#include "Editor/EditorDirtyState.h"
#include "Editor/EditorSelection.h"
#include "Editor/PlatformFileDialog.h"
#include "Runtime/RuntimeContext.h"
#include "Runtime/AssetRegistry.h"
#include "ECS/Coordinator.h"
#include "ECS/ECSComponents.h"
#include "ECS/Public/IECSWorld.h"
#include "Scene/Scene.h"
#include "Scene/SceneManager.h"
#include "Scene/SceneSerializer.h"
#include "Scene/SceneValidator.h"
#include "Scene/PrefabRegistry.h"

#include <filesystem>

namespace eng::runtime {

    namespace {
        constexpr const char* SceneDialogFilter =
            "Omnix Scene (*.omnixscene)\0*.omnixscene\0JSON Scene (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    }

    EditorFileService::EditorFileService(RuntimeContext& context, EditorDirtyState& dirtyState, EditorSelection& selection)
        : m_Context(context)
        , m_DirtyState(dirtyState)
        , m_Selection(selection) {
    }

    SceneManager* EditorFileService::GetSceneManager() const {
        return dynamic_cast<SceneManager*>(m_Context.scenes);
    }

    bool EditorFileService::NewScene() {
        SceneManager* sceneManager = GetSceneManager();
        if (!sceneManager) {
            NotifyError("New Scene failed: SceneManager is unavailable.");
            return false;
        }

        if (m_DirtyState.IsSceneDirty() && !SaveScene()) {
            NotifyError("New Scene cancelled: current scene was not saved.");
            return false;
        }

        sceneManager->CreateNewScene("Untitled");
        AddDefaultSceneLighting(*sceneManager);
        m_DirtyState.ClearSceneDirty();
        m_Selection.Clear();
        NotifySuccess("Created new scene.");
        return true;
    }

    bool EditorFileService::OpenScene() {
        SceneManager* sceneManager = GetSceneManager();
        if (!sceneManager) {
            NotifyError("Open Scene failed: SceneManager is unavailable.");
            return false;
        }

        if (m_DirtyState.IsSceneDirty() && !SaveScene()) {
            NotifyError("Open Scene cancelled: current scene was not saved.");
            return false;
        }

        std::string path = eng::editor::PlatformFileDialog::ShowOpenDialog(SceneDialogFilter);
        if (path.empty()) {
            NotifyError("Open Scene cancelled.");
            return false;
        }

        sceneManager->LoadScene(path);
        m_DirtyState.ClearSceneDirty();
        m_Selection.Clear();
        NotifySuccess("Open Scene requested.");
        return true;
    }

    bool EditorFileService::SaveScene() {
        SceneManager* sceneManager = GetSceneManager();
        if (!sceneManager) {
            NotifyError("Save Scene failed: SceneManager is unavailable.");
            return false;
        }

        Scene* activeScene = sceneManager->GetActiveScene();
        if (!activeScene) {
            sceneManager->CreateNewScene("Untitled");
            activeScene = sceneManager->GetActiveScene();
        }

        if (!activeScene) {
            NotifyError("Save Scene failed: could not create or find an active scene.");
            return false;
        }

        std::string path = activeScene->GetFilePath();
        if (path.empty()) {
            return SaveSceneAs();
        }

        return SaveSceneToPath(*sceneManager, path);
    }

    bool EditorFileService::SaveSceneAs() {
        SceneManager* sceneManager = GetSceneManager();
        if (!sceneManager) {
            NotifyError("Save Scene As failed: SceneManager is unavailable.");
            return false;
        }

        if (!sceneManager->GetActiveScene()) {
            sceneManager->CreateNewScene("Untitled");
        }

        std::string path = eng::editor::PlatformFileDialog::ShowSaveDialog(SceneDialogFilter, "omnixscene");
        if (path.empty()) {
            NotifyError("Save Scene As cancelled.");
            return false;
        }

        return SaveSceneToPath(*sceneManager, path);
    }

    bool EditorFileService::ReloadScene() {
        SceneManager* sceneManager = GetSceneManager();
        if (!sceneManager) {
            NotifyError("Reload Scene failed: SceneManager is unavailable.");
            return false;
        }

        Scene* activeScene = sceneManager->GetActiveScene();
        if (!activeScene) {
            NotifyError("Reload Scene failed: no active scene.");
            return false;
        }

        if (m_DirtyState.IsSceneDirty() && !SaveScene()) {
            NotifyError("Reload Scene cancelled: current scene was not saved.");
            return false;
        }

        if (activeScene->GetFilePath().empty()) {
            NotifyError("Reload Scene failed: active scene has not been saved yet.");
            return false;
        }

        sceneManager->ReloadCurrentScene();
        m_DirtyState.ClearSceneDirty();
        m_Selection.Clear();
        NotifySuccess("Reload Scene requested.");
        return true;
    }

    bool EditorFileService::ValidateScene() {
        SceneManager* sceneManager = GetSceneManager();
        if (!sceneManager) {
            NotifyError("Validate Scene failed: SceneManager is unavailable.");
            return false;
        }

        Scene* activeScene = sceneManager->GetActiveScene();
        if (!activeScene) {
            NotifyError("Validate Scene failed: no active scene.");
            return false;
        }

        sceneManager->SyncECSToScene();

        std::string path = activeScene->GetFilePath();
        if (path.empty()) {
            return ValidateInMemoryScene(*activeScene);
        }

        SceneValidator validator;
        auto report = validator.ValidateSceneFile(path, m_Context.assetRegistry, &PrefabRegistry::Get());
        sceneManager->SetLastValidationReport(report);

        if (report.HasErrors()) {
            sceneManager->TriggerValidationFailedModal();
            NotifyError("Validate Scene failed: validation errors found.");
            return false;
        }

        NotifySuccess("Validate Scene passed.");
        return true;
    }

    bool EditorFileService::SaveSceneToPath(SceneManager& sceneManager, const std::string& path) {
        if (path.empty()) {
            NotifyError("Save Scene failed: output path is empty.");
            return false;
        }

        std::filesystem::path fsPath(path);
        if (fsPath.has_parent_path()) {
            std::filesystem::create_directories(fsPath.parent_path());
        }

        if (!sceneManager.SaveActiveScene(path)) {
            NotifyError("Save Scene failed: SceneSerializer returned false.");
            return false;
        }

        m_DirtyState.ClearSceneDirty();
        NotifySuccess("Scene saved.");
        return true;
    }

    void EditorFileService::AddDefaultSceneLighting(SceneManager& sceneManager) {
        if (!m_Context.ecs) {
            NotifyError("Default scene lighting skipped: ECS is unavailable.");
            return;
        }

        auto& coordinator = m_Context.ecs->getCoordinator();

        Entity directional = coordinator.CreateEntity();
        coordinator.AddComponent<NameComponent>(directional, NameComponent("Sun Light"));
        TransformComponent dirTransform;
        dirTransform.dirty = true;
        coordinator.AddComponent<TransformComponent>(directional, dirTransform);
        DirectionalLightComponent dirLight;
        dirLight.color = {1.0f, 0.96f, 0.88f};
        dirLight.intensity = 3.0f;
        dirLight.enabled = true;
        coordinator.AddComponent<DirectionalLightComponent>(directional, dirLight);

        Entity ambient = coordinator.CreateEntity();
        coordinator.AddComponent<NameComponent>(ambient, NameComponent("Sky Light"));
        coordinator.AddComponent<TransformComponent>(ambient, TransformComponent());
        SkyLightComponent skyLight;
        skyLight.color = {0.35f, 0.40f, 0.48f};
        skyLight.intensity = 0.45f;
        skyLight.enabled = true;
        coordinator.AddComponent<SkyLightComponent>(ambient, skyLight);

        sceneManager.SyncECSToScene();
    }

    bool EditorFileService::ValidateInMemoryScene(Scene& scene) {
        SceneManager* sceneManager = GetSceneManager();
        if (!sceneManager) {
            NotifyError("Validate Scene failed: SceneManager is unavailable.");
            return false;
        }

        std::filesystem::path tempPath = std::filesystem::temp_directory_path() / "omnix_editor_unsaved_scene_validation.omnixscene";
        if (!SceneSerializer::SaveScene(&scene, tempPath.string())) {
            NotifyError("Validate Scene failed: could not serialize unsaved scene for validation.");
            return false;
        }

        SceneValidator validator;
        auto report = validator.ValidateSceneFile(tempPath.string(), m_Context.assetRegistry, &PrefabRegistry::Get());
        sceneManager->SetLastValidationReport(report);

        if (report.HasErrors()) {
            sceneManager->TriggerValidationFailedModal();
            NotifyError("Validate Scene failed: in-memory scene has validation errors.");
            return false;
        }

        NotifySuccess("Validate Scene passed for unsaved scene.");
        return true;
    }

    void EditorFileService::NotifySuccess(const char* message) const {
        CORE_LOG_INFO("[EditorFileService] %s", message);
    }

    void EditorFileService::NotifyError(const char* message) const {
        CORE_LOG_WARN("[EditorFileService] %s", message);
    }
}
