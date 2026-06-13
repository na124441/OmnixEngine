#pragma once
#include <string>
#include <vector>
#include <functional>
#include <vulkan/vulkan.h>
#include "RenderingEngine/Renderer/Pass.h" // For PassID
#include "Rendering/Core/RenderTargetManager.h"

namespace eng::renderer {

    enum class PassResult {
        Success,
        Skipped,
        Failed
    };

    struct RenderPass {
        std::string name;
        std::vector<std::string> inputs;
        std::vector<std::string> outputs;
        PassID physicalSlot = PassID::Geometry;
        std::function<void(VkCommandBuffer)> execute;

        // Validation fields
        bool requiresFramebuffer = false;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        bool requiresPipeline = false;
        VkPipeline pipeline = VK_NULL_HANDLE;
        std::vector<RenderTargetHandle> inputHandles;
        std::vector<RenderTargetHandle> outputHandles;
        std::function<PassResult(VkCommandBuffer)> executeWithResult = nullptr;
    };

} // namespace eng::renderer
