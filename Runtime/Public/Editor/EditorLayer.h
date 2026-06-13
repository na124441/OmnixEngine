#pragma once

#include "Runtime/Public/RuntimeContext.h"
#include "Runtime/Public/Editor/EditorSelection.h"
#include "Runtime/Public/Editor/EditorDirtyState.h"
#include "Runtime/Public/Editor/EditorNotificationService.h"
#include "Runtime/Private/Editor/Panels/SceneHierarchyPanel.h"
#include "Runtime/Private/Editor/Panels/InspectorPanel.h"
#include "Runtime/Private/Editor/Panels/ConsolePanel.h"
#include "Runtime/Private/Editor/Panels/ViewportPanel.h"
#include "Runtime/Private/Editor/Panels/AssetBrowserPanel.h"
#include "Runtime/Private/Editor/Panels/ImportLogPanel.h"
#include "Runtime/Public/Editor/EditorCamera.h"
#include <memory>
#include <vector>
#include <chrono>
#include "Runtime/Public/Gameplay/Validation/GameplayValidator.h"

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>

class Scene;

namespace eng::runtime {

    class GameMode;

    enum class EditorInputOwner {
        None,
        UI,
        ViewportEditorCamera,
        Game
    };

    class EditorLayer {
    public:
        EditorLayer();
        ~EditorLayer();

        // Subsystem lifecycle
        bool Initialize(RuntimeContext* context);
        void BeginFrame();
        void Render();
        void EndFrame();
        void Shutdown();

        // Draw HUD/Editor on the UI pass command buffer
        void RenderUI(VkCommandBuffer cmd, uint32_t imageIndex);

        // Selection & Dirty Accessors
        EditorSelection& GetSelection() { return m_Selection; }
        EditorDirtyState& GetDirtyState() { return m_DirtyState; }
        EditorNotificationService& GetNotifications() { return m_Notifications; }

        void CreateEntityFromMesh(AssetHandle meshHandle);

    private:
        void RenderDockspace();
        void RecreateFramebuffers(VkExtent2D extent);
        void CleanupFramebuffers();

        bool EnterPlayMode();
        bool ExitPlayMode();

        void DrawGameplayValidatorWindow();
        void DrawGameplayValidatorDiagnostics();

        RuntimeContext* m_Context = nullptr;

        EditorSimulationState m_SimulationState = EditorSimulationState::Edit;
        bool m_EditDirtyBeforePlay = false;
        bool m_RestoreDirtyStateAfterLoad = false;
        int m_PlayStopCycleCount = 0;
        std::chrono::high_resolution_clock::time_point m_PlaySessionStart;
        Scene* m_EditSceneBackup = nullptr;
        std::unique_ptr<eng::runtime::IECSWorld> m_EditWorldBackup = nullptr;
        std::unique_ptr<GameMode> m_GameMode;
        bool m_ShowColliders = true;
        bool m_ShowBounds = false;
        bool m_ShowDiagnostics = false;
        bool m_ResetLayout = false;
        bool m_ShowInteractPrompt = false;

        float m_LastViewportWidth = 1280.0f;
        float m_LastViewportHeight = 720.0f;
        bool m_ResizePending = false;
        uint32_t m_TargetViewportWidth = 0;
        uint32_t m_TargetViewportHeight = 0;
        std::chrono::steady_clock::time_point m_LastResizeRequestTime;
        EditorCamera m_EditorCamera;
        bool m_CursorCaptured = false;
        EditorInputOwner m_InputOwner = EditorInputOwner::None;

        // Subsystems
        EditorSelection m_Selection;
        EditorDirtyState m_DirtyState;
        EditorNotificationService m_Notifications;

        // Panels
        SceneHierarchyPanel m_HierarchyPanel;
        InspectorPanel m_InspectorPanel;
        ConsolePanel m_ConsolePanel;
        ViewportPanel m_ViewportPanel;
        AssetBrowserPanel m_AssetBrowserPanel;
        ImportLogPanel m_ImportLogPanel;

        // Vulkan Resources for ImGui
        VkDescriptorPool m_ImGuiDescriptorPool = VK_NULL_HANDLE;
        VkRenderPass m_UIRenderPass = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> m_UIFramebuffers;
        VkExtent2D m_CurrentExtent = {0, 0};

        // Gameplay Validation
        GameplayValidator m_GameplayValidator;
        std::vector<ValidationResult> m_LastValidationResults;
        bool m_ShowGameplayValidatorWindow = false;
    };

} // namespace eng::runtime
