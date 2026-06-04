#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "Core/types/Result.h"

namespace eng::vulkan {

    class VulkanDevice;

    class VulkanSwapChain {
    public:
        VulkanSwapChain();
        ~VulkanSwapChain();

        eng::core::Result Initialize(VulkanDevice* device, VkSurfaceKHR surface, uint32_t width, uint32_t height);
        void Shutdown();

        VkSwapchainKHR GetHandle() const { return m_SwapChain; }
        VkFormat GetImageFormat() const { return m_ImageFormat; }
        VkExtent2D GetExtent() const { return m_Extent; }
        const std::vector<VkImage>& GetImages() const { return m_Images; }
        const std::vector<VkImageView>& GetImageViews() const { return m_ImageViews; }

    private:
        VulkanDevice* m_Device = nullptr;
        VkSwapchainKHR m_SwapChain = VK_NULL_HANDLE;
        VkFormat m_ImageFormat;
        VkExtent2D m_Extent;
        
        std::vector<VkImage> m_Images;
        std::vector<VkImageView> m_ImageViews;

        eng::core::Result CreateImageViews();
    };

} // namespace eng::vulkan
