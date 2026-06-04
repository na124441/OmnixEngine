#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "Core/types/Result.h"

#include "RHI/RHIDevice.h"

namespace eng::vulkan {

    /**
     * @struct QueueFamilyIndices
     * @brief Stores indices for different Vulkan queue families.
     */
    struct QueueFamilyIndices {
        uint32_t graphicsFamily = 0xFFFFFFFF;
        uint32_t presentFamily = 0xFFFFFFFF;

        bool IsComplete() const {
            return graphicsFamily != 0xFFFFFFFF && presentFamily != 0xFFFFFFFF;
        }
    };

    /**
     * @class VulkanDevice
     * @brief Manages the VkPhysicalDevice and VkDevice.
     */
    class VulkanDevice : public eng::rhi::Device {
    public:
        VulkanDevice();
        ~VulkanDevice() override;

        eng::core::Result Initialize(VkInstance instance, VkSurfaceKHR surface);
        void Shutdown();

        // RHI::Device implementation
        void WaitIdle() override;

        VkDevice GetHandle() const { return m_Device; }
        VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
        VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
        VkQueue GetPresentQueue() const { return m_PresentQueue; }
        uint32_t GetGraphicsQueueFamily() const { return m_GraphicsFamily; }

    private:
        uint32_t m_GraphicsFamily = 0;
        VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
        VkDevice m_Device = VK_NULL_HANDLE;

        VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
        VkQueue m_PresentQueue = VK_NULL_HANDLE;

        bool IsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface);
        QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
    };

} // namespace eng::vulkan
