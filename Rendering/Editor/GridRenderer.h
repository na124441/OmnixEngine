#pragma once
#include <vulkan/vulkan.h>

namespace eng::renderer {

    class GridRenderer {
    public:
        static void Record(VkCommandBuffer cmd);
    };

} // namespace eng::renderer
