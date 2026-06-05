#pragma once

#include "Runtime/Public/RuntimeContext.h"
#include <vulkan/vulkan.h>

namespace eng::runtime {

    class ViewportPanel {
    public:
        void Initialize(RuntimeContext* context);
        void Render(VkDescriptorSet viewportTexture, float& outWidth, float& outHeight, class EditorSelection& selection, class EditorDirtyState& dirtyState);

        bool ShowCollidersEnabled() const { return m_ShowColliders; }
        void SetShowColliders(bool show) { m_ShowColliders = show; }

        bool ShowGridEnabled() const { return m_ShowGrid; }
        void SetShowGrid(bool show) { m_ShowGrid = show; }

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

    private:
        RuntimeContext* m_Context = nullptr;
        bool m_ShowColliders = true;
        bool m_ShowGrid = true;
        bool m_ShowDiagnostics = false;
        float m_GridScale = 1.0f;
        bool m_IsFocused = false;
        bool m_IsHovered = false;
        int m_RenderMode = 1; // 0 = Scene Lights, 1 = Preview Sunny, 2 = Unlit, 3 = Wireframe
        float m_ViewportScreenX = 0.0f;
        float m_ViewportScreenY = 0.0f;
        float m_ViewportWidth = 1.0f;
        float m_ViewportHeight = 1.0f;
        int m_GizmoType = 7; // ImGuizmo::TRANSLATE
    };

} // namespace eng::runtime
