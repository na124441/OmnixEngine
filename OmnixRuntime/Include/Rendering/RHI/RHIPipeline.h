#pragma once
#include <vulkan/vulkan.h>

namespace eng::renderer {

    struct RHIPipeline {
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;
    };

} // namespace eng::renderer

namespace eng::rhi {
    using Pipeline = eng::renderer::RHIPipeline;
}
