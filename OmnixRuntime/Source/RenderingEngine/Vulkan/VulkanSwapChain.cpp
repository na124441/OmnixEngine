#include "RenderingEngine/Vulkan/VulkanSwapChain.h"
#include "RenderingEngine/Vulkan/VulkanDevice.h"
#include "Core/Log/Log.h"
#include <algorithm>

namespace eng::vulkan {

    VulkanSwapChain::VulkanSwapChain() = default;

    VulkanSwapChain::~VulkanSwapChain() {
        Shutdown();
    }

    eng::core::Result VulkanSwapChain::Initialize(VulkanDevice* device, VkSurfaceKHR surface, uint32_t width, uint32_t height) {
        m_Device = device;

        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->GetPhysicalDevice(), surface, &capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device->GetPhysicalDevice(), surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        if (formatCount != 0) {
            vkGetPhysicalDeviceSurfaceFormatsKHR(device->GetPhysicalDevice(), surface, &formatCount, formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device->GetPhysicalDevice(), surface, &presentModeCount, nullptr);
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        if (presentModeCount != 0) {
            vkGetPhysicalDeviceSurfacePresentModesKHR(device->GetPhysicalDevice(), surface, &presentModeCount, presentModes.data());
        }

        if (formats.empty() || presentModes.empty()) {
            return eng::core::Result(eng::core::ResultCode::Failure);
        }

        VkSurfaceFormatKHR surfaceFormat = formats[0];
        for (const auto& availableFormat : formats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                surfaceFormat = availableFormat;
                break;
            }
        }

        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
        for (const auto& availablePresentMode : presentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                presentMode = availablePresentMode;
                break;
            }
        }

        VkExtent2D extent;
        if (capabilities.currentExtent.width != 0xFFFFFFFF) {
            extent = capabilities.currentExtent;
        } else {
            extent = { width, height };
            extent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, extent.width));
            extent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, extent.height));
        }
        if (extent.width == 0 || extent.height == 0) {
            ENG_LOG_WARN("Skipping swapchain creation for zero-sized surface extent: {}x{}", extent.width, extent.height);
            return eng::core::Result(eng::core::ResultCode::Failure);
        }

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
            imageCount = capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        // Assuming graphics and present queues are the same for simplicity
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.preTransform = capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;

        if (vkCreateSwapchainKHR(m_Device->GetHandle(), &createInfo, nullptr, &m_SwapChain) != VK_SUCCESS) {
            return eng::core::Result(eng::core::ResultCode::Failure);
        }

        vkGetSwapchainImagesKHR(m_Device->GetHandle(), m_SwapChain, &imageCount, nullptr);
        m_Images.resize(imageCount);
        vkGetSwapchainImagesKHR(m_Device->GetHandle(), m_SwapChain, &imageCount, m_Images.data());

        m_ImageFormat = surfaceFormat.format;
        m_Extent = extent;

        return CreateImageViews();
    }

    eng::core::Result VulkanSwapChain::CreateImageViews() {
        m_ImageViews.resize(m_Images.size());

        for (size_t i = 0; i < m_Images.size(); i++) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = m_Images[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = m_ImageFormat;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(m_Device->GetHandle(), &createInfo, nullptr, &m_ImageViews[i]) != VK_SUCCESS) {
                return eng::core::Result(eng::core::ResultCode::Failure);
            }
        }

        return eng::core::Result();
    }

    void VulkanSwapChain::Shutdown() {
        if (!m_Device) return;

        for (auto imageView : m_ImageViews) {
            vkDestroyImageView(m_Device->GetHandle(), imageView, nullptr);
        }
        m_ImageViews.clear();

        if (m_SwapChain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(m_Device->GetHandle(), m_SwapChain, nullptr);
            m_SwapChain = VK_NULL_HANDLE;
        }
    }

} // namespace eng::vulkan
