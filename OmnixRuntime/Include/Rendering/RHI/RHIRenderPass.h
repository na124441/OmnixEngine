#pragma once
#include <vulkan/vulkan.h>

namespace eng::renderer {
    using RHIRenderPass = VkRenderPass;
    using RHIFrambuffer = VkFramebuffer;
}

namespace eng::rhi {
    using RenderPass = VkRenderPass;
    using Framebuffer = VkFramebuffer;
}
