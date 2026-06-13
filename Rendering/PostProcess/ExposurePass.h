#pragma once
#include <vulkan/vulkan.h>

namespace eng::renderer {

    class ExposurePass {
    public:
        static void Record(VkCommandBuffer cmd);
    };

} // namespace eng::renderer
