#pragma once
#include <vulkan/vulkan.h>
#include "Core/Engine/VmaUsage.h"

namespace eng::renderer {

    struct RHITexture {
        VkImage image = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        uint32_t width = 0;
        uint32_t height = 0;
    };

} // namespace eng::renderer
