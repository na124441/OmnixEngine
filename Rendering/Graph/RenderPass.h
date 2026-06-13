#pragma once
#include <string>
#include <vector>
#include <functional>
#include <vulkan/vulkan.h>
#include "RenderingEngine/Renderer/Pass.h" // For PassID

namespace eng::renderer {

    struct RenderPass {
        std::string name;
        std::vector<std::string> inputs;
        std::vector<std::string> outputs;
        PassID physicalSlot = PassID::Geometry;
        std::function<void(VkCommandBuffer)> execute;
    };

} // namespace eng::renderer
