#pragma once
#include <vulkan/vulkan.h>
#include "Core/Engine/VmaUsage.h"

namespace eng::renderer {

    struct RHIBuffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VkDeviceSize size = 0;
    };

} // namespace eng::renderer

namespace eng::rhi {
    using Buffer = eng::renderer::RHIBuffer;
}
