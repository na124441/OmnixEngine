#pragma once
#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include "Core/types/Result.h"

namespace eng::vulkan {

    /**
     * @class VulkanInstance
     * @brief Manages the VkInstance and debug messengers.
     */
    class VulkanInstance {
    public:
        VulkanInstance();
        ~VulkanInstance();

        eng::core::Result Initialize(const std::string& appName, bool enableValidation);
        void Shutdown();

        eng::core::Result CreateSurface(void* nativeHandle, VkSurfaceKHR& outSurface);

        VkInstance GetHandle() const { return m_Instance; }

    private:
        VkInstance m_Instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;

        bool CheckValidationLayerSupport();
        std::vector<const char*> GetRequiredExtensions(bool enableValidation);
    };

} // namespace eng::vulkan
