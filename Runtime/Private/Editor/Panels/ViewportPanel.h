#pragma once

#include "Runtime/Public/RuntimeContext.h"
#include <vulkan/vulkan.h>

namespace eng::runtime {

    class ViewportPanel {
    public:
        void Initialize(RuntimeContext* context);
        void Render(VkDescriptorSet viewportTexture, float& outWidth, float& outHeight);

        bool ShowCollidersEnabled() const { return m_ShowColliders; }
        void SetShowColliders(bool show) { m_ShowColliders = show; }

        bool ShowGridEnabled() const { return m_ShowGrid; }
        void SetShowGrid(bool show) { m_ShowGrid = show; }

        bool IsFocused() const { return m_IsFocused; }
        bool IsHovered() const { return m_IsHovered; }

        int GetRenderMode() const { return m_RenderMode; }
        void SetRenderMode(int mode) { m_RenderMode = mode; }

    private:
        RuntimeContext* m_Context = nullptr;
        bool m_ShowColliders = true;
        bool m_ShowGrid = true;
        bool m_IsFocused = false;
        bool m_IsHovered = false;
        int m_RenderMode = 0; // 0 = Lit, 1 = Unlit, 2 = Wireframe
    };

} // namespace eng::runtime
