#pragma once
#include "Vulkan/VulkanSwapChain.h"

namespace eng::renderer {
    using RHISwapchain = eng::vulkan::VulkanSwapChain;
}

namespace eng::rhi {
    using Swapchain = eng::vulkan::VulkanSwapChain;
    using SwapChain = eng::vulkan::VulkanSwapChain;
}
