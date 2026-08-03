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

        eng::core::Result AcquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex, uint64_t timeout = UINT64_MAX);
        eng::core::Result Present(VkQueue presentQueue, uint32_t imageIndex, VkSemaphore renderCompleteSemaphore = VK_NULL_HANDLE);
        eng::core::Result Recreate(uint32_t width, uint32_t height);

        VkSwapchainKHR GetHandle() const { return m_SwapChain; }
        VkFormat GetImageFormat() const { return m_ImageFormat; }
        VkExtent2D GetExtent() const { return m_Extent; }
        VkSurfaceKHR GetSurface() const { return m_Surface; }
        VkPresentModeKHR GetPresentMode() const { return m_PresentMode; }
        const std::vector<VkImage>& GetImages() const { return m_Images; }
        const std::vector<VkImageView>& GetImageViews() const { return m_ImageViews; }

    private:
        VulkanDevice* m_Device = nullptr;
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
        VkSwapchainKHR m_SwapChain = VK_NULL_HANDLE;
        VkFormat m_ImageFormat = VK_FORMAT_UNDEFINED;
        VkExtent2D m_Extent{ 0, 0 };
        VkPresentModeKHR m_PresentMode = VK_PRESENT_MODE_FIFO_KHR;
        
        std::vector<VkImage> m_Images;
        std::vector<VkImageView> m_ImageViews;

        eng::core::Result CreateImageViews();
    };

} // namespace eng::vulkan
