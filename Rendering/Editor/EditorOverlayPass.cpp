#include "Core/pch.h"
#include "EditorOverlayPass.h"
#include "Core/Vulkan/VkUtils.h"
#include "Core/Engine/Log.h"
#include <array>

namespace eng::renderer {

void EditorOverlayPass::Initialize(EngineResources& resources) {
    // No-op for now as pipelines are managed in Renderer
}

void EditorOverlayPass::Shutdown(EngineResources& resources) {
    // No-op for now
}

void EditorOverlayPass::Execute(
    VkCommandBuffer cmd,
    EngineResources& resources,
    uint32_t frameIndex,
    VkRenderPass renderPass,
    VkFramebuffer framebuffer,
    VkExtent2D extent,
    VkDescriptorSet gpuSceneDescriptorSet,
    VkDescriptorSet gbufferDescriptorSet,
    VkPipeline gridPipeline,
    VkPipelineLayout gridPipelineLayout,
    SelectionOutlinePass& selectionOutlinePass,
    uint32_t selectedEntityID,
    const ViewportOverlaySettings& settings
) {
    if (framebuffer == VK_NULL_HANDLE || renderPass == VK_NULL_HANDLE) {
        return;
    }

    // 1. Draw Grid if enabled
    if (settings.showGrid && gridPipeline != VK_NULL_HANDLE) {
        VkRenderPassBeginInfo rpInfo{};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass = renderPass;
        rpInfo.framebuffer = framebuffer;
        rpInfo.renderArea.offset = {0, 0};
        rpInfo.renderArea.extent = extent;
        rpInfo.clearValueCount = 0; // loadOp = LOAD

        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(extent.width);
        viewport.height = static_cast<float>(extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = extent;
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gridPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gridPipelineLayout, 0, 1, &gpuSceneDescriptorSet, 0, nullptr);
        
        vkCmdDraw(cmd, 6, 1, 0, 0);

        vkCmdEndRenderPass(cmd);
    }

    // 2. Draw Selection Outline if enabled and entity is selected
    if (settings.showSelectionOutline && selectedEntityID != 0) {
        selectionOutlinePass.Execute(
            cmd,
            resources,
            frameIndex,
            renderPass,
            framebuffer,
            extent,
            gbufferDescriptorSet,
            selectedEntityID
        );
    }
}

} // namespace eng::renderer
