#include "RenderingEngine/Vulkan/VulkanPipelineCache.h"
#include "Core/Log/Log.h"
#include <fstream>
#include <vector>

namespace eng::vulkan {

    VulkanPipelineCache::~VulkanPipelineCache() {
        Shutdown();
    }

    eng::core::Result VulkanPipelineCache::Initialize(VkDevice device, const std::string& cacheFilePath) {
        m_Device = device;
        m_FilePath = cacheFilePath;

        std::vector<char> buffer;
        std::ifstream file(cacheFilePath, std::ios::ate | std::ios::binary);
        if (file.is_open()) {
            size_t size = static_cast<size_t>(file.tellg());
            buffer.resize(size);
            file.seekg(0);
            file.read(buffer.data(), size);
            file.close();
            ENG_LOG_INFO("Loaded pipeline cache binary from '{}' ({} bytes)", cacheFilePath, size);
        }

        VkPipelineCacheCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        createInfo.initialDataSize = buffer.size();
        createInfo.pInitialData = buffer.empty() ? nullptr : buffer.data();

        if (vkCreatePipelineCache(m_Device, &createInfo, nullptr, &m_Cache) != VK_SUCCESS) {
            ENG_LOG_ERROR("Failed to create Vulkan pipeline cache");
            return eng::core::Result(eng::core::ResultCode::Failure);
        }

        return eng::core::Result();
    }

    void VulkanPipelineCache::SaveCache() {
        if (m_Device == VK_NULL_HANDLE || m_Cache == VK_NULL_HANDLE || m_FilePath.empty()) {
            return;
        }

        size_t dataSize = 0;
        if (vkGetPipelineCacheData(m_Device, m_Cache, &dataSize, nullptr) != VK_SUCCESS || dataSize == 0) {
            return;
        }

        std::vector<char> buffer(dataSize);
        if (vkGetPipelineCacheData(m_Device, m_Cache, &dataSize, buffer.data()) == VK_SUCCESS) {
            std::ofstream file(m_FilePath, std::ios::binary);
            if (file.is_open()) {
                file.write(buffer.data(), dataSize);
                file.close();
                ENG_LOG_INFO("Saved pipeline cache binary to '{}' ({} bytes)", m_FilePath, dataSize);
            }
        }
    }

    void VulkanPipelineCache::Shutdown() {
        if (m_Device != VK_NULL_HANDLE && m_Cache != VK_NULL_HANDLE) {
            SaveCache();
            vkDestroyPipelineCache(m_Device, m_Cache, nullptr);
            m_Cache = VK_NULL_HANDLE;
        }
    }

} // namespace eng::vulkan
