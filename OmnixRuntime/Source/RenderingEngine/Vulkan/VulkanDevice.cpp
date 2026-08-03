#include "RenderingEngine/Vulkan/VulkanDevice.h"
#include <set>
#include <string>

namespace eng::vulkan {

    VulkanDevice::VulkanDevice() {}

    VulkanDevice::~VulkanDevice() {
        Shutdown();
    }

    void VulkanDevice::WaitIdle() {
        if (m_Device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(m_Device);
        }
    }

    eng::core::Result VulkanDevice::Initialize(VkInstance instance, VkSurfaceKHR surface) {
        // 1. Pick Physical Device
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0) {
            return eng::core::Result(eng::core::ResultCode::Failure);
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const auto& device : devices) {
            if (IsDeviceSuitable(device, surface)) {
                m_PhysicalDevice = device;
                break;
            }
        }

        if (m_PhysicalDevice == VK_NULL_HANDLE) {
            return eng::core::Result(eng::core::ResultCode::Failure);
        }

        // 2. Create Logical Device
        QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice, surface);
        m_GraphicsFamily = indices.graphicsFamily;

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily, indices.presentFamily };

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        // Query supported physical device features
        VkPhysicalDeviceFeatures2 supportedFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
        VkPhysicalDeviceVulkan12Features supportedFeatures12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
        supportedFeatures.pNext = &supportedFeatures12;
        vkGetPhysicalDeviceFeatures2(m_PhysicalDevice, &supportedFeatures);

        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = supportedFeatures.features.samplerAnisotropy;
        deviceFeatures.multiDrawIndirect = supportedFeatures.features.multiDrawIndirect;
        deviceFeatures.drawIndirectFirstInstance = supportedFeatures.features.drawIndirectFirstInstance;

        VkPhysicalDeviceVulkan12Features features12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
        features12.drawIndirectCount = supportedFeatures12.drawIndirectCount;
        features12.bufferDeviceAddress = supportedFeatures12.bufferDeviceAddress;
        features12.descriptorBindingPartiallyBound = supportedFeatures12.descriptorBindingPartiallyBound;
        features12.runtimeDescriptorArray = supportedFeatures12.runtimeDescriptorArray;

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.pNext = &features12;

        const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        createInfo.enabledExtensionCount = 1;
        createInfo.ppEnabledExtensionNames = deviceExtensions;

        // Note: enabledLayerCount and ppEnabledLayerNames are deprecated for VkDevice
        createInfo.enabledLayerCount = 0;

        if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device) != VK_SUCCESS) {
            return eng::core::Result(eng::core::ResultCode::Failure);
        }

        vkGetDeviceQueue(m_Device, indices.graphicsFamily, 0, &m_GraphicsQueue);
        vkGetDeviceQueue(m_Device, indices.presentFamily, 0, &m_PresentQueue);

        return eng::core::Result(eng::core::ResultCode::Success);
    }

    void VulkanDevice::Shutdown() {
        if (m_Device != VK_NULL_HANDLE) {
            vkDestroyDevice(m_Device, nullptr);
            m_Device = VK_NULL_HANDLE;
        }
    }

    bool VulkanDevice::IsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) {
        QueueFamilyIndices indices = FindQueueFamilies(device, surface);
        return indices.IsComplete();
    }

    QueueFamilyIndices VulkanDevice::FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            VkBool32 presentSupport = false;
            if (surface != VK_NULL_HANDLE) {
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            } else {
                presentSupport = true; // For headless testing
            }

            if (presentSupport) {
                indices.presentFamily = i;
            }

            if (indices.IsComplete()) break;
            i++;
        }

        return indices;
    }

} // namespace eng::vulkan
