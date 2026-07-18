#include "Core/pch.h"
#include "Rendering/Editor/SelectionOutlinePass.h"
#include "Core/Vulkan/VkUtils.h"
#include "Core/Engine/Log.h"
#include <array>

namespace eng::renderer {

void SelectionOutlinePass::Initialize(EngineResources& resources, VkRenderPass renderPass, VkDescriptorSetLayout gbufferLayout) {
    // 1. Create pipeline layout with one descriptor set layout and push constants
    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstant.offset = 0;
    pushConstant.size = 32; // alignas(16) on outlineColor makes struct size 32 bytes to match std140 layout

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &gbufferLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstant;

    VK_CHECK(vkCreatePipelineLayout(resources.device, &layoutInfo, nullptr, &m_PipelineLayout));

    // 2. Create the graphics pipeline
    createPipeline(resources, renderPass);
}

void SelectionOutlinePass::Shutdown(EngineResources& resources) {
    if (m_Pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(resources.device, m_Pipeline, nullptr);
        m_Pipeline = VK_NULL_HANDLE;
    }
    if (m_PipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(resources.device, m_PipelineLayout, nullptr);
        m_PipelineLayout = VK_NULL_HANDLE;
    }
}

void SelectionOutlinePass::createPipeline(EngineResources& resources, VkRenderPass renderPass) {
    VkShaderModule fullscreenVert = resources.loadShaderModule("shaders/fullscreen_vert.spv");
    VkShaderModule selectionFrag = resources.loadShaderModule("shaders/selection_outline_frag.spv");

    if (fullscreenVert == VK_NULL_HANDLE || selectionFrag == VK_NULL_HANDLE) {
        LOG_ERROR("SelectionOutlinePass: Failed to load shaders!");
        if (fullscreenVert != VK_NULL_HANDLE) vkDestroyShaderModule(resources.device, fullscreenVert, nullptr);
        if (selectionFrag != VK_NULL_HANDLE) vkDestroyShaderModule(resources.device, selectionFrag, nullptr);
        return;
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = fullscreenVert;
    stages[0].pName = "main";

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = selectionFrag;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAsm{};
    inputAsm.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAsm.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{};
    viewport.x = 0.0f; viewport.y = 0.0f;
    viewport.width  = (float)resources.swapChainExtent.width;
    viewport.height = (float)resources.swapChainExtent.height;
    viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0,0}; scissor.extent = resources.swapChainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1; viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1; viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
    raster.lineWidth = 1.0f;
    raster.polygonMode = VK_POLYGON_MODE_FILL;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;

    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttachment.blendEnable = VK_FALSE; // No blend needed since non-edges call discard;

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1; colorBlend.pAttachments = &blendAttachment;

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicInfo{};
    dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.dynamicStateCount = 2;
    dynamicInfo.pDynamicStates = dynamicStates;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAsm;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &raster;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicInfo;
    pipelineInfo.layout = m_PipelineLayout;
    pipelineInfo.renderPass = renderPass;

    VK_CHECK(vkCreateGraphicsPipelines(resources.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline));

    vkDestroyShaderModule(resources.device, fullscreenVert, nullptr);
    vkDestroyShaderModule(resources.device, selectionFrag, nullptr);
}

void SelectionOutlinePass::Execute(
    VkCommandBuffer cmd,
    EngineResources& resources,
    uint32_t frameIndex,
    VkRenderPass renderPass,
    VkFramebuffer framebuffer,
    VkExtent2D extent,
    VkDescriptorSet gbufferDescriptorSet,
    uint32_t selectedEntityID
) {
    if (m_Pipeline == VK_NULL_HANDLE || framebuffer == VK_NULL_HANDLE) {
        return;
    }

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = renderPass;
    rpInfo.framebuffer = framebuffer;
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = extent;

    // Load existing color attachment content
    std::array<VkClearValue, 1> clearVals{};
    rpInfo.clearValueCount = 0; // No clear values since loadOp = LOAD

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

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);

    // Bind Set 0: GBuffer textures
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &gbufferDescriptorSet, 0, nullptr);

    // Push Constants: selectedEntityID, outlineThickness, outlineColor
    struct PushConstants {
        uint32_t selectedEntityID;
        float outlineThickness;
        alignas(16) float outlineColor[4];
    } pcs{};
    pcs.selectedEntityID = selectedEntityID;
    pcs.outlineThickness = 1.5f; // default thickness
    pcs.outlineColor[0] = 1.0f;  // warm orange (Red)
    pcs.outlineColor[1] = 0.55f; // Green
    pcs.outlineColor[2] = 0.0f;  // Blue
    pcs.outlineColor[3] = 1.0f;  // Alpha

    vkCmdPushConstants(cmd, m_PipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pcs);

    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRenderPass(cmd);
}

} // namespace eng::renderer
