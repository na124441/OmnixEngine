#pragma once

#include "Runtime/Public/RuntimeContext.h"
#include "Runtime/Public/Editor/EditorSelection.h"
#include "Runtime/Public/Editor/EditorDirtyState.h"
#include "Runtime/Private/Editor/Panels/SceneHierarchyPanel.h"
#include "Runtime/Private/Editor/Panels/InspectorPanel.h"
#include "Runtime/Private/Editor/Panels/ConsolePanel.h"
#include "Runtime/Private/Editor/Panels/ViewportPanel.h"
#include "Runtime/Private/Editor/Panels/AssetBrowserPanel.h"
#include "Runtime/Public/Editor/EditorCamera.h"
#include <memory>
#include <vector>
#include <chrono>

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>

class Scene;

namespace eng::runtime {

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

        void CreateEntityFromMesh(AssetHandle meshHandle);

    private:
        void RenderDockspace();
        void RecreateFramebuffers(VkExtent2D extent);
        void CleanupFramebuffers();

        bool EnterPlayMode();
        bool ExitPlayMode();

        RuntimeContext* m_Context = nullptr;

        EditorSimulationState m_SimulationState = EditorSimulationState::Edit;
        bool m_EditDirtyBeforePlay = false;
        bool m_RestoreDirtyStateAfterLoad = false;
        int m_PlayStopCycleCount = 0;
        std::chrono::high_resolution_clock::time_point m_PlaySessionStart;
        Scene* m_EditSceneBackup = nullptr;
        std::unique_ptr<eng::runtime::IECSWorld> m_EditWorldBackup = nullptr;
        bool m_ShowColliders = true;
        bool m_ShowDiagnostics = false;
        bool m_ResetLayout = false;
        bool m_ShowInteractPrompt = false;

        float m_LastViewportWidth = 1280.0f;
        float m_LastViewportHeight = 720.0f;
        EditorCamera m_EditorCamera;
        bool m_CursorCaptured = false;

        // Subsystems
        EditorSelection m_Selection;
        EditorDirtyState m_DirtyState;

        // Panels
        SceneHierarchyPanel m_HierarchyPanel;
        InspectorPanel m_InspectorPanel;
        ConsolePanel m_ConsolePanel;
        ViewportPanel m_ViewportPanel;
        AssetBrowserPanel m_AssetBrowserPanel;

        // Vulkan Resources for ImGui
        VkDescriptorPool m_ImGuiDescriptorPool = VK_NULL_HANDLE;
        VkRenderPass m_UIRenderPass = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> m_UIFramebuffers;
        VkExtent2D m_CurrentExtent = {0, 0};
    };

} // namespace eng::runtime
