#pragma once
#include <vulkan/vulkan.h>

namespace eng::renderer {

    class DebugViewRenderer {
    public:
        static void Record(VkCommandBuffer cmd);
    };

} // namespace eng::renderer
