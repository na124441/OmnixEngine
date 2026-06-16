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

    using ResourceHandle = RenderTargetHandle;

    enum class UsageType {
        Read,
        Write,
        ReadWrite
    };

    struct RenderResourceUsage {
        ResourceHandle resource;
        VkImageLayout requiredLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkPipelineStageFlags stage = 0;
        VkAccessFlags access = 0;
        UsageType usage = UsageType::Read;
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
        std::vector<RenderResourceUsage> resourceUsages;
    };

} // namespace eng::renderer
