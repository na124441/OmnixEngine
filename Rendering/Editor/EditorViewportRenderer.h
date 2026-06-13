#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "Core/Engine/VmaUsage.h"
#include "RenderingEngine/Core/Engine/EngineResources.h"

namespace eng::renderer {

    class EditorViewportRenderer {
    public:
        EditorViewportRenderer() = default;
        ~EditorViewportRenderer() { cleanup(); }

        void init(EngineResources* res) { resources = res; }
        void cleanup() { destroyOffscreenResources(); }

        void setOffscreenRenderingEnabled(bool enabled);
        bool isOffscreenRenderingEnabled() const { return m_OffscreenRenderingEnabled; }

        void createOffscreenResources(uint32_t width, uint32_t height);
        void destroyOffscreenResources();
        VkDescriptorSet getOffscreenTexture(uint32_t frameIndex) const;

        uint32_t getOffscreenWidth() const { return m_OffscreenWidth; }
        uint32_t getOffscreenHeight() const { return m_OffscreenHeight; }
        VkRenderPass getOffscreenRenderPass() const { return m_OffscreenRenderPass; }
        VkFramebuffer getOffscreenFramebuffer(uint32_t frameIndex) const { 
            if (frameIndex >= m_OffscreenFramebuffers.size()) return VK_NULL_HANDLE;
            return m_OffscreenFramebuffers[frameIndex]; 
        }
        VkImageView getOffscreenDepthImageView(uint32_t frameIndex) const {
            if (frameIndex >= m_OffscreenDepthImageViews.size()) return VK_NULL_HANDLE;
            return m_OffscreenDepthImageViews[frameIndex];
        }
        VkImage getOffscreenDepthImage(uint32_t frameIndex) const {
            if (frameIndex >= m_OffscreenDepthImages.size()) return VK_NULL_HANDLE;
            return m_OffscreenDepthImages[frameIndex];
        }
        VkImage getOffscreenImage(uint32_t frameIndex) const {
            if (frameIndex >= m_OffscreenImages.size()) return VK_NULL_HANDLE;
            return m_OffscreenImages[frameIndex];
        }

    private:
        EngineResources* resources = nullptr;
        bool m_OffscreenRenderingEnabled = false;
        uint32_t m_OffscreenWidth = 1280;
        uint32_t m_OffscreenHeight = 720;
        VkRenderPass m_OffscreenRenderPass = VK_NULL_HANDLE;
        std::vector<VkImage> m_OffscreenImages;
        std::vector<VmaAllocation> m_OffscreenAllocations;
        std::vector<VkImageView> m_OffscreenImageViews;
        std::vector<VkImage> m_OffscreenDepthImages;
        std::vector<VmaAllocation> m_OffscreenDepthAllocations;
        std::vector<VkImageView> m_OffscreenDepthImageViews;
        std::vector<VkFramebuffer> m_OffscreenFramebuffers;
        VkSampler m_OffscreenSampler = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> m_OffscreenImGuiTextures;
    };

} // namespace eng::renderer
