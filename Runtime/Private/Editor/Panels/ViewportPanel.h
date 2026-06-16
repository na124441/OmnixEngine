#pragma once

#include "Runtime/Public/RuntimeContext.h"
#include <vulkan/vulkan.h>

namespace eng::runtime {

    class ViewportPanel {
    public:
        void Initialize(RuntimeContext* context);
        void Render(VkDescriptorSet viewportTexture, float& outWidth, float& outHeight, class EditorSelection& selection, class EditorDirtyState& dirtyState, EditorSimulationState simulationState, float cameraSpeed);

        bool ShowCollidersEnabled() const { return m_ShowColliders; }
        void SetShowColliders(bool show) { m_ShowColliders = show; }

        bool ShowBoundsEnabled() const { return m_ShowBounds; }
        void SetShowBounds(bool show) { m_ShowBounds = show; }

        bool ShowGridEnabled() const { return m_ShowGrid; }
        void SetShowGrid(bool show) { m_ShowGrid = show; }

        bool ShowGizmosEnabled() const { return m_ShowGizmos; }
        void SetShowGizmos(bool show) { m_ShowGizmos = show; }

        bool ShowLightVolumesEnabled() const { return m_ShowLightVolumes; }
        void SetShowLightVolumes(bool show) { m_ShowLightVolumes = show; }

        bool ShowLabelsEnabled() const { return m_ShowLabels; }
        void SetShowLabels(bool show) { m_ShowLabels = show; }

        bool ShowDiagnosticsEnabled() const { return m_ShowDiagnostics; }
        void SetShowDiagnostics(bool show) { m_ShowDiagnostics = show; }

        bool IsFocused() const { return m_IsFocused; }
        bool IsHovered() const { return m_IsHovered; }

        int GetRenderMode() const { return m_RenderMode; }
        void SetRenderMode(int mode) { m_RenderMode = mode; }

        float GetViewportScreenX() const { return m_ViewportScreenX; }
        float GetViewportScreenY() const { return m_ViewportScreenY; }
        float GetViewportWidth() const { return m_ViewportWidth; }
        float GetViewportHeight() const { return m_ViewportHeight; }
        void SetInputDiagnostics(const char* owner, bool cursorCaptured) {
            m_InputOwnerLabel = owner ? owner : "None";
            m_CursorCaptured = cursorCaptured;
        }

    private:
        RuntimeContext* m_Context = nullptr;
        bool m_ShowColliders = true;
        bool m_ShowBounds = false;
        bool m_ShowGrid = true;
        bool m_ShowGizmos = true;
        bool m_ShowLightVolumes = false;
        bool m_ShowLabels = true;
        bool m_ShowDiagnostics = false;
        float m_GridScale = 1.0f;
        bool m_IsFocused = false;
        bool m_IsHovered = false;
        int m_RenderMode = 1; // 0 = Lit, 1 = Preview Lit, 2+ = debug views
        float m_ViewportScreenX = 0.0f;
        float m_ViewportScreenY = 0.0f;
        float m_ViewportWidth = 1.0f;
        float m_ViewportHeight = 1.0f;
        int m_GizmoType = 7; // ImGuizmo::TRANSLATE
        const char* m_InputOwnerLabel = "None";
        bool m_CursorCaptured = false;
    };

} // namespace eng::runtime
