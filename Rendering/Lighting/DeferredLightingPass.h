#pragma once
#include <vulkan/vulkan.h>

namespace eng::renderer {

    class DeferredLightingPass {
    public:
        static void Record(VkCommandBuffer cmd);
    };

} // namespace eng::renderer
