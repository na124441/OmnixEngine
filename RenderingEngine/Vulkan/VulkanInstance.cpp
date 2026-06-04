#include "VulkanInstance.h"
#include <iostream>
#include <cstring>

namespace eng::vulkan {

    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    VulkanInstance::VulkanInstance() {}

    VulkanInstance::~VulkanInstance() {
        Shutdown();
    }

    eng::core::Result VulkanInstance::Initialize(const std::string& appName, bool enableValidation) {
        if (enableValidation && !CheckValidationLayerSupport()) {
            std::cerr << "Validation layers requested, but not available!" << std::endl;
            enableValidation = false;
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = appName.c_str();
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "Omnix Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_2;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        auto extensions = GetRequiredExtensions(enableValidation);
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        if (enableValidation) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }

        if (vkCreateInstance(&createInfo, nullptr, &m_Instance) != VK_SUCCESS) {
            return eng::core::Result(eng::core::ResultCode::Failure);
        }

        return eng::core::Result(eng::core::ResultCode::Success);
    }

    void VulkanInstance::Shutdown()
    {
        if (m_Instance != VK_NULL_HANDLE) {
            vkDestroyInstance(m_Instance, nullptr);
            m_Instance = VK_NULL_HANDLE;
        }
    }

    eng::core::Result VulkanInstance::CreateSurface(void* nativeHandle, VkSurfaceKHR& outSurface)
    {
#ifdef _WIN32
        VkWin32SurfaceCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.hwnd = static_cast<HWND>(nativeHandle);
        createInfo.hinstance = GetModuleHandle(nullptr);

        if (vkCreateWin32SurfaceKHR(m_Instance, &createInfo, nullptr, &outSurface) != VK_SUCCESS) {
            return eng::core::Result(eng::core::ResultCode::Failure);
        }
        return eng::core::Result();
#else
        return eng::core::Result(eng::core::ResultCode::Failure);
#endif
    }

    bool VulkanInstance::CheckValidationLayerSupport()
 {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : validationLayers) {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) return false;
        }

        return true;
    }

    std::vector<const char*> VulkanInstance::GetRequiredExtensions(bool enableValidation) {
        std::vector<const char*> extensions;
        
        // Basic window system extensions
        extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
        #ifdef _WIN32
        extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
        #endif

        if (enableValidation) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

} // namespace eng::vulkan
