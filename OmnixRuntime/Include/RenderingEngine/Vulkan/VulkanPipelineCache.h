#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include "Core/types/Result.h"

namespace eng::vulkan {

    class VulkanPipelineCache {
    public:
        VulkanPipelineCache() = default;
        ~VulkanPipelineCache();

        eng::core::Result Initialize(VkDevice device, const std::string& cacheFilePath = "pipeline_cache.bin");
        void SaveCache();
        void Shutdown();

        VkPipelineCache GetHandle() const { return m_Cache; }

    private:
        VkDevice m_Device = VK_NULL_HANDLE;
        VkPipelineCache m_Cache = VK_NULL_HANDLE;
        std::string m_FilePath = "pipeline_cache.bin";
    };

} // namespace eng::vulkan
