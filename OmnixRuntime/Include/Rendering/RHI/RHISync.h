#pragma once
#include <vulkan/vulkan.h>

namespace eng::renderer {
    using RHIFence = VkFence;
    using RHISemaphore = VkSemaphore;
}

namespace eng::rhi {
    using Fence = VkFence;
    using Semaphore = VkSemaphore;
}
