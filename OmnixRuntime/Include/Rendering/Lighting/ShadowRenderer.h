#pragma once
#include <vulkan/vulkan.h>

namespace eng::renderer {

    class ShadowRenderer {
    public:
        static void Record(VkCommandBuffer cmd);
    };

} // namespace eng::renderer
