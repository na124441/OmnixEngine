#pragma once
#include <vulkan/vulkan.h>

namespace eng::renderer {

    class SkyLightRenderer {
    public:
        static void Record(VkCommandBuffer cmd);
    };

} // namespace eng::renderer
