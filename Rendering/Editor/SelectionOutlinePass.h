#pragma once
#include <vulkan/vulkan.h>

namespace eng::renderer {

    class SelectionOutlinePass {
    public:
        static void Record(VkCommandBuffer cmd);
    };

} // namespace eng::renderer
