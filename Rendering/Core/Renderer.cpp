#include "Core/pch.h"
#include "Renderer.h"
#include "Runtime/Public/OmnixMaterialFormat.h"
#include "Runtime/Public/AssetRegistry.h"
#include "ECS/Public/IECSWorld.h"
#include "ECS/Coordinator.h"
#include "ECS/ECSComponents.h"
#include "Core/World.h"
#include "RenderingEngine/Renderer/Pass.h"
#include "Core/Vulkan/VkUtils.h"
#include "Core/Engine/EngineResources.h"
#include "Core/Engine/VmaHelpers.h"
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/backends/imgui_impl_vulkan.h"
#include "Rendering/Scene/RenderSceneExtractor.h"
#include "Rendering/Scene/GPUScene.h"
#include "Rendering/Geometry/MeshRenderer.h"
#include "Rendering/Geometry/VisibilitySystem.h"
#include <fstream>
#include <filesystem>
#include <unordered_set>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace eng::renderer {

struct PostProcessPushConstants {
    float exposure;
    float gamma;
    float bloomThreshold;
    float bloomIntensity;
    uint32_t exposureMode;
    uint32_t enableTonemapping;
    uint32_t enableGammaCorrection;
    uint32_t debugBeforePostProcess;
    float autoExposure;
    float padding[3];
};

Renderer::~Renderer()
{
    Shutdown();
}

void Renderer::Initialize()
{
    m_CurrentShadowResolution = 2048;
    createShadowResources();

    m_ViewportRenderer.init(&resources);

    gpuScene.Initialize(resources);
    
    // Create m_DepthRenderPass (depth-only)
    VkAttachmentDescription depthAttachmentDesc{};
    depthAttachmentDesc.format = VK_FORMAT_D32_SFLOAT;
    depthAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 0;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription depthSubpass{};
    depthSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    depthSubpass.colorAttachmentCount = 0;
    depthSubpass.pColorAttachments = nullptr;
    depthSubpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkRenderPassCreateInfo depthRenderPassInfo{};
    depthRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    depthRenderPassInfo.attachmentCount = 1;
    depthRenderPassInfo.pAttachments = &depthAttachmentDesc;
    depthRenderPassInfo.subpassCount = 1;
    depthRenderPassInfo.pSubpasses = &depthSubpass;

    VK_CHECK(vkCreateRenderPass(resources.device, &depthRenderPassInfo, nullptr, &m_DepthRenderPass));

    // Create m_GeometryRenderPass (color + depth)
    std::array<VkAttachmentDescription, 2> geomAttachments{};
    
    // Color attachment
    geomAttachments[0].format = resources.swapChainImageFormat;
    geomAttachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    geomAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    geomAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    geomAttachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    geomAttachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    geomAttachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    geomAttachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Depth attachment (load from prepass)
    geomAttachments[1].format = VK_FORMAT_D32_SFLOAT;
    geomAttachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    geomAttachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // LOAD!
    geomAttachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    geomAttachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    geomAttachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    geomAttachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    geomAttachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference geomColorRef{};
    geomColorRef.attachment = 0;
    geomColorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference geomDepthRef{};
    geomDepthRef.attachment = 1;
    geomDepthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription geomSubpass{};
    geomSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    geomSubpass.colorAttachmentCount = 1;
    geomSubpass.pColorAttachments = &geomColorRef;
    geomSubpass.pDepthStencilAttachment = &geomDepthRef;

    VkSubpassDependency geomDependency{};
    geomDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    geomDependency.dstSubpass = 0;
    geomDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    geomDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    geomDependency.srcAccessMask = 0;
    geomDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo geomRenderPassInfo{};
    geomRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    geomRenderPassInfo.attachmentCount = 2;
    geomRenderPassInfo.pAttachments = geomAttachments.data();
    geomRenderPassInfo.subpassCount = 1;
    geomRenderPassInfo.pSubpasses = &geomSubpass;
    geomRenderPassInfo.dependencyCount = 1;
    geomRenderPassInfo.pDependencies = &geomDependency;

    VK_CHECK(vkCreateRenderPass(resources.device, &geomRenderPassInfo, nullptr, &m_GeometryRenderPass));

    // Preserve swapchain render pass
    m_SwapchainRenderPass = resources.renderPass;

    // Create m_GBufferRenderPass
    std::array<VkAttachmentDescription, 5> gbufferAttachments{};
    // 0: GBufferA (Albedo + Material Flags)
    gbufferAttachments[0].format = VK_FORMAT_R8G8B8A8_UNORM;
    gbufferAttachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    gbufferAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    gbufferAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    gbufferAttachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    gbufferAttachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    gbufferAttachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    gbufferAttachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // 1: GBufferB (Normal + Roughness)
    gbufferAttachments[1].format = VK_FORMAT_R16G16B16A16_SFLOAT;
    gbufferAttachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    gbufferAttachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    gbufferAttachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    gbufferAttachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    gbufferAttachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    gbufferAttachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    gbufferAttachments[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // 2: GBufferC (Metallic + AO + Entity ID)
    gbufferAttachments[2].format = VK_FORMAT_R8G8B8A8_UNORM;
    gbufferAttachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
    gbufferAttachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    gbufferAttachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    gbufferAttachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    gbufferAttachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    gbufferAttachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    gbufferAttachments[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // 3: GBufferD (Emissive + Shading Model)
    gbufferAttachments[3].format = VK_FORMAT_R8G8B8A8_UNORM;
    gbufferAttachments[3].samples = VK_SAMPLE_COUNT_1_BIT;
    gbufferAttachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    gbufferAttachments[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    gbufferAttachments[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    gbufferAttachments[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    gbufferAttachments[3].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    gbufferAttachments[3].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // 4: Depth
    gbufferAttachments[4].format = VK_FORMAT_D32_SFLOAT;
    gbufferAttachments[4].samples = VK_SAMPLE_COUNT_1_BIT;
    gbufferAttachments[4].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // loaded from depth prepass
    gbufferAttachments[4].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    gbufferAttachments[4].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    gbufferAttachments[4].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    gbufferAttachments[4].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    gbufferAttachments[4].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    std::array<VkAttachmentReference, 4> colorRefs{};
    for (uint32_t i = 0; i < 4; ++i) {
        colorRefs[i].attachment = i;
        colorRefs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    VkAttachmentReference depthRef{};
    depthRef.attachment = 4;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription gbufferSubpass{};
    gbufferSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    gbufferSubpass.colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
    gbufferSubpass.pColorAttachments = colorRefs.data();
    gbufferSubpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency gbufferDependency{};
    gbufferDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    gbufferDependency.dstSubpass = 0;
    gbufferDependency.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    gbufferDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    gbufferDependency.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    gbufferDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo gbufferRenderPassInfo{};
    gbufferRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    gbufferRenderPassInfo.attachmentCount = static_cast<uint32_t>(gbufferAttachments.size());
    gbufferRenderPassInfo.pAttachments = gbufferAttachments.data();
    gbufferRenderPassInfo.subpassCount = 1;
    gbufferRenderPassInfo.pSubpasses = &gbufferSubpass;
    gbufferRenderPassInfo.dependencyCount = 1;
    gbufferRenderPassInfo.pDependencies = &gbufferDependency;

    VK_CHECK(vkCreateRenderPass(resources.device, &gbufferRenderPassInfo, nullptr, &m_GBufferRenderPass));

    // Swap to new render pass for all subsequent materials/pipelines compatibility
    resources.renderPass = m_GBufferRenderPass;

    // Create GBuffer descriptor set layout
    VkDescriptorSetLayoutBinding gbufferBindings[6]{};
    for (uint32_t i = 0; i < 6; ++i) {
        gbufferBindings[i].binding = i;
        gbufferBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        gbufferBindings[i].descriptorCount = 1;
        gbufferBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        gbufferBindings[i].pImmutableSamplers = nullptr;
    }
    VkDescriptorSetLayoutCreateInfo gbufferSetLayoutInfo{};
    gbufferSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    gbufferSetLayoutInfo.bindingCount = 6;
    gbufferSetLayoutInfo.pBindings = gbufferBindings;
    VK_CHECK(vkCreateDescriptorSetLayout(resources.device, &gbufferSetLayoutInfo, nullptr, &m_GBufferDescriptorSetLayout));

    // Create GBuffer descriptor pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = resources.MAX_FRAMES_IN_FLIGHT * 6;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = resources.MAX_FRAMES_IN_FLIGHT;
    VK_CHECK(vkCreateDescriptorPool(resources.device, &poolInfo, nullptr, &m_GBufferDescriptorPool));

    // Allocate GBuffer descriptor sets
    m_GBufferDescriptorSets.resize(resources.MAX_FRAMES_IN_FLIGHT);
    std::vector<VkDescriptorSetLayout> layouts(resources.MAX_FRAMES_IN_FLIGHT, m_GBufferDescriptorSetLayout);
    VkDescriptorSetAllocateInfo gbufferAllocInfo{};
    gbufferAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    gbufferAllocInfo.descriptorPool = m_GBufferDescriptorPool;
    gbufferAllocInfo.descriptorSetCount = resources.MAX_FRAMES_IN_FLIGHT;
    gbufferAllocInfo.pSetLayouts = layouts.data();
    VK_CHECK(vkAllocateDescriptorSets(resources.device, &gbufferAllocInfo, m_GBufferDescriptorSets.data()));

    // -------------------------------------------------------------------------
    // Create m_HDRRenderPass (1 color attachment of format VK_FORMAT_R16G16B16A16_SFLOAT)
    // -------------------------------------------------------------------------
    VkAttachmentDescription hdrAttachment{};
    hdrAttachment.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    hdrAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    hdrAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    hdrAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    hdrAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    hdrAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    hdrAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    hdrAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference hdrColorRef{};
    hdrColorRef.attachment = 0;
    hdrColorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription hdrSubpass{};
    hdrSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    hdrSubpass.colorAttachmentCount = 1;
    hdrSubpass.pColorAttachments = &hdrColorRef;
    hdrSubpass.pDepthStencilAttachment = nullptr;

    VkSubpassDependency hdrDependency{};
    hdrDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    hdrDependency.dstSubpass = 0;
    hdrDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    hdrDependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    hdrDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    hdrDependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo hdrRpInfo{};
    hdrRpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    hdrRpInfo.attachmentCount = 1;
    hdrRpInfo.pAttachments = &hdrAttachment;
    hdrRpInfo.subpassCount = 1;
    hdrRpInfo.pSubpasses = &hdrSubpass;
    hdrRpInfo.dependencyCount = 1;
    hdrRpInfo.pDependencies = &hdrDependency;

    VK_CHECK(vkCreateRenderPass(resources.device, &hdrRpInfo, nullptr, &m_HDRRenderPass));

    // -------------------------------------------------------------------------
    // Create m_TransparentRenderPass
    // -------------------------------------------------------------------------
    {
        std::array<VkAttachmentDescription, 2> transAttachments{};
        
        // Color attachment (HDR Color)
        transAttachments[0].format = VK_FORMAT_R16G16B16A16_SFLOAT;
        transAttachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        transAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        transAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        transAttachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        transAttachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        transAttachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        transAttachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        // Depth attachment (Depth Buffer)
        transAttachments[1].format = VK_FORMAT_D32_SFLOAT;
        transAttachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        transAttachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        transAttachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        transAttachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        transAttachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        transAttachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        transAttachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference transColorRef{};
        transColorRef.attachment = 0;
        transColorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference transDepthRef{};
        transDepthRef.attachment = 1;
        transDepthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription transSubpass{};
        transSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        transSubpass.colorAttachmentCount = 1;
        transSubpass.pColorAttachments = &transColorRef;
        transSubpass.pDepthStencilAttachment = &transDepthRef;

        VkSubpassDependency transDependency{};
        transDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        transDependency.dstSubpass = 0;
        transDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        transDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        transDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        transDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

        VkRenderPassCreateInfo transRpInfo{};
        transRpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        transRpInfo.attachmentCount = 2;
        transRpInfo.pAttachments = transAttachments.data();
        transRpInfo.subpassCount = 1;
        transRpInfo.pSubpasses = &transSubpass;
        transRpInfo.dependencyCount = 1;
        transRpInfo.pDependencies = &transDependency;

        VK_CHECK(vkCreateRenderPass(resources.device, &transRpInfo, nullptr, &m_TransparentRenderPass));
        resources.transparentRenderPass = m_TransparentRenderPass;
    }

    // -------------------------------------------------------------------------
    // Create m_PostProcessDescriptorSetLayout (binding 0: sampled HDR texture)
    // -------------------------------------------------------------------------
    VkDescriptorSetLayoutBinding postProcessBinding{};
    postProcessBinding.binding = 0;
    postProcessBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    postProcessBinding.descriptorCount = 1;
    postProcessBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    postProcessBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo postProcessLayoutInfo{};
    postProcessLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    postProcessLayoutInfo.bindingCount = 1;
    postProcessLayoutInfo.pBindings = &postProcessBinding;
    VK_CHECK(vkCreateDescriptorSetLayout(resources.device, &postProcessLayoutInfo, nullptr, &m_PostProcessDescriptorSetLayout));

    // Create m_PostProcessDescriptorPool
    VkDescriptorPoolSize postProcessPoolSize{};
    postProcessPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    postProcessPoolSize.descriptorCount = resources.MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo postProcessPoolInfo{};
    postProcessPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    postProcessPoolInfo.poolSizeCount = 1;
    postProcessPoolInfo.pPoolSizes = &postProcessPoolSize;
    postProcessPoolInfo.maxSets = resources.MAX_FRAMES_IN_FLIGHT;
    VK_CHECK(vkCreateDescriptorPool(resources.device, &postProcessPoolInfo, nullptr, &m_PostProcessDescriptorPool));

    // Allocate m_PostProcessDescriptorSets
    m_PostProcessDescriptorSets.resize(resources.MAX_FRAMES_IN_FLIGHT);
    std::vector<VkDescriptorSetLayout> postProcessLayouts(resources.MAX_FRAMES_IN_FLIGHT, m_PostProcessDescriptorSetLayout);
    VkDescriptorSetAllocateInfo postProcessAllocInfo{};
    postProcessAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    postProcessAllocInfo.descriptorPool = m_PostProcessDescriptorPool;
    postProcessAllocInfo.descriptorSetCount = resources.MAX_FRAMES_IN_FLIGHT;
    postProcessAllocInfo.pSetLayouts = postProcessLayouts.data();
    VK_CHECK(vkAllocateDescriptorSets(resources.device, &postProcessAllocInfo, m_PostProcessDescriptorSets.data()));

    // Create m_PostProcessPipelineLayout
    VkPipelineLayoutCreateInfo postProcessPipelineLayoutInfo{};
    postProcessPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    postProcessPipelineLayoutInfo.setLayoutCount = 1;
    postProcessPipelineLayoutInfo.pSetLayouts = &m_PostProcessDescriptorSetLayout;
    VkPushConstantRange postPushRange{};
    postPushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    postPushRange.offset = 0;
    postPushRange.size = sizeof(PostProcessPushConstants);
    postProcessPipelineLayoutInfo.pushConstantRangeCount = 1;
    postProcessPipelineLayoutInfo.pPushConstantRanges = &postPushRange;
    VK_CHECK(vkCreatePipelineLayout(resources.device, &postProcessPipelineLayoutInfo, nullptr, &m_PostProcessPipelineLayout));

    // Create GBuffer Sampler (nearest, clamp-to-edge)
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    VK_CHECK(vkCreateSampler(resources.device, &samplerInfo, nullptr, &m_GBufferSampler));

    resources.globalSetLayout = gpuScene.GetDescriptorSetLayout();

    // Create Deferred Pipeline Layout
    std::vector<VkDescriptorSetLayout> deferredSetLayouts = {
        resources.globalSetLayout,         // Set 0: camera/lights
        m_GBufferDescriptorSetLayout       // Set 1: GBuffer textures
    };
    VkPipelineLayoutCreateInfo deferredLayoutInfo{};
    deferredLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    deferredLayoutInfo.setLayoutCount = static_cast<uint32_t>(deferredSetLayouts.size());
    deferredLayoutInfo.pSetLayouts = deferredSetLayouts.data();
    deferredLayoutInfo.pushConstantRangeCount = 0;
    deferredLayoutInfo.pPushConstantRanges = nullptr;
    VK_CHECK(vkCreatePipelineLayout(resources.device, &deferredLayoutInfo, nullptr, &m_DeferredPipelineLayout));

    resources.createPipelineLayout();

    // 1. Geometry pipeline is already created by legacy engine
    geometryPipeline = resources.graphicsPipeline;

    // 2. Create pipelines for other passes
    initPipelines();

    // 3. Build the render-graph
    setupRenderGraph();

    // 4. Create default fallback assets
    m_DefaultMesh = scene.createMesh();
    const std::vector<Vertex> vertices = {
        {{ 0.0f, -0.5f,  0.0f}, {1.0f, 0.0f, 0.0f}, {0.5f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}
    };
    const std::vector<uint32_t> indices = {
        0,1,2, 0,3,1, 0,2,4, 0,4,3,
        1,3,4, 1,4,2
    };
    m_DefaultMesh->init(vertices.data(), vertices.size(), indices.data(), indices.size(), resources);

    m_DefaultMaterial = scene.createMaterial();
    bool ok = m_DefaultMaterial->create("shaders/gbuffer_vert.spv", 
                                        "shaders/gbuffer_frag.spv", 
                                        "textures/brick_albedo.png", 
                                        "textures/brick_normal.png", 
                                        resources);
    if (!ok) {
        m_DefaultMaterial->setFallbackPipeline(resources.graphicsPipeline);
    }

    initGridPipeline();
}

void Renderer::Shutdown()
{
    destroyGridPipeline();

    if (resources.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(resources.device);
    }
    renderGraph.Shutdown(resources);

    gpuScene.Shutdown(resources);

    if (m_ShadowPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(resources.device, m_ShadowPipeline, nullptr);
        m_ShadowPipeline = VK_NULL_HANDLE;
    }
    if (m_ShadowPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(resources.device, m_ShadowPipelineLayout, nullptr);
        m_ShadowPipelineLayout = VK_NULL_HANDLE;
    }
    destroyShadowResources();

    m_ViewportRenderer.cleanup();

    if (m_GBufferRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(resources.device, m_GBufferRenderPass, nullptr);
        m_GBufferRenderPass = VK_NULL_HANDLE;
    }
    if (m_GBufferSampler != VK_NULL_HANDLE) {
        vkDestroySampler(resources.device, m_GBufferSampler, nullptr);
        m_GBufferSampler = VK_NULL_HANDLE;
    }
    if (m_GBufferDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(resources.device, m_GBufferDescriptorPool, nullptr);
        m_GBufferDescriptorPool = VK_NULL_HANDLE;
    }
    if (m_GBufferDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(resources.device, m_GBufferDescriptorSetLayout, nullptr);
        m_GBufferDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_DeferredPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(resources.device, m_DeferredPipelineLayout, nullptr);
        m_DeferredPipelineLayout = VK_NULL_HANDLE;
    }
    if (m_DeferredLightingPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(resources.device, m_DeferredLightingPipeline, nullptr);
        m_DeferredLightingPipeline = VK_NULL_HANDLE;
    }
    if (m_OffscreenDeferredLightingPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(resources.device, m_OffscreenDeferredLightingPipeline, nullptr);
        m_OffscreenDeferredLightingPipeline = VK_NULL_HANDLE;
    }

    if (m_HDRRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(resources.device, m_HDRRenderPass, nullptr);
        m_HDRRenderPass = VK_NULL_HANDLE;
    }
    if (m_TransparentRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(resources.device, m_TransparentRenderPass, nullptr);
        m_TransparentRenderPass = VK_NULL_HANDLE;
    }
    if (m_PostProcessDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(resources.device, m_PostProcessDescriptorSetLayout, nullptr);
        m_PostProcessDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_PostProcessDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(resources.device, m_PostProcessDescriptorPool, nullptr);
        m_PostProcessDescriptorPool = VK_NULL_HANDLE;
    }
    if (m_PostProcessPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(resources.device, m_PostProcessPipelineLayout, nullptr);
        m_PostProcessPipelineLayout = VK_NULL_HANDLE;
    }
    if (m_PostProcessPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(resources.device, m_PostProcessPipeline, nullptr);
        m_PostProcessPipeline = VK_NULL_HANDLE;
    }
    if (m_OffscreenPostProcessPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(resources.device, m_OffscreenPostProcessPipeline, nullptr);
        m_OffscreenPostProcessPipeline = VK_NULL_HANDLE;
    }

    if (m_DepthPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(resources.device, m_DepthPipeline, nullptr);
        m_DepthPipeline = VK_NULL_HANDLE;
    }
    if (m_DepthRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(resources.device, m_DepthRenderPass, nullptr);
        m_DepthRenderPass = VK_NULL_HANDLE;
    }
    if (m_GeometryRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(resources.device, m_GeometryRenderPass, nullptr);
        m_GeometryRenderPass = VK_NULL_HANDLE;
    }
    recreateDepthResources(0, 0);

    if (shadowPipeline && shadowPipeline != resources.graphicsPipeline) {
        vkDestroyPipeline(resources.device, shadowPipeline, nullptr);
        shadowPipeline = VK_NULL_HANDLE;
    }
    if (lightingPipeline && lightingPipeline != resources.graphicsPipeline) {
        vkDestroyPipeline(resources.device, lightingPipeline, nullptr);
        lightingPipeline = VK_NULL_HANDLE;
    }
    if (postProcessPipeline && postProcessPipeline != resources.graphicsPipeline) {
        vkDestroyPipeline(resources.device, postProcessPipeline, nullptr);
        postProcessPipeline = VK_NULL_HANDLE;
    }
    
    scene.clearObjects();
}

void Renderer::BeginFrame()
{
    // --------------------------------------------------------------
    // 0. Wait for the *previous* frame to finish (fence)
    vkWaitForFences(resources.device, 1,
                    &resources.inFlightFences.at(frameIndex),
                    VK_TRUE, UINT64_MAX);
    renderGraph.CollectGpuTimings(resources, frameIndex);
    m_RenderStats.passTimings = renderGraph.GetLastPassTimings();
    m_RenderStats.gpuFrameTimeMs = renderGraph.GetLastGpuFrameTimeMs();
    m_CpuFrameStart = std::chrono::steady_clock::now();

    // --------------------------------------------------------------
    // 1. Acquire next swap-chain image
    VkResult result = vkAcquireNextImageKHR(resources.device,
                                            resources.swapChain,
                                            UINT64_MAX,
                                            resources.imageAvailableSemaphores.at(frameIndex),
                                            VK_NULL_HANDLE,
                                            &currentSwapchainImageIndex);
    
    m_SwapchainNeedsRecreation = false;
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        if (recreateSwapChainCallback) {
            recreateSwapChainCallback();
        } else {
            onWindowResized();
        }
        return;
    } else if (result == VK_SUBOPTIMAL_KHR) {
        m_SwapchainNeedsRecreation = true;
    } else if (result != VK_SUCCESS) {
        VK_CHECK(result);
    }

    // --------------------------------------------------------------
    // 2. Reset the command pool
    vkResetCommandPool(resources.device,
                       resources.commandPools.at(frameIndex),
                       VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);

    // --------------------------------------------------------------
    // 3. Populate activeFrameContext
    timer.tick();
    activeFrameContext.frameIndex = frameIndex;
    activeFrameContext.deltaTime = timer.deltaMs() * 0.001; // convert ms to seconds
    
    if (m_ViewportRenderer.isOffscreenRenderingEnabled()) {
        activeFrameContext.viewportSize = glm::uvec2(m_ViewportRenderer.getOffscreenWidth(), m_ViewportRenderer.getOffscreenHeight());
    } else {
        activeFrameContext.viewportSize = glm::uvec2(resources.swapChainExtent.width, resources.swapChainExtent.height);
    }
    
    if (frameIndex < resources.commandBuffers.size()) {
        activeFrameContext.commandBuffer = resources.commandBuffers[frameIndex][static_cast<size_t>(PassID::Geometry)];
    } else {
        activeFrameContext.commandBuffer = VK_NULL_HANDLE;
    }
    
    if (currentSwapchainImageIndex < resources.swapChainImages.size()) {
        activeFrameContext.swapchainImage = resources.swapChainImages.at(currentSwapchainImageIndex);
    } else {
        activeFrameContext.swapchainImage = VK_NULL_HANDLE;
    }
    
    if (frameIndex < resources.perFrameData.size()) {
        const FrameData& fd = resources.perFrameData[frameIndex];
        activeFrameContext.uboBuffer = fd.uboBuffer;
        activeFrameContext.uboDescriptor = fd.uboDescriptor;
        activeFrameContext.descriptorPool = fd.descriptorPool;
    }
    if (frameIndex < resources.perFrameLightingData.size()) {
        const auto& lightingData = resources.perFrameLightingData[frameIndex];
        activeFrameContext.lightingUboBuffer = lightingData.uboBuffer;
        activeFrameContext.lightingDescriptor = lightingData.descriptor;
    }
}

void Renderer::RenderFrame(ECSWorld& world, const CameraComponent& cameraComp)
{
    uint32_t targetWidth = 0;
    uint32_t targetHeight = 0;
    if (m_ViewportRenderer.isOffscreenRenderingEnabled()) {
        targetWidth = m_ViewportRenderer.getOffscreenWidth();
        targetHeight = m_ViewportRenderer.getOffscreenHeight();
    } else {
        targetWidth = resources.swapChainExtent.width;
        targetHeight = resources.swapChainExtent.height;
    }
    if (targetWidth != m_DepthWidth || targetHeight != m_DepthHeight) {
        recreateDepthResources(targetWidth, targetHeight);
    }

    m_World = dynamic_cast<eng::runtime::World*>(&world);

    // Derive camera world matrix from the legacy camera object.
    // In Edit mode, EditorLayer sets camera.position/target/up before this call.
    // In Play mode, EditorLayer's play-mode handler does the same.
    // No entity lookup is needed — the camera member is the source of truth.
    glm::mat4 cameraWorldMatrix = glm::inverse(camera.getViewMatrix());

    // 1. Extract scene state into activeRenderScene
    RenderSceneExtractor::ExtractScene(
        world,
        m_AssetRegistry,
        cameraComp,
        cameraWorldMatrix,
        activeRenderScene,
        m_UseEditorDefaultLighting
    );

    activeRenderScene.camera.selectedEntityID = m_SelectedEntityID;

    // Optionally debug print the scene under debug builds
#ifdef OMNIX_DEBUG_RENDERER
    RenderSceneExtractor::DebugPrint(activeRenderScene);
#endif

    // 2. Populate camera matrices in FrameContext
    activeFrameContext.viewMatrix = activeRenderScene.camera.viewMatrix;
    float aspect = activeFrameContext.viewportSize.y > 0 ? (float)activeFrameContext.viewportSize.x / (float)activeFrameContext.viewportSize.y : 16.0f / 9.0f;
    activeFrameContext.projectionMatrix = glm::perspective(
        glm::radians(activeRenderScene.camera.fov),
        aspect,
        activeRenderScene.camera.nearPlane,
        activeRenderScene.camera.farPlane
    );
    activeFrameContext.cameraPosition = activeRenderScene.camera.position;
    activeFrameContext.cameraNear = activeRenderScene.camera.nearPlane;
    activeFrameContext.cameraFar = activeRenderScene.camera.farPlane;
    activeFrameContext.cameraFov = activeRenderScene.camera.fov;

    // Sync legacy camera object for viewport panel queries
    this->camera.position = activeRenderScene.camera.position;
    this->camera.fovY = glm::radians(activeRenderScene.camera.fov);
    this->camera.nearPlane = activeRenderScene.camera.nearPlane;
    this->camera.farPlane = activeRenderScene.camera.farPlane;
    
    // --------------------------------------------------------------
    // 3. Build render-queue
    // --------------------------------------------------------------
    buildRenderQueue();

    // Calculate shadow light space matrix if directional light is present
    if (!activeRenderScene.directionalLights.empty()) {
        auto& dirLight = activeRenderScene.directionalLights[0];
        if (dirLight.castShadows > 0.0f) {
            uint32_t neededRes = static_cast<uint32_t>(dirLight.shadowResolution);
            if (neededRes != m_CurrentShadowResolution) {
                vkDeviceWaitIdle(resources.device);
                destroyShadowResources();
                m_CurrentShadowResolution = neededRes;
                createShadowResources();
                updateGBufferDescriptorSets();
                initPipelines();
            }

            glm::vec3 lightDir = glm::normalize(dirLight.direction);
            glm::vec3 cameraPos = activeRenderScene.camera.position;
            glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
            if (std::abs(glm::dot(lightDir, up)) > 0.99f) {
                up = glm::vec3(0.0f, 0.0f, 1.0f);
            }
            glm::vec3 lightPos = cameraPos - lightDir * 80.0f;
            glm::mat4 lightView = glm::lookAt(lightPos, cameraPos, up);
            glm::mat4 lightProj = glm::ortho(-40.0f, 40.0f, -40.0f, 40.0f, 1.0f, 160.0f);
            lightProj[1][1] *= -1.0f;
            glm::mat4 lightVP = lightProj * lightView;

            dirLight.lightSpaceMatrix = lightVP;
            m_LastLightSpaceMatrix = lightVP;
        }
    }

    std::vector<RenderItem> combinedItems = renderQueue.getItems();
    combinedItems.insert(combinedItems.end(), transparentRenderQueue.getItems().begin(), transparentRenderQueue.getItems().end());

    gpuScene.UpdateFrame(
        resources,
        frameIndex,
        activeRenderScene,
        combinedItems,
        m_EcsMaterialCache,
        m_DefaultMaterial,
        m_ShadingMode
    );

    // Populate FrameContext with GPUScene handles
    const auto& frameRes = gpuScene.GetFrameResources(frameIndex);
    activeFrameContext.uboBuffer = frameRes.cameraBuffer;
    activeFrameContext.uboDescriptor = frameRes.descriptorSet;
    activeFrameContext.descriptorPool = frameRes.descriptorPool;
    activeFrameContext.lightingUboBuffer = frameRes.lightBuffer;
    activeFrameContext.lightingDescriptor = frameRes.descriptorSet;

    // --------------------------------------------------------------
    // 4. Compile and Execute render graph
    renderGraph.Compile(resources);

    static bool s_DebugPrinted = false;
    if (!s_DebugPrinted) {
        renderGraph.PrintDebug();
        s_DebugPrinted = true;
    }

    renderGraph.Execute(resources, frameIndex);
}

void Renderer::EndFrame()
{
    // --------------------------------------------------------------
    // 5. Gather per-pass command buffers
    const auto& perPassCmds = resources.commandBuffers.at(frameIndex);
    std::vector<VkCommandBuffer> submitCmds;
    for (auto cmd : perPassCmds) {
        if (cmd != VK_NULL_HANDLE) submitCmds.push_back(cmd);
    }

    // --------------------------------------------------------------
    // 6. Submit batch
    VkSemaphore waitSem   = resources.imageAvailableSemaphores.at(frameIndex);
    VkSemaphore signalSem = resources.renderFinishedSemaphores.at(frameIndex);
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    VkSubmitInfo submitInfo{};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = &waitSem;
    submitInfo.pWaitDstStageMask    = waitStages;
    submitInfo.commandBufferCount   = static_cast<uint32_t>(submitCmds.size());
    submitInfo.pCommandBuffers      = submitCmds.data();
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &signalSem;

    vkResetFences(resources.device, 1, &resources.inFlightFences.at(frameIndex));
    VK_CHECK(vkQueueSubmit(resources.graphicsQueue,
                            1,
                            &submitInfo,
                            resources.inFlightFences.at(frameIndex)));

    // --------------------------------------------------------------
    // 7. Present
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &signalSem;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &resources.swapChain;
    presentInfo.pImageIndices      = &currentSwapchainImageIndex;

    VkResult result = vkQueuePresentKHR(resources.presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_SwapchainNeedsRecreation) {
        if (recreateSwapChainCallback) {
            recreateSwapChainCallback();
        } else {
            onWindowResized();
        }
    } else {
        VK_CHECK(result);
    }

    const auto cpuFrameEnd = std::chrono::steady_clock::now();
    m_RenderStats.cpuFrameTimeMs = std::chrono::duration<float, std::milli>(cpuFrameEnd - m_CpuFrameStart).count();
    m_RenderStats.renderDocCaptureRequested = m_RenderDocCaptureRequested;
    if (m_RenderDocCaptureRequested) {
        LOG_INFO("RenderDoc capture requested (foundation): capture API integration is not bound yet.");
        m_RenderDocCaptureRequested = false;
    }

    // --------------------------------------------------------------
    // 8. Advance frame index
    frameIndex = (frameIndex + 1) % resources.MAX_FRAMES_IN_FLIGHT;
    timer.tick();
}

void Renderer::drawFrame()
{
    BeginFrame();
    CameraComponent dummyCam;
    dummyCam.fov = glm::degrees(camera.fovY);
    dummyCam.nearPlane = camera.nearPlane;
    dummyCam.farPlane = camera.farPlane;
    if (m_World) {
        RenderFrame(*m_World, dummyCam);
    }
    EndFrame();
}

void Renderer::RenderFrame(const FrameContext& context)
{
    m_World = context.world;
    frameIndex = context.frameIndex;
    
    CameraComponent dummyCam;
    dummyCam.fov = glm::degrees(camera.fovY);
    dummyCam.nearPlane = camera.nearPlane;
    dummyCam.farPlane = camera.farPlane;
    if (m_World) {
        RenderFrame(*m_World, dummyCam);
    }
}

void Renderer::onWindowResized()
{
    if (resources.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(resources.device);
    }
    resources.recreateSwapChain();
    setupRenderGraph();
}

void Renderer::initPipelines()
{
    auto createPipeline = [&](const char* vertPath,
                              const char* fragPath,
                              VkRenderPass rp,
                              VkPipelineLayout layout,
                              VkPipeline* outPipeline)
    {
        VkShaderModule vertModule = resources.loadShaderModule(vertPath);
        VkShaderModule fragModule = resources.loadShaderModule(fragPath);

        if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE) {
            *outPipeline = resources.graphicsPipeline;
            if (vertModule != VK_NULL_HANDLE) vkDestroyShaderModule(resources.device, vertModule, nullptr);
            if (fragModule != VK_NULL_HANDLE) vkDestroyShaderModule(resources.device, fragModule, nullptr);
            return;
        }

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertModule;
        stages[0].pName = "main";

        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragModule;
        stages[1].pName = "main";

        VkVertexInputBindingDescription bindingDesc{};
        bindingDesc.binding = 0;
        bindingDesc.stride = sizeof(Vertex);
        bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attrDescs[3]{};
        attrDescs[0].binding = 0; attrDescs[0].location = 0; attrDescs[0].format = VK_FORMAT_R32G32B32_SFLOAT; attrDescs[0].offset = offsetof(Vertex, pos);
        attrDescs[1].binding = 0; attrDescs[1].location = 1; attrDescs[1].format = VK_FORMAT_R32G32B32_SFLOAT; attrDescs[1].offset = offsetof(Vertex, color);
        attrDescs[2].binding = 0; attrDescs[2].location = 2; attrDescs[2].format = VK_FORMAT_R32G32_SFLOAT;    attrDescs[2].offset = offsetof(Vertex, uv);

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
        vertexInputInfo.vertexAttributeDescriptionCount = 3;
        vertexInputInfo.pVertexAttributeDescriptions = attrDescs;

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
        raster.cullMode = VK_CULL_MODE_BACK_BIT;
        raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
        raster.lineWidth = 1.0f;
        raster.polygonMode = VK_POLYGON_MODE_FILL;

        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

        VkPipelineColorBlendAttachmentState blendAttachment{};
        blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        blendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlend{};
        colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlend.attachmentCount = 1; colorBlend.pAttachments = &blendAttachment;

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2; pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAsm;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &raster;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlend;
        pipelineInfo.layout = layout;
        pipelineInfo.renderPass = rp;

        VK_CHECK(vkCreateGraphicsPipelines(resources.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, outPipeline));

        vkDestroyShaderModule(resources.device, vertModule, nullptr);
        vkDestroyShaderModule(resources.device, fragModule, nullptr);
    };

    // Destroy/create shadow pipeline & layout
    if (m_ShadowPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(resources.device, m_ShadowPipeline, nullptr);
        m_ShadowPipeline = VK_NULL_HANDLE;
    }
    if (m_ShadowPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(resources.device, m_ShadowPipelineLayout, nullptr);
        m_ShadowPipelineLayout = VK_NULL_HANDLE;
    }

    std::vector<VkDescriptorSetLayout> shadowSetLayouts = { resources.globalSetLayout };
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.offset = 0;
    pushRange.size = 80; // uint32_t instanceIndex (4) + pad (12) + glm::mat4 lightSpaceMatrix (64) = 80 bytes

    VkPipelineLayoutCreateInfo shadowLayoutInfo{};
    shadowLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    shadowLayoutInfo.setLayoutCount = static_cast<uint32_t>(shadowSetLayouts.size());
    shadowLayoutInfo.pSetLayouts = shadowSetLayouts.data();
    shadowLayoutInfo.pushConstantRangeCount = 1;
    shadowLayoutInfo.pPushConstantRanges = &pushRange;
    VK_CHECK(vkCreatePipelineLayout(resources.device, &shadowLayoutInfo, nullptr, &m_ShadowPipelineLayout));

    VkShaderModule shadowVertModule = resources.loadShaderModule("shaders/shadow_vert.spv");
    if (shadowVertModule != VK_NULL_HANDLE) {
        VkPipelineShaderStageCreateInfo stage{};
        stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        stage.module = shadowVertModule;
        stage.pName = "main";

        VkVertexInputBindingDescription bindingDesc{};
        bindingDesc.binding = 0;
        bindingDesc.stride = sizeof(Vertex);
        bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attrDescs[3]{};
        attrDescs[0].binding = 0; attrDescs[0].location = 0; attrDescs[0].format = VK_FORMAT_R32G32B32_SFLOAT; attrDescs[0].offset = offsetof(Vertex, pos);
        attrDescs[1].binding = 0; attrDescs[1].location = 1; attrDescs[1].format = VK_FORMAT_R32G32B32_SFLOAT; attrDescs[1].offset = offsetof(Vertex, color);
        attrDescs[2].binding = 0; attrDescs[2].location = 2; attrDescs[2].format = VK_FORMAT_R32G32_SFLOAT;    attrDescs[2].offset = offsetof(Vertex, uv);

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
        vertexInputInfo.vertexAttributeDescriptionCount = 3;
        vertexInputInfo.pVertexAttributeDescriptions = attrDescs;

        VkPipelineInputAssemblyStateCreateInfo inputAsm{};
        inputAsm.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAsm.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkViewport viewport{};
        viewport.x = 0.0f; viewport.y = 0.0f;
        viewport.width  = (float)m_CurrentShadowResolution;
        viewport.height = (float)m_CurrentShadowResolution;
        viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0,0}; scissor.extent = {m_CurrentShadowResolution, m_CurrentShadowResolution};

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1; viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1; viewportState.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo raster{};
        raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.cullMode = VK_CULL_MODE_BACK_BIT;
        raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
        raster.lineWidth = 1.0f;
        raster.polygonMode = VK_POLYGON_MODE_FILL;

        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

        VkPipelineColorBlendAttachmentState blendAttachment{};
        blendAttachment.colorWriteMask = 0;
        blendAttachment.blendEnable = VK_FALSE;

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
        pipelineInfo.stageCount = 1;
        pipelineInfo.pStages = &stage;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAsm;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &raster;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlend;
        pipelineInfo.pDynamicState = &dynamicInfo;
        pipelineInfo.layout = m_ShadowPipelineLayout;
        pipelineInfo.renderPass = m_ShadowRenderPass;

        VK_CHECK(vkCreateGraphicsPipelines(resources.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_ShadowPipeline));

        vkDestroyShaderModule(resources.device, shadowVertModule, nullptr);
    } else {
        LOG_ERROR("Failed to load shadow vertex shader shaders/shadow_vert.spv");
    }

    createPipeline("fullscreen_vert.spv", "lighting_frag.spv", resources.renderPass, resources.pipelineLayout, &lightingPipeline);
    createPipeline("fullscreen_vert.spv", "postprocess_frag.spv", resources.renderPass, resources.pipelineLayout, &postProcessPipeline);

    // Create depth prepass pipeline
    if (m_DepthPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(resources.device, m_DepthPipeline, nullptr);
        m_DepthPipeline = VK_NULL_HANDLE;
    }

    VkShaderModule depthVertModule = resources.loadShaderModule("shaders/depth_vert.spv");
    if (depthVertModule != VK_NULL_HANDLE) {
        VkPipelineShaderStageCreateInfo stage{};
        stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        stage.module = depthVertModule;
        stage.pName = "main";

        VkVertexInputBindingDescription bindingDesc{};
        bindingDesc.binding = 0;
        bindingDesc.stride = sizeof(Vertex);
        bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attrDescs[3]{};
        attrDescs[0].binding = 0; attrDescs[0].location = 0; attrDescs[0].format = VK_FORMAT_R32G32B32_SFLOAT; attrDescs[0].offset = offsetof(Vertex, pos);
        attrDescs[1].binding = 0; attrDescs[1].location = 1; attrDescs[1].format = VK_FORMAT_R32G32B32_SFLOAT; attrDescs[1].offset = offsetof(Vertex, color);
        attrDescs[2].binding = 0; attrDescs[2].location = 2; attrDescs[2].format = VK_FORMAT_R32G32_SFLOAT;    attrDescs[2].offset = offsetof(Vertex, uv);

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
        vertexInputInfo.vertexAttributeDescriptionCount = 3;
        vertexInputInfo.pVertexAttributeDescriptions = attrDescs;

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
        raster.cullMode = VK_CULL_MODE_BACK_BIT;
        raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
        raster.lineWidth = 1.0f;
        raster.polygonMode = VK_POLYGON_MODE_FILL;

        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

        VkPipelineColorBlendAttachmentState blendAttachment{};
        blendAttachment.colorWriteMask = 0; // Disable color write
        blendAttachment.blendEnable = VK_FALSE;

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
        pipelineInfo.stageCount = 1; // Vertex stage only
        pipelineInfo.pStages = &stage;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAsm;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &raster;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlend;
        pipelineInfo.pDynamicState = &dynamicInfo;
        pipelineInfo.layout = resources.pipelineLayout;
        pipelineInfo.renderPass = m_DepthRenderPass;

        VK_CHECK(vkCreateGraphicsPipelines(resources.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_DepthPipeline));

        vkDestroyShaderModule(resources.device, depthVertModule, nullptr);
    } else {
        LOG_ERROR("Failed to load depth vertex shader shaders/depth_vert.spv");
    }

    // Destroy existing deferred pipelines
    if (m_DeferredLightingPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(resources.device, m_DeferredLightingPipeline, nullptr);
        m_DeferredLightingPipeline = VK_NULL_HANDLE;
    }
    if (m_OffscreenDeferredLightingPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(resources.device, m_OffscreenDeferredLightingPipeline, nullptr);
        m_OffscreenDeferredLightingPipeline = VK_NULL_HANDLE;
    }

    VkShaderModule fullscreenVert = resources.loadShaderModule("shaders/fullscreen_vert.spv");
    VkShaderModule deferredFrag = resources.loadShaderModule("shaders/deferred_lighting.spv");

    if (fullscreenVert != VK_NULL_HANDLE && deferredFrag != VK_NULL_HANDLE) {
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = fullscreenVert;
        stages[0].pName = "main";

        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = deferredFrag;
        stages[1].pName = "main";

        // Empty vertex input (fullscreen triangle generated dynamically)
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
        blendAttachment.blendEnable = VK_FALSE;

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
        pipelineInfo.layout = m_DeferredPipelineLayout;

        // 1. Swapchain & Offscreen pipelines (both use m_HDRRenderPass now)
        pipelineInfo.renderPass = m_HDRRenderPass;
        if (m_HDRRenderPass != VK_NULL_HANDLE) {
            VK_CHECK(vkCreateGraphicsPipelines(resources.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_DeferredLightingPipeline));
            VK_CHECK(vkCreateGraphicsPipelines(resources.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_OffscreenDeferredLightingPipeline));
        }

        vkDestroyShaderModule(resources.device, fullscreenVert, nullptr);
        vkDestroyShaderModule(resources.device, deferredFrag, nullptr);
    } else {
        LOG_ERROR("Failed to load deferred lighting shaders!");
        if (fullscreenVert != VK_NULL_HANDLE) vkDestroyShaderModule(resources.device, fullscreenVert, nullptr);
        if (deferredFrag != VK_NULL_HANDLE) vkDestroyShaderModule(resources.device, deferredFrag, nullptr);
    }

    // -------------------------------------------------------------------------
    // 3. Post-Process Pipelines
    // -------------------------------------------------------------------------
    if (m_PostProcessPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(resources.device, m_PostProcessPipeline, nullptr);
        m_PostProcessPipeline = VK_NULL_HANDLE;
    }
    if (m_OffscreenPostProcessPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(resources.device, m_OffscreenPostProcessPipeline, nullptr);
        m_OffscreenPostProcessPipeline = VK_NULL_HANDLE;
    }

    {
        VkShaderModule fullscreenVert = resources.loadShaderModule("shaders/fullscreen_vert.spv");
        VkShaderModule postprocessFrag = resources.loadShaderModule("shaders/postprocess_frag.spv");

        if (fullscreenVert != VK_NULL_HANDLE && postprocessFrag != VK_NULL_HANDLE) {
            VkPipelineShaderStageCreateInfo stages[2]{};
            stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
            stages[0].module = fullscreenVert;
            stages[0].pName = "main";

            stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            stages[1].module = postprocessFrag;
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
            blendAttachment.blendEnable = VK_FALSE;

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
            pipelineInfo.layout = m_PostProcessPipelineLayout;

            // 1. Swapchain pipeline
            pipelineInfo.renderPass = m_SwapchainRenderPass;
            if (m_SwapchainRenderPass != VK_NULL_HANDLE) {
                VK_CHECK(vkCreateGraphicsPipelines(resources.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_PostProcessPipeline));
            }

            // 2. Offscreen pipeline
            VkRenderPass offscreenRp = m_ViewportRenderer.getOffscreenRenderPass();
            if (offscreenRp != VK_NULL_HANDLE) {
                pipelineInfo.renderPass = offscreenRp;
                VK_CHECK(vkCreateGraphicsPipelines(resources.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_OffscreenPostProcessPipeline));
            }

            vkDestroyShaderModule(resources.device, fullscreenVert, nullptr);
            vkDestroyShaderModule(resources.device, postprocessFrag, nullptr);
        } else {
            LOG_ERROR("Failed to load post-process shaders!");
            if (fullscreenVert != VK_NULL_HANDLE) vkDestroyShaderModule(resources.device, fullscreenVert, nullptr);
            if (postprocessFrag != VK_NULL_HANDLE) vkDestroyShaderModule(resources.device, postprocessFrag, nullptr);
        }
    }
}

void Renderer::recreateOffscreenPostProcessPipeline()
{
    if (resources.device == VK_NULL_HANDLE) return;

    if (m_OffscreenPostProcessPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(resources.device, m_OffscreenPostProcessPipeline, nullptr);
        m_OffscreenPostProcessPipeline = VK_NULL_HANDLE;
    }

    VkRenderPass offscreenRp = m_ViewportRenderer.getOffscreenRenderPass();
    if (offscreenRp == VK_NULL_HANDLE) return;

    VkShaderModule fullscreenVert = resources.loadShaderModule("shaders/fullscreen_vert.spv");
    VkShaderModule postprocessFrag = resources.loadShaderModule("shaders/postprocess_frag.spv");

    if (fullscreenVert != VK_NULL_HANDLE && postprocessFrag != VK_NULL_HANDLE) {
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = fullscreenVert;
        stages[0].pName = "main";

        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = postprocessFrag;
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
        blendAttachment.blendEnable = VK_FALSE;

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
        pipelineInfo.layout = m_PostProcessPipelineLayout;
        pipelineInfo.renderPass = offscreenRp;

        VK_CHECK(vkCreateGraphicsPipelines(resources.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_OffscreenPostProcessPipeline));

        vkDestroyShaderModule(resources.device, fullscreenVert, nullptr);
        vkDestroyShaderModule(resources.device, postprocessFrag, nullptr);
    }
}

void Renderer::setupRenderGraph()
{
    renderGraph.Clear();

    // Declare graph resources (Textures)
    TextureResourceDesc texDesc{};
    texDesc.width = 1920;
    texDesc.height = 1080;
    texDesc.format = VK_FORMAT_R8G8B8A8_UNORM;

    renderGraph.DeclareTexture("ShadowMap", texDesc);
    renderGraph.DeclareTexture("DepthBuffer", texDesc);
    renderGraph.DeclareTexture("GBufferA", texDesc);
    renderGraph.DeclareTexture("GBufferB", texDesc);
    renderGraph.DeclareTexture("GBufferC", texDesc);
    renderGraph.DeclareTexture("GBufferD", texDesc);

    TextureResourceDesc hdrDesc = texDesc;
    hdrDesc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    renderGraph.DeclareTexture("HDRColor", hdrDesc);
    renderGraph.DeclareTexture("BloomExtract", hdrDesc);
    renderGraph.DeclareTexture("BloomBlurA", hdrDesc);
    renderGraph.DeclareTexture("BloomBlurB", hdrDesc);
    renderGraph.DeclareTexture("BloomComposite", hdrDesc);
    renderGraph.DeclareTexture("LDRColor", texDesc);
    renderGraph.DeclareTexture("ViewportColor", texDesc);
    renderGraph.DeclareTexture("Swapchain", texDesc);
    renderGraph.DeclareTexture("Backbuffer", texDesc);

    // Register all 9 required passes in dependency order (topological sort will verify)
    
    // 1. Shadow Pass
    renderGraph.RegisterPass(
        "ShadowPass",
        {},
        {"ShadowMap"},
        PassID::Shadow,
        [this](VkCommandBuffer cmd) {
            if (activeRenderScene.directionalLights.empty()) {
                return;
            }
            const auto& dirLight = activeRenderScene.directionalLights[0];
            if (dirLight.castShadows <= 0.0f) {
                return;
            }

            VkImage shadowImg = m_ShadowImages[frameIndex];
            if (shadowImg == VK_NULL_HANDLE) return;

            VkRenderPassBeginInfo rpInfo{};
            rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpInfo.renderPass = m_ShadowRenderPass;
            rpInfo.framebuffer = m_ShadowFramebuffers[frameIndex];
            rpInfo.renderArea.offset = {0, 0};
            rpInfo.renderArea.extent = { m_CurrentShadowResolution, m_CurrentShadowResolution };

            VkClearValue clearValue{};
            clearValue.depthStencil = { 1.0f, 0 };
            rpInfo.clearValueCount = 1;
            rpInfo.pClearValues = &clearValue;

            vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(m_CurrentShadowResolution);
            viewport.height = static_cast<float>(m_CurrentShadowResolution);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(cmd, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = { m_CurrentShadowResolution, m_CurrentShadowResolution };
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            if (m_ShadowPipeline != VK_NULL_HANDLE) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ShadowPipeline);

                VkDescriptorSet gpuSet = gpuScene.GetDescriptorSet(frameIndex);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ShadowPipelineLayout, 0, 1, &gpuSet, 0, nullptr);

                struct ShadowPushConstants {
                    uint32_t instanceIndex;
                    uint32_t pad0, pad1, pad2;
                    glm::mat4 lightSpaceMatrix;
                };

                const auto& items = renderQueue.getItems();
                for (uint32_t i = 0; i < items.size(); ++i) {
                    const RenderItem& item = items[i];
                    if (!item.castShadows) {
                        continue;
                    }

                    ShadowPushConstants push{};
                    push.instanceIndex = i;
                    push.lightSpaceMatrix = m_LastLightSpaceMatrix;

                    vkCmdPushConstants(cmd, m_ShadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowPushConstants), &push);

                    item.mesh->bind(cmd);
                    vkCmdDrawIndexed(cmd, item.mesh->getIndexCount(), 1, 0, 0, 0);
                }
            }

            vkCmdEndRenderPass(cmd);
        }
    );

    // 2. Depth Prepass
    renderGraph.RegisterPass(
        "DepthPrepass",
        {"ShadowMap"},
        {"DepthBuffer"},
        PassID::Geometry,
        [this](VkCommandBuffer cmd) {
            VkRenderPassBeginInfo rpInfo{};
            rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpInfo.renderPass = m_DepthRenderPass;
            if (m_ViewportRenderer.isOffscreenRenderingEnabled() && frameIndex < m_OffscreenDepthFramebuffers.size() && m_OffscreenDepthFramebuffers[frameIndex] != VK_NULL_HANDLE) {
                rpInfo.framebuffer = m_OffscreenDepthFramebuffers[frameIndex];
                rpInfo.renderArea.extent = { m_ViewportRenderer.getOffscreenWidth(), m_ViewportRenderer.getOffscreenHeight() };
            } else {
                rpInfo.framebuffer = frameIndex < m_DepthFramebuffers.size() ? m_DepthFramebuffers[frameIndex] : VK_NULL_HANDLE;
                rpInfo.renderArea.extent = resources.swapChainExtent;
            }

            VkClearValue clearValue{};
            clearValue.depthStencil = { 1.0f, 0 };
            rpInfo.clearValueCount = 1;
            rpInfo.pClearValues = &clearValue;

            vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = m_ViewportRenderer.isOffscreenRenderingEnabled() ? static_cast<float>(m_ViewportRenderer.getOffscreenWidth()) : static_cast<float>(resources.swapChainExtent.width);
            viewport.height = m_ViewportRenderer.isOffscreenRenderingEnabled() ? static_cast<float>(m_ViewportRenderer.getOffscreenHeight()) : static_cast<float>(resources.swapChainExtent.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(cmd, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = m_ViewportRenderer.isOffscreenRenderingEnabled() ? VkExtent2D{m_ViewportRenderer.getOffscreenWidth(), m_ViewportRenderer.getOffscreenHeight()} : resources.swapChainExtent;
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            // Bind GPUScene descriptor set to Set 0
            VkDescriptorSet gpuSet = gpuScene.GetDescriptorSet(frameIndex);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.pipelineLayout, 0, 1, &gpuSet, 0, nullptr);

            // Draw meshes in queue using MeshRenderer with depth pipeline override
            MeshRenderer::DrawQueue(cmd, resources.pipelineLayout, renderQueue, m_DepthPipeline);

            vkCmdEndRenderPass(cmd);
        }
    );

    // 3. GBuffer Pass (performs geometry queue rendering)
    renderGraph.RegisterPass(
        "GBufferPass",
        {"DepthBuffer"},
        {"GBufferA", "GBufferB", "GBufferC", "GBufferD"},
        PassID::Geometry,
        [this](VkCommandBuffer cmd) {
            VkRenderPassBeginInfo rpInfo{};
            rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpInfo.renderPass = m_GBufferRenderPass;
            if (m_ViewportRenderer.isOffscreenRenderingEnabled() && frameIndex < m_OffscreenGBufferFramebuffers.size() && m_OffscreenGBufferFramebuffers[frameIndex] != VK_NULL_HANDLE) {
                rpInfo.framebuffer = m_OffscreenGBufferFramebuffers[frameIndex];
            } else {
                rpInfo.framebuffer = frameIndex < m_GBufferFramebuffers.size() ? m_GBufferFramebuffers[frameIndex] : VK_NULL_HANDLE;
            }
            rpInfo.renderArea.offset = {0, 0};
            rpInfo.renderArea.extent = { m_DepthWidth, m_DepthHeight };

            std::array<VkClearValue, 5> clearVals{};
            clearVals[0].color = {{0.035f, 0.040f, 0.050f, 1.0f}};
            clearVals[1].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
            clearVals[2].color = {{0.0f, 1.0f, 0.0f, 1.0f}};
            clearVals[3].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
            clearVals[4].depthStencil = {1.0f, 0};
            rpInfo.clearValueCount = 5;
            rpInfo.pClearValues = clearVals.data();

            vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(m_DepthWidth);
            viewport.height = static_cast<float>(m_DepthHeight);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(cmd, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = { m_DepthWidth, m_DepthHeight };
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            // Bind GPUScene descriptor set to Set 0
            VkDescriptorSet gpuSet = gpuScene.GetDescriptorSet(frameIndex);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.pipelineLayout, 0, 1, &gpuSet, 0, nullptr);

            // Draw meshes in queue using MeshRenderer
            MeshRenderer::DrawQueue(cmd, resources.pipelineLayout, renderQueue);

            vkCmdEndRenderPass(cmd);
        }
    );

    // 4. Deferred Lighting Pass
    renderGraph.RegisterPass(
        "DeferredLightingPass",
        {"GBufferA", "GBufferB", "GBufferC", "GBufferD", "DepthBuffer"},
        {"HDRColor"},
        PassID::Lighting,
        [this](VkCommandBuffer cmd) {
            VkRenderPassBeginInfo rpInfo{};
            rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpInfo.renderPass = m_HDRRenderPass;
            rpInfo.framebuffer = frameIndex < m_HDRColorFramebuffers.size() ? m_HDRColorFramebuffers[frameIndex] : VK_NULL_HANDLE;
            rpInfo.renderArea.offset = {0, 0};
            rpInfo.renderArea.extent = { m_DepthWidth, m_DepthHeight };

            VkClearValue clearValue{};
            clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
            rpInfo.clearValueCount = 1;
            rpInfo.pClearValues = &clearValue;

            if (rpInfo.framebuffer == VK_NULL_HANDLE || m_DeferredLightingPipeline == VK_NULL_HANDLE) {
                LOG_ERROR("DeferredLightingPass: Framebuffer or pipeline is NULL");
                return;
            }

            // Transition depth buffer to DEPTH_STENCIL_READ_ONLY_OPTIMAL for sampling
            VkImage depthImage = m_ViewportRenderer.isOffscreenRenderingEnabled() ?
                m_ViewportRenderer.getOffscreenDepthImage(frameIndex) : (frameIndex < m_DepthImages.size() ? m_DepthImages[frameIndex] : VK_NULL_HANDLE);

            if (depthImage != VK_NULL_HANDLE) {
                VkImageMemoryBarrier barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = depthImage;
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = 1;
                barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                vkCmdPipelineBarrier(cmd,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &barrier);
            }

            vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(m_DepthWidth);
            viewport.height = static_cast<float>(m_DepthHeight);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(cmd, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = { m_DepthWidth, m_DepthHeight };
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DeferredLightingPipeline);

            // Bind Set 0: GPUScene (camera, lights)
            VkDescriptorSet gpuSet = gpuScene.GetDescriptorSet(frameIndex);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DeferredPipelineLayout, 0, 1, &gpuSet, 0, nullptr);

            // Bind Set 1: GBuffer textures
            VkDescriptorSet gbufferSet = frameIndex < m_GBufferDescriptorSets.size() ? m_GBufferDescriptorSets[frameIndex] : VK_NULL_HANDLE;
            if (gbufferSet != VK_NULL_HANDLE) {
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DeferredPipelineLayout, 1, 1, &gbufferSet, 0, nullptr);
            }

            // Draw fullscreen triangle
            vkCmdDraw(cmd, 3, 1, 0, 0);

            vkCmdEndRenderPass(cmd);

            // Transition depth buffer back to DEPTH_STENCIL_ATTACHMENT_OPTIMAL for subsequent passes
            if (depthImage != VK_NULL_HANDLE) {
                VkImageMemoryBarrier barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = depthImage;
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = 1;
                barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

                vkCmdPipelineBarrier(cmd,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &barrier);
            }
        }
    );

    // 5. Transparent Pass
    renderGraph.RegisterPass(
        "TransparentPass",
        {"DepthBuffer"},
        {"HDRColor"},
        PassID::Lighting,
        [this](VkCommandBuffer cmd) {
            VkRenderPassBeginInfo rpInfo{};
            rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpInfo.renderPass = m_TransparentRenderPass;
            
            if (m_ViewportRenderer.isOffscreenRenderingEnabled() && frameIndex < m_OffscreenTransparentFramebuffers.size() && m_OffscreenTransparentFramebuffers[frameIndex] != VK_NULL_HANDLE) {
                rpInfo.framebuffer = m_OffscreenTransparentFramebuffers[frameIndex];
                rpInfo.renderArea.extent = { m_ViewportRenderer.getOffscreenWidth(), m_ViewportRenderer.getOffscreenHeight() };
            } else {
                rpInfo.framebuffer = frameIndex < m_TransparentFramebuffers.size() ? m_TransparentFramebuffers[frameIndex] : VK_NULL_HANDLE;
                rpInfo.renderArea.extent = { m_DepthWidth, m_DepthHeight };
            }
            rpInfo.renderArea.offset = {0, 0};

            std::array<VkClearValue, 2> clearVals{};
            rpInfo.clearValueCount = 2;
            rpInfo.pClearValues = clearVals.data();

            if (rpInfo.framebuffer == VK_NULL_HANDLE || m_TransparentRenderPass == VK_NULL_HANDLE) {
                return;
            }

            vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = m_ViewportRenderer.isOffscreenRenderingEnabled() ? static_cast<float>(m_ViewportRenderer.getOffscreenWidth()) : static_cast<float>(m_DepthWidth);
            viewport.height = m_ViewportRenderer.isOffscreenRenderingEnabled() ? static_cast<float>(m_ViewportRenderer.getOffscreenHeight()) : static_cast<float>(m_DepthHeight);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(cmd, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = m_ViewportRenderer.isOffscreenRenderingEnabled() ? VkExtent2D{m_ViewportRenderer.getOffscreenWidth(), m_ViewportRenderer.getOffscreenHeight()} : VkExtent2D{m_DepthWidth, m_DepthHeight};
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            // Bind GPUScene descriptor set to Set 0
            VkDescriptorSet gpuSet = gpuScene.GetDescriptorSet(frameIndex);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.pipelineLayout, 0, 1, &gpuSet, 0, nullptr);

            // Draw Infinite Grid
            if (m_GridPipeline != VK_NULL_HANDLE) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GridPipeline);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GridPipelineLayout, 0, 1, &gpuSet, 0, nullptr);
                vkCmdDraw(cmd, 6, 1, 0, 0);
            }

            uint32_t opaqueCount = static_cast<uint32_t>(renderQueue.getItems().size());
            const auto& transItems = transparentRenderQueue.getItems();

            for (uint32_t i = 0; i < transItems.size(); ++i) {
                const RenderItem& item = transItems[i];
                if (!item.mesh || !item.material) continue;

                item.material->bind(cmd, resources.pipelineLayout);

                uint32_t gpuInstanceIndex = opaqueCount + i;
                vkCmdPushConstants(cmd, resources.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t), &gpuInstanceIndex);

                item.mesh->bind(cmd);
                vkCmdDrawIndexed(cmd, item.mesh->getIndexCount(), 1, 0, 0, 0);
            }

            vkCmdEndRenderPass(cmd);
        }
    );

    // 6. Post Process Pass
    renderGraph.RegisterPass(
        "PostProcessPass",
        {"HDRColor"},
        {"LDRColor"},
        PassID::PostProcess,
        [this](VkCommandBuffer cmd) {
            VkRenderPassBeginInfo rpInfo{};
            rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;

            VkPipeline activePipeline = VK_NULL_HANDLE;

            if (m_ViewportRenderer.isOffscreenRenderingEnabled()) {
                rpInfo.renderPass = m_ViewportRenderer.getOffscreenRenderPass();
                rpInfo.framebuffer = m_ViewportRenderer.getOffscreenFramebuffer(frameIndex);
                rpInfo.renderArea.extent = { m_ViewportRenderer.getOffscreenWidth(), m_ViewportRenderer.getOffscreenHeight() };
                activePipeline = m_OffscreenPostProcessPipeline;
            } else {
                rpInfo.renderPass = m_SwapchainRenderPass;
                rpInfo.framebuffer = currentSwapchainImageIndex < resources.swapChainFramebuffers.size() ? resources.swapChainFramebuffers[currentSwapchainImageIndex] : VK_NULL_HANDLE;
                rpInfo.renderArea.extent = resources.swapChainExtent;
                activePipeline = m_PostProcessPipeline;
            }

            if (activePipeline == VK_NULL_HANDLE || rpInfo.framebuffer == VK_NULL_HANDLE) {
                LOG_ERROR("PostProcessPass: activePipeline or framebuffer is NULL");
                return;
            }

            rpInfo.renderArea.offset = {0, 0};

            VkClearValue clearValue{};
            clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
            rpInfo.clearValueCount = 1;
            rpInfo.pClearValues = &clearValue;

            vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = m_ViewportRenderer.isOffscreenRenderingEnabled() ? static_cast<float>(m_ViewportRenderer.getOffscreenWidth()) : static_cast<float>(resources.swapChainExtent.width);
            viewport.height = m_ViewportRenderer.isOffscreenRenderingEnabled() ? static_cast<float>(m_ViewportRenderer.getOffscreenHeight()) : static_cast<float>(resources.swapChainExtent.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(cmd, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = m_ViewportRenderer.isOffscreenRenderingEnabled() ? VkExtent2D{m_ViewportRenderer.getOffscreenWidth(), m_ViewportRenderer.getOffscreenHeight()} : resources.swapChainExtent;
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, activePipeline);

            // Bind Set 0: PostProcess descriptor set (contains HDR texture)
            VkDescriptorSet postProcessSet = frameIndex < m_PostProcessDescriptorSets.size() ? m_PostProcessDescriptorSets[frameIndex] : VK_NULL_HANDLE;
            if (postProcessSet != VK_NULL_HANDLE) {
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PostProcessPipelineLayout, 0, 1, &postProcessSet, 0, nullptr);
            }

            // Auto exposure foundation: keep a stable fallback until luminance reduction is added.
            m_AutoExposure = glm::mix(m_AutoExposure, 1.0f, 0.05f);
            PostProcessPushConstants postConstants{};
            postConstants.exposure = m_PostProcessSettings.exposure;
            postConstants.gamma = m_PostProcessSettings.gamma;
            postConstants.bloomThreshold = m_PostProcessSettings.bloomThreshold;
            postConstants.bloomIntensity = m_PostProcessSettings.bloomIntensity;
            postConstants.exposureMode = static_cast<uint32_t>(m_PostProcessSettings.exposureMode);
            postConstants.enableTonemapping = m_PostProcessSettings.enableTonemapping ? 1u : 0u;
            postConstants.enableGammaCorrection = m_PostProcessSettings.enableGammaCorrection ? 1u : 0u;
            postConstants.debugBeforePostProcess = m_PostProcessSettings.debugBeforePostProcess ? 1u : 0u;
            postConstants.autoExposure = m_AutoExposure;
            vkCmdPushConstants(cmd, m_PostProcessPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PostProcessPushConstants), &postConstants);

            // Draw fullscreen triangle
            vkCmdDraw(cmd, 3, 1, 0, 0);

            vkCmdEndRenderPass(cmd);
        }
    );

    renderGraph.RegisterPass(
        "BloomExtractPass",
        {"HDRColor"},
        {"BloomExtract"},
        PassID::PostProcess,
        [](VkCommandBuffer cmd) {
            (void)cmd;
        }
    );

    renderGraph.RegisterPass(
        "BloomBlurPass",
        {"BloomExtract"},
        {"BloomBlurA", "BloomBlurB"},
        PassID::PostProcess,
        [](VkCommandBuffer cmd) {
            (void)cmd;
        }
    );

    renderGraph.RegisterPass(
        "BloomCompositePass",
        {"HDRColor", "BloomBlurB"},
        {"BloomComposite"},
        PassID::PostProcess,
        [](VkCommandBuffer cmd) {
            (void)cmd;
        }
    );

    // 7. Editor Overlay Pass
    renderGraph.RegisterPass(
        "EditorOverlayPass",
        {"LDRColor"},
        {"ViewportColor"},
        PassID::PostProcess,
        [this](VkCommandBuffer cmd) {
            // Editor grid, selection outline overlays (stub for now)
        }
    );

    // 8. UI Pass (records ImGui UI overlay)
    renderGraph.RegisterPass(
        "UIPass",
        {"ViewportColor"},
        {"Swapchain"},
        PassID::UI,
        [this](VkCommandBuffer cmd) {
            if (m_ViewportRenderer.isOffscreenRenderingEnabled() && currentSwapchainImageIndex < resources.swapChainImages.size()) {
                VkImageMemoryBarrier barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = resources.swapChainImages[currentSwapchainImageIndex];
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = 1;
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                vkCmdPipelineBarrier(cmd,
                                     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                     0,
                                     0, nullptr,
                                     0, nullptr,
                                     1, &barrier);
            }

            if (resources.uiCallback) {
                resources.uiCallback(cmd, currentSwapchainImageIndex);
            }
        }
    );

    // 9. Present Pass (performs swapchain image layout transitions)
    renderGraph.RegisterPass(
        "PresentPass",
        {"Swapchain"},
        {"Backbuffer"},
        PassID::UI,
        [this](VkCommandBuffer cmd) {
            // No transition needed here; UIPass transitions the swapchain image to PRESENT_SRC_KHR beforehand,
            // and ImGui's renderpass transitions it to COLOR_ATTACHMENT_OPTIMAL and back to PRESENT_SRC_KHR on exit.
        }
    );
}

void Renderer::buildRenderQueue()
{
    renderQueue.clear();
    transparentRenderQueue.clear();
    m_StaticRenderCount = 0;
    m_EcsRenderCount = 0;
    m_TransparentRenderCount = 0;
    m_TotalRenderCount = 0;

    // 1. Static loaded scene models
    for (const RenderObject& ro : scene.getObjects()) {
        if (m_LocalViewActive) {
            continue;
        }
        RenderItem item{};
        item.mesh = ro.mesh;
        item.material = ro.material;
        item.transform = ro.transform;
        item.previousTransform = ro.transform;
        item.minBounds = ro.mesh ? ro.mesh->minBounds : glm::vec3(0.0f);
        item.maxBounds = ro.mesh ? ro.mesh->maxBounds : glm::vec3(0.0f);
        item.entityID = 0;
        item.castShadows = ro.castShadows;

        if (item.material && item.material->getBlendMode() == MaterialBlendMode::Blend) {
            transparentRenderQueue.push_back(item);
            m_TransparentRenderCount++;
        } else {
            renderQueue.push_back(item);
            m_StaticRenderCount++;
        }
    }

    // 2. Dynamic ECS entities
    if (m_DefaultMesh != nullptr && m_DefaultMaterial != nullptr) {
        for (const auto& instance : activeRenderScene.meshInstances) {
            if (m_LocalViewActive && instance.entityID != m_LocalViewEntityID) {
                continue;
            }
            RenderItem item{};
            item.mesh = m_DefaultMesh;
            item.material = m_DefaultMaterial;
            item.transform = instance.worldMatrix;
            item.previousTransform = instance.previousWorldMatrix;
            item.minBounds = glm::vec3(instance.worldBounds.min.x, instance.worldBounds.min.y, instance.worldBounds.min.z);
            item.maxBounds = glm::vec3(instance.worldBounds.max.x, instance.worldBounds.max.y, instance.worldBounds.max.z);
            item.entityID = instance.entityID;
            item.castShadows = instance.castShadows;

            // Resolve custom mesh
            if (instance.meshHandle.IsValid()) {
                uint64_t handleVal = instance.meshHandle.value;
                if (m_EcsMeshCache.find(handleVal) != m_EcsMeshCache.end()) {
                    item.mesh = m_EcsMeshCache[handleVal];
                } else if (m_EcsWarningHandles.find(handleVal) == m_EcsWarningHandles.end()) {
                    if (m_AssetRegistry) {
                        const auto* meta = m_AssetRegistry->GetMetadata(instance.meshHandle);
                        if (meta && meta->type == AssetType::Mesh) {
                            Mesh* loadedMesh = nullptr;
                            if (!meta->importedPath.empty() && std::filesystem::exists(meta->importedPath)) {
                                loadedMesh = scene.createMeshFromOmnixMesh(meta->importedPath, resources);
                            } else {
                                loadedMesh = scene.createMeshFromOBJ(meta->sourcePath, resources);
                            }
                            if (loadedMesh) {
                                m_EcsMeshCache[handleVal] = loadedMesh;
                                item.mesh = loadedMesh;
                                m_EcsAssignedMeshCount++;
                            } else {
                                m_EcsWarningHandles.insert(handleVal);
                                m_EcsFallbackMeshCount++;
                            }
                        } else {
                            m_EcsWarningHandles.insert(handleVal);
                            m_EcsFallbackMeshCount++;
                        }
                    }
                }
            }

            // Resolve custom material
            if (instance.materialHandle.IsValid()) {
                uint64_t handleVal = instance.materialHandle.value;

                // Hot reload check
                if (m_AssetRegistry && m_EcsMaterialCache.find(handleVal) != m_EcsMaterialCache.end()) {
                    const auto* meta = m_AssetRegistry->GetMetadata(instance.materialHandle);
                    if (meta && meta->type == AssetType::Material) {
                        try {
                            auto lastWrite = std::filesystem::last_write_time(meta->sourcePath);
                            if (m_MaterialWriteTimes.find(handleVal) != m_MaterialWriteTimes.end()) {
                                if (m_MaterialWriteTimes[handleVal] != lastWrite) {
                                    LOG_INFO("Hot reloading material asset: " + meta->sourcePath);
                                    Material* oldMat = m_EcsMaterialCache[handleVal];
                                    vkDeviceWaitIdle(resources.device); // Safe cleanup
                                    scene.destroyMaterial(oldMat);
                                    m_EcsMaterialCache.erase(handleVal);
                                    m_MaterialWriteTimes.erase(handleVal);
                                }
                            } else {
                                m_MaterialWriteTimes[handleVal] = lastWrite;
                            }
                        } catch (const std::exception& e) {
                            LOG_WARN("Material hot-reload check failed for " + meta->sourcePath + ": " + e.what());
                        }
                    }
                }

                if (m_EcsMaterialCache.find(handleVal) != m_EcsMaterialCache.end()) {
                    item.material = m_EcsMaterialCache[handleVal];
                } else if (m_EcsWarningHandles.find(handleVal) == m_EcsWarningHandles.end()) {
                    if (m_AssetRegistry) {
                        const auto* meta = m_AssetRegistry->GetMetadata(instance.materialHandle);
                        if (meta && meta->type == AssetType::Material) {
                            OmnixMaterial omnixMat;
                            bool deserializeOk = DeserializeMaterial(omnixMat, meta->sourcePath);
                            if (!deserializeOk) {
                                LOG_WARN("Failed to deserialize material file: " + meta->sourcePath);
                            }
                            
                            std::string albedoPath = omnixMat.albedoTexturePath;
                            std::string normalPath = omnixMat.normalTexturePath;
                            std::string metallicRoughnessPath = omnixMat.metallicRoughnessTexturePath;
                            std::string aoPath = omnixMat.aoTexturePath;
                            std::string emissivePath = omnixMat.emissiveTexturePath;

                            if (albedoPath.empty()) {
                                if (meta->sourcePath.find("wood") != std::string::npos) {
                                    albedoPath = "textures/wood_albedo.png";
                                } else {
                                    albedoPath = "textures/brick_albedo.png";
                                    normalPath = "textures/brick_normal.png";
                                }
                            }

                            MaterialAsset assetData{};
                            if (deserializeOk) {
                                assetData.baseColorFactor = omnixMat.baseColorFactor;
                                assetData.metallicFactor = omnixMat.metallicFactor;
                                assetData.roughnessFactor = omnixMat.roughnessFactor;
                                assetData.normalScale = omnixMat.normalScale;
                                assetData.emissiveStrength = omnixMat.emissiveStrength;
                                assetData.blendMode = static_cast<MaterialBlendMode>(omnixMat.blendMode);
                                assetData.shadingModel = static_cast<MaterialShadingModel>(omnixMat.shadingModel);
                            }

                            Material* loadedMat = scene.createMaterial();
                            bool ok = loadedMat->createPBR("shaders/gbuffer_vert.spv",
                                                           "shaders/gbuffer_frag.spv",
                                                           assetData,
                                                           albedoPath,
                                                           normalPath,
                                                           metallicRoughnessPath,
                                                           aoPath,
                                                           emissivePath,
                                                           resources);
                            if (ok) {
                                m_EcsMaterialCache[handleVal] = loadedMat;
                                item.material = loadedMat;
                                // Store write time upon successful creation
                                try {
                                    m_MaterialWriteTimes[handleVal] = std::filesystem::last_write_time(meta->sourcePath);
                                } catch (...) {}
                            } else {
                                scene.destroyMaterial(loadedMat);
                                m_EcsWarningHandles.insert(handleVal);
                            }
                        } else {
                            m_EcsWarningHandles.insert(handleVal);
                        }
                    }
                }
            }

            if (item.material && item.material->getBlendMode() == MaterialBlendMode::Blend) {
                transparentRenderQueue.push_back(item);
                m_TransparentRenderCount++;
            } else {
                renderQueue.push_back(item);
                m_EcsRenderCount++;
            }
        }
    }

    m_TotalRenderCount = m_StaticRenderCount + m_EcsRenderCount;

    // Call geometry sorting to optimize state switches
    VisibilitySystem::CullAndSort(renderQueue);

    // Sort transparent objects back-to-front
    transparentRenderQueue.sortByDistanceBackToFront(activeRenderScene.camera.position);

    updateRenderStats();
}

void Renderer::updateRenderStats()
{
    RenderStats stats = m_RenderStats;
    stats.drawCallCount = 0;
    stats.triangleCount = 0;
    stats.visibleMeshCount = static_cast<uint32_t>(renderQueue.getItems().size() + transparentRenderQueue.getItems().size());
    stats.staticMeshCount = m_StaticRenderCount;
    stats.ecsMeshCount = m_EcsRenderCount;
    stats.transparentObjectCount = m_TransparentRenderCount;
    stats.lightCount = static_cast<uint32_t>(activeRenderScene.directionalLights.size()
                                           + activeRenderScene.pointLights.size()
                                           + activeRenderScene.spotLights.size());
    stats.shadowCasterCount = 0;

    std::unordered_set<const Material*> uniqueMaterials;
    uint32_t opaqueDraws = 0;
    uint32_t transparentDraws = 0;
    auto accumulateItems = [&](const std::vector<RenderItem>& items, bool countShadowCasters) {
        for (const RenderItem& item : items) {
            if (!item.mesh) {
                continue;
            }
            stats.triangleCount += item.mesh->getIndexCount() / 3;
            if (countShadowCasters && item.castShadows) {
                stats.shadowCasterCount += 1;
            }
            if (item.material) {
                uniqueMaterials.insert(item.material);
            }
        }
    };

    accumulateItems(renderQueue.getItems(), true);
    accumulateItems(transparentRenderQueue.getItems(), false);
    opaqueDraws = static_cast<uint32_t>(renderQueue.getItems().size());
    transparentDraws = static_cast<uint32_t>(transparentRenderQueue.getItems().size());
    stats.drawCallCount = (opaqueDraws * 2) + stats.shadowCasterCount + transparentDraws + 2; // depth + GBuffer + shadow + transparent + deferred/post
    if (m_GridPipeline != VK_NULL_HANDLE) {
        stats.drawCallCount += 1;
    }

    stats.materialCount = static_cast<uint32_t>(uniqueMaterials.size());
    stats.textureCount = stats.materialCount * 5;

    const uint64_t pixels = static_cast<uint64_t>(m_DepthWidth) * static_cast<uint64_t>(m_DepthHeight);
    const uint64_t bytesPerPixel = 4 + 8 + 4 + 4 + 4 + 8; // GBuffer A/B/C/D + depth + HDR
    stats.gbufferMemoryMB = static_cast<float>((pixels * bytesPerPixel) / (1024.0 * 1024.0));

    m_RenderStats = std::move(stats);
}

void Renderer::loadModel(const std::string& path)
{
    if (path.find(".gltf") != std::string::npos || path.find(".glb") != std::string::npos) {
        auto model = std::make_unique<GltfModel>();
        if (model->load(path, resources, scene)) {
            gltfModels.push_back(std::move(model));
        }
        return;
    }

    Mesh* m = nullptr;
    if (path.find(".omnixmesh") != std::string::npos) {
        m = scene.createMeshFromOmnixMesh(path, resources);
    } else {
        m = scene.createMeshFromOBJ(path, resources);
    }
    if (m) {
        glm::vec3 size = m->maxBounds - m->minBounds;
        float maxDim = std::max({size.x, size.y, size.z});
        float scale = (maxDim > 0.001f) ? (10.0f / maxDim) : 1.0f;
        glm::vec3 center = (m->minBounds + m->maxBounds) * 0.5f;
        
        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::scale(transform, glm::vec3(scale));
        transform = glm::translate(transform, -center);

        Material* mat = scene.createMaterial();
        bool ok = mat->create("shaders/gbuffer_vert.spv", 
                              "shaders/gbuffer_frag.spv", 
                              "textures/brick_albedo.png", 
                              "textures/brick_normal.png", 
                              resources);
        if (!ok) {
            mat->setFallbackPipeline(resources.graphicsPipeline);
        }
        scene.addObject(m, mat, transform);
    }
}

void Renderer::updateGlobalUBO()
{
}

void Renderer::updateLightingUBO()
{
    LightData uboData{};
    uboData.shadingMode = m_ShadingMode;

    if (!activeRenderScene.directionalLights.empty()) {
        const auto& dirLight = activeRenderScene.directionalLights[0];
        uboData.directionalDirectionIntensity = glm::vec4(dirLight.direction, dirLight.intensity);
        uboData.directionalColor = glm::vec4(dirLight.color, 1.0f);
    } else {
        uboData.directionalDirectionIntensity = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f);
        uboData.directionalColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    uboData.ambientColorIntensity = glm::vec4(activeRenderScene.skyLight.color, activeRenderScene.skyLight.intensity);

    uboData.pointLightCount = std::min(static_cast<uint32_t>(activeRenderScene.pointLights.size()), 16u);
    for (uint32_t i = 0; i < uboData.pointLightCount; ++i) {
        const auto& pt = activeRenderScene.pointLights[i];
        uboData.pointPositionsRadius[i] = glm::vec4(pt.position, pt.radius);
        uboData.pointColorsIntensity[i] = glm::vec4(pt.color, pt.intensity);
    }

    m_LastLightData = uboData;
    m_LastFallbackActive = m_UseEditorDefaultLighting || activeRenderScene.directionalLights.empty();
}

void Renderer::updateGBufferDescriptorSets()
{
    if (resources.device == VK_NULL_HANDLE) return;

    uint32_t maxFrames = resources.MAX_FRAMES_IN_FLIGHT;
    for (uint32_t i = 0; i < maxFrames; ++i) {
        if (m_GBufferAImageViews[i] == VK_NULL_HANDLE || m_GBufferBImageViews[i] == VK_NULL_HANDLE || m_GBufferCImageViews[i] == VK_NULL_HANDLE || m_GBufferDImageViews[i] == VK_NULL_HANDLE || m_DepthImageViews[i] == VK_NULL_HANDLE || m_ShadowImageViews[i] == VK_NULL_HANDLE) {
            continue;
        }
        std::array<VkWriteDescriptorSet, 6> writes{};

        VkDescriptorImageInfo imageInfoA{};
        imageInfoA.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfoA.imageView = m_GBufferAImageViews[i];
        imageInfoA.sampler = m_GBufferSampler;

        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = m_GBufferDescriptorSets[i];
        writes[0].dstBinding = 0;
        writes[0].dstArrayElement = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].descriptorCount = 1;
        writes[0].pImageInfo = &imageInfoA;

        VkDescriptorImageInfo imageInfoB{};
        imageInfoB.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfoB.imageView = m_GBufferBImageViews[i];
        imageInfoB.sampler = m_GBufferSampler;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = m_GBufferDescriptorSets[i];
        writes[1].dstBinding = 1;
        writes[1].dstArrayElement = 0;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo = &imageInfoB;

        VkDescriptorImageInfo imageInfoC{};
        imageInfoC.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfoC.imageView = m_GBufferCImageViews[i];
        imageInfoC.sampler = m_GBufferSampler;

        writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2].dstSet = m_GBufferDescriptorSets[i];
        writes[2].dstBinding = 2;
        writes[2].dstArrayElement = 0;
        writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[2].descriptorCount = 1;
        writes[2].pImageInfo = &imageInfoC;

        VkDescriptorImageInfo imageInfoDepth{};
        imageInfoDepth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        imageInfoDepth.imageView = m_ViewportRenderer.isOffscreenRenderingEnabled() ?
            m_ViewportRenderer.getOffscreenDepthImageView(i) : m_DepthImageViews[i];
        imageInfoDepth.sampler = m_GBufferSampler;

        writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[3].dstSet = m_GBufferDescriptorSets[i];
        writes[3].dstBinding = 3;
        writes[3].dstArrayElement = 0;
        writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[3].descriptorCount = 1;
        writes[3].pImageInfo = &imageInfoDepth;

        VkDescriptorImageInfo imageInfoD{};
        imageInfoD.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfoD.imageView = m_GBufferDImageViews[i];
        imageInfoD.sampler = m_GBufferSampler;

        writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[4].dstSet = m_GBufferDescriptorSets[i];
        writes[4].dstBinding = 4;
        writes[4].dstArrayElement = 0;
        writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[4].descriptorCount = 1;
        writes[4].pImageInfo = &imageInfoD;

        VkDescriptorImageInfo imageInfoShadow{};
        imageInfoShadow.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        imageInfoShadow.imageView = m_ShadowImageViews[i];
        imageInfoShadow.sampler = m_ShadowSampler;

        writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[5].dstSet = m_GBufferDescriptorSets[i];
        writes[5].dstBinding = 5;
        writes[5].dstArrayElement = 0;
        writes[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[5].descriptorCount = 1;
        writes[5].pImageInfo = &imageInfoShadow;

        vkUpdateDescriptorSets(resources.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void Renderer::recreateDepthResources(uint32_t width, uint32_t height)
{
    if (resources.device == VK_NULL_HANDLE) return;

    for (auto fb : m_DepthFramebuffers) {
        if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(resources.device, fb, nullptr);
    }
    m_DepthFramebuffers.clear();

    for (auto fb : m_OffscreenDepthFramebuffers) {
        if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(resources.device, fb, nullptr);
    }
    m_OffscreenDepthFramebuffers.clear();

    for (auto fb : m_GeometryFramebuffers) {
        if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(resources.device, fb, nullptr);
    }
    m_GeometryFramebuffers.clear();

    for (auto fb : m_GBufferFramebuffers) {
        if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(resources.device, fb, nullptr);
    }
    m_GBufferFramebuffers.clear();

    for (auto fb : m_OffscreenGBufferFramebuffers) {
        if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(resources.device, fb, nullptr);
    }
    m_OffscreenGBufferFramebuffers.clear();

    for (auto view : m_DepthImageViews) {
        if (view != VK_NULL_HANDLE) vkDestroyImageView(resources.device, view, nullptr);
    }
    m_DepthImageViews.clear();

    for (auto view : m_GBufferAImageViews) {
        if (view != VK_NULL_HANDLE) vkDestroyImageView(resources.device, view, nullptr);
    }
    m_GBufferAImageViews.clear();

    for (auto view : m_GBufferBImageViews) {
        if (view != VK_NULL_HANDLE) vkDestroyImageView(resources.device, view, nullptr);
    }
    m_GBufferBImageViews.clear();

    for (auto view : m_GBufferCImageViews) {
        if (view != VK_NULL_HANDLE) vkDestroyImageView(resources.device, view, nullptr);
    }
    m_GBufferCImageViews.clear();

    for (auto view : m_GBufferDImageViews) {
        if (view != VK_NULL_HANDLE) vkDestroyImageView(resources.device, view, nullptr);
    }
    m_GBufferDImageViews.clear();

    for (size_t i = 0; i < m_DepthImages.size(); ++i) {
        if (m_DepthImages[i] != VK_NULL_HANDLE) {
            vmaDestroyImage(resources.allocator, m_DepthImages[i], m_DepthAllocations[i]);
            ::eng::ResourceTracker::decImage();
        }
    }
    m_DepthImages.clear();
    m_DepthAllocations.clear();

    for (size_t i = 0; i < m_GBufferAImages.size(); ++i) {
        if (m_GBufferAImages[i] != VK_NULL_HANDLE) {
            vmaDestroyImage(resources.allocator, m_GBufferAImages[i], m_GBufferAAllocations[i]);
            ::eng::ResourceTracker::decImage();
        }
    }
    m_GBufferAImages.clear();
    m_GBufferAAllocations.clear();

    for (size_t i = 0; i < m_GBufferBImages.size(); ++i) {
        if (m_GBufferBImages[i] != VK_NULL_HANDLE) {
            vmaDestroyImage(resources.allocator, m_GBufferBImages[i], m_GBufferBAllocations[i]);
            ::eng::ResourceTracker::decImage();
        }
    }
    m_GBufferBImages.clear();
    m_GBufferBAllocations.clear();

    for (size_t i = 0; i < m_GBufferCImages.size(); ++i) {
        if (m_GBufferCImages[i] != VK_NULL_HANDLE) {
            vmaDestroyImage(resources.allocator, m_GBufferCImages[i], m_GBufferCAllocations[i]);
            ::eng::ResourceTracker::decImage();
        }
    }
    m_GBufferCImages.clear();
    m_GBufferCAllocations.clear();

    for (size_t i = 0; i < m_GBufferDImages.size(); ++i) {
        if (m_GBufferDImages[i] != VK_NULL_HANDLE) {
            vmaDestroyImage(resources.allocator, m_GBufferDImages[i], m_GBufferDAllocations[i]);
            ::eng::ResourceTracker::decImage();
        }
    }
    m_GBufferDImages.clear();
    m_GBufferDAllocations.clear();

    for (auto fb : m_HDRColorFramebuffers) {
        if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(resources.device, fb, nullptr);
    }
    m_HDRColorFramebuffers.clear();

    for (auto fb : m_TransparentFramebuffers) {
        if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(resources.device, fb, nullptr);
    }
    m_TransparentFramebuffers.clear();

    for (auto fb : m_OffscreenTransparentFramebuffers) {
        if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(resources.device, fb, nullptr);
    }
    m_OffscreenTransparentFramebuffers.clear();

    for (auto view : m_HDRColorImageViews) {
        if (view != VK_NULL_HANDLE) vkDestroyImageView(resources.device, view, nullptr);
    }
    m_HDRColorImageViews.clear();

    for (size_t i = 0; i < m_HDRColorImages.size(); ++i) {
        if (m_HDRColorImages[i] != VK_NULL_HANDLE) {
            vmaDestroyImage(resources.allocator, m_HDRColorImages[i], m_HDRColorAllocations[i]);
            ::eng::ResourceTracker::decImage();
        }
    }
    m_HDRColorImages.clear();
    m_HDRColorAllocations.clear();

    if (width == 0 || height == 0) return;

    uint32_t maxFrames = resources.MAX_FRAMES_IN_FLIGHT;
    m_DepthImages.resize(maxFrames, VK_NULL_HANDLE);
    m_DepthAllocations.resize(maxFrames, VK_NULL_HANDLE);
    m_DepthImageViews.resize(maxFrames, VK_NULL_HANDLE);
    m_DepthFramebuffers.resize(maxFrames, VK_NULL_HANDLE);
    m_OffscreenDepthFramebuffers.resize(maxFrames, VK_NULL_HANDLE);
    m_GeometryFramebuffers.resize(maxFrames, VK_NULL_HANDLE);

    m_GBufferAImages.resize(maxFrames, VK_NULL_HANDLE);
    m_GBufferAAllocations.resize(maxFrames, VK_NULL_HANDLE);
    m_GBufferAImageViews.resize(maxFrames, VK_NULL_HANDLE);

    m_GBufferBImages.resize(maxFrames, VK_NULL_HANDLE);
    m_GBufferBAllocations.resize(maxFrames, VK_NULL_HANDLE);
    m_GBufferBImageViews.resize(maxFrames, VK_NULL_HANDLE);

    m_GBufferCImages.resize(maxFrames, VK_NULL_HANDLE);
    m_GBufferCAllocations.resize(maxFrames, VK_NULL_HANDLE);
    m_GBufferCImageViews.resize(maxFrames, VK_NULL_HANDLE);

    m_GBufferDImages.resize(maxFrames, VK_NULL_HANDLE);
    m_GBufferDAllocations.resize(maxFrames, VK_NULL_HANDLE);
    m_GBufferDImageViews.resize(maxFrames, VK_NULL_HANDLE);

    m_GBufferFramebuffers.resize(maxFrames, VK_NULL_HANDLE);
    m_OffscreenGBufferFramebuffers.resize(maxFrames, VK_NULL_HANDLE);

    m_HDRColorImages.resize(maxFrames, VK_NULL_HANDLE);
    m_HDRColorAllocations.resize(maxFrames, VK_NULL_HANDLE);
    m_HDRColorImageViews.resize(maxFrames, VK_NULL_HANDLE);
    m_HDRColorFramebuffers.resize(maxFrames, VK_NULL_HANDLE);

    m_TransparentFramebuffers.resize(maxFrames, VK_NULL_HANDLE);
    m_OffscreenTransparentFramebuffers.resize(maxFrames, VK_NULL_HANDLE);

    for (uint32_t i = 0; i < maxFrames; ++i) {
        VkImageCreateInfo imgInfo{};
        imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.imageType = VK_IMAGE_TYPE_2D;
        imgInfo.format = VK_FORMAT_D32_SFLOAT;
        imgInfo.extent.width = width;
        imgInfo.extent.height = height;
        imgInfo.extent.depth = 1;
        imgInfo.mipLevels = 1;
        imgInfo.arrayLayers = 1;
        imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VK_CHECK(vmaCreateImage(resources.allocator, &imgInfo, &allocInfo, &m_DepthImages[i], &m_DepthAllocations[i], nullptr));
        ::eng::ResourceTracker::incImage();

        // Transition layout to DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        VkCommandBuffer cmd = resources.beginSingleTimeCommands();
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_DepthImages[i];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        resources.endSingleTimeCommands(cmd);

        // View
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_DepthImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_D32_SFLOAT;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(resources.device, &viewInfo, nullptr, &m_DepthImageViews[i]));

        // Create depth prepass framebuffer
        VkImageView depthAttachment = m_DepthImageViews[i];
        VkFramebufferCreateInfo depthFbInfo{};
        depthFbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        depthFbInfo.renderPass = m_DepthRenderPass;
        depthFbInfo.attachmentCount = 1;
        depthFbInfo.pAttachments = &depthAttachment;
        depthFbInfo.width = width;
        depthFbInfo.height = height;
        depthFbInfo.layers = 1;

        VK_CHECK(vkCreateFramebuffer(resources.device, &depthFbInfo, nullptr, &m_DepthFramebuffers[i]));

        // Create offscreen depth prepass framebuffer
        if (m_ViewportRenderer.isOffscreenRenderingEnabled()) {
            VkImageView offscreenDepthAttachment = m_ViewportRenderer.getOffscreenDepthImageView(i);
            if (offscreenDepthAttachment != VK_NULL_HANDLE) {
                VkFramebufferCreateInfo offscreenDepthFbInfo{};
                offscreenDepthFbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                offscreenDepthFbInfo.renderPass = m_DepthRenderPass;
                offscreenDepthFbInfo.attachmentCount = 1;
                offscreenDepthFbInfo.pAttachments = &offscreenDepthAttachment;
                offscreenDepthFbInfo.width = width;
                offscreenDepthFbInfo.height = height;
                offscreenDepthFbInfo.layers = 1;

                VK_CHECK(vkCreateFramebuffer(resources.device, &offscreenDepthFbInfo, nullptr, &m_OffscreenDepthFramebuffers[i]));
            }
        }

        // Create geometry framebuffer
        if (!m_ViewportRenderer.isOffscreenRenderingEnabled() && i < resources.swapChainImageViews.size()) {
            VkImageView geomAttachments[] = { resources.swapChainImageViews[i], m_DepthImageViews[i] };
            VkFramebufferCreateInfo geomFbInfo{};
            geomFbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            geomFbInfo.renderPass = m_GeometryRenderPass;
            geomFbInfo.attachmentCount = 2;
            geomFbInfo.pAttachments = geomAttachments;
            geomFbInfo.width = resources.swapChainExtent.width;
            geomFbInfo.height = resources.swapChainExtent.height;
            geomFbInfo.layers = 1;

            VK_CHECK(vkCreateFramebuffer(resources.device, &geomFbInfo, nullptr, &m_GeometryFramebuffers[i]));
        }

        // Create GBufferA texture (R8G8B8A8_UNORM)
        {
            VkImageCreateInfo gbufferImgInfo{};
            gbufferImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            gbufferImgInfo.imageType = VK_IMAGE_TYPE_2D;
            gbufferImgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
            gbufferImgInfo.extent.width = width;
            gbufferImgInfo.extent.height = height;
            gbufferImgInfo.extent.depth = 1;
            gbufferImgInfo.mipLevels = 1;
            gbufferImgInfo.arrayLayers = 1;
            gbufferImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            gbufferImgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            gbufferImgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            gbufferImgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            gbufferImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            VmaAllocationCreateInfo gbufferAllocInfo{};
            gbufferAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

            VK_CHECK(vmaCreateImage(resources.allocator, &gbufferImgInfo, &gbufferAllocInfo, &m_GBufferAImages[i], &m_GBufferAAllocations[i], nullptr));
            ::eng::ResourceTracker::incImage();

            VkImageViewCreateInfo gbufferViewInfo{};
            gbufferViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            gbufferViewInfo.image = m_GBufferAImages[i];
            gbufferViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            gbufferViewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
            gbufferViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            gbufferViewInfo.subresourceRange.baseMipLevel = 0;
            gbufferViewInfo.subresourceRange.levelCount = 1;
            gbufferViewInfo.subresourceRange.baseArrayLayer = 0;
            gbufferViewInfo.subresourceRange.layerCount = 1;
            VK_CHECK(vkCreateImageView(resources.device, &gbufferViewInfo, nullptr, &m_GBufferAImageViews[i]));
        }

        // Create GBufferB texture (R16G16B16A16_SFLOAT)
        {
            VkImageCreateInfo gbufferImgInfo{};
            gbufferImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            gbufferImgInfo.imageType = VK_IMAGE_TYPE_2D;
            gbufferImgInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
            gbufferImgInfo.extent.width = width;
            gbufferImgInfo.extent.height = height;
            gbufferImgInfo.extent.depth = 1;
            gbufferImgInfo.mipLevels = 1;
            gbufferImgInfo.arrayLayers = 1;
            gbufferImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            gbufferImgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            gbufferImgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            gbufferImgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            gbufferImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            VmaAllocationCreateInfo gbufferAllocInfo{};
            gbufferAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

            VK_CHECK(vmaCreateImage(resources.allocator, &gbufferImgInfo, &gbufferAllocInfo, &m_GBufferBImages[i], &m_GBufferBAllocations[i], nullptr));
            ::eng::ResourceTracker::incImage();

            VkImageViewCreateInfo gbufferViewInfo{};
            gbufferViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            gbufferViewInfo.image = m_GBufferBImages[i];
            gbufferViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            gbufferViewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
            gbufferViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            gbufferViewInfo.subresourceRange.baseMipLevel = 0;
            gbufferViewInfo.subresourceRange.levelCount = 1;
            gbufferViewInfo.subresourceRange.baseArrayLayer = 0;
            gbufferViewInfo.subresourceRange.layerCount = 1;
            VK_CHECK(vkCreateImageView(resources.device, &gbufferViewInfo, nullptr, &m_GBufferBImageViews[i]));
        }

        // Create GBufferC texture (R8G8B8A8_UNORM)
        {
            VkImageCreateInfo gbufferImgInfo{};
            gbufferImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            gbufferImgInfo.imageType = VK_IMAGE_TYPE_2D;
            gbufferImgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
            gbufferImgInfo.extent.width = width;
            gbufferImgInfo.extent.height = height;
            gbufferImgInfo.extent.depth = 1;
            gbufferImgInfo.mipLevels = 1;
            gbufferImgInfo.arrayLayers = 1;
            gbufferImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            gbufferImgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            gbufferImgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            gbufferImgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            gbufferImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            VmaAllocationCreateInfo gbufferAllocInfo{};
            gbufferAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

            VK_CHECK(vmaCreateImage(resources.allocator, &gbufferImgInfo, &gbufferAllocInfo, &m_GBufferCImages[i], &m_GBufferCAllocations[i], nullptr));
            ::eng::ResourceTracker::incImage();

            VkImageViewCreateInfo gbufferViewInfo{};
            gbufferViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            gbufferViewInfo.image = m_GBufferCImages[i];
            gbufferViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            gbufferViewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
            gbufferViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            gbufferViewInfo.subresourceRange.baseMipLevel = 0;
            gbufferViewInfo.subresourceRange.levelCount = 1;
            gbufferViewInfo.subresourceRange.baseArrayLayer = 0;
            gbufferViewInfo.subresourceRange.layerCount = 1;
            VK_CHECK(vkCreateImageView(resources.device, &gbufferViewInfo, nullptr, &m_GBufferCImageViews[i]));
        }

        // Create GBufferD texture (R8G8B8A8_UNORM)
        {
            VkImageCreateInfo gbufferImgInfo{};
            gbufferImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            gbufferImgInfo.imageType = VK_IMAGE_TYPE_2D;
            gbufferImgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
            gbufferImgInfo.extent.width = width;
            gbufferImgInfo.extent.height = height;
            gbufferImgInfo.extent.depth = 1;
            gbufferImgInfo.mipLevels = 1;
            gbufferImgInfo.arrayLayers = 1;
            gbufferImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            gbufferImgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            gbufferImgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            gbufferImgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            gbufferImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            VmaAllocationCreateInfo gbufferAllocInfo{};
            gbufferAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

            VK_CHECK(vmaCreateImage(resources.allocator, &gbufferImgInfo, &gbufferAllocInfo, &m_GBufferDImages[i], &m_GBufferDAllocations[i], nullptr));
            ::eng::ResourceTracker::incImage();

            VkImageViewCreateInfo gbufferViewInfo{};
            gbufferViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            gbufferViewInfo.image = m_GBufferDImages[i];
            gbufferViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            gbufferViewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
            gbufferViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            gbufferViewInfo.subresourceRange.baseMipLevel = 0;
            gbufferViewInfo.subresourceRange.levelCount = 1;
            gbufferViewInfo.subresourceRange.baseArrayLayer = 0;
            gbufferViewInfo.subresourceRange.layerCount = 1;
            VK_CHECK(vkCreateImageView(resources.device, &gbufferViewInfo, nullptr, &m_GBufferDImageViews[i]));
        }

        // Create GBuffer framebuffer (GBufferA + GBufferB + GBufferC + GBufferD + Depth)
        {
            VkImageView attachmentsList[] = {
                m_GBufferAImageViews[i],
                m_GBufferBImageViews[i],
                m_GBufferCImageViews[i],
                m_GBufferDImageViews[i],
                m_DepthImageViews[i]
            };
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_GBufferRenderPass;
            framebufferInfo.attachmentCount = 5;
            framebufferInfo.pAttachments = attachmentsList;
            framebufferInfo.width = width;
            framebufferInfo.height = height;
            framebufferInfo.layers = 1;
            VK_CHECK(vkCreateFramebuffer(resources.device, &framebufferInfo, nullptr, &m_GBufferFramebuffers[i]));
        }

        // Create offscreen GBuffer framebuffer (GBufferA + GBufferB + GBufferC + GBufferD + Offscreen Depth)
        if (m_ViewportRenderer.isOffscreenRenderingEnabled()) {
            VkImageView offscreenGBufferDepth = m_ViewportRenderer.getOffscreenDepthImageView(i);
            if (offscreenGBufferDepth != VK_NULL_HANDLE) {
                VkImageView offscreenAttachmentsList[] = {
                    m_GBufferAImageViews[i],
                    m_GBufferBImageViews[i],
                    m_GBufferCImageViews[i],
                    m_GBufferDImageViews[i],
                    offscreenGBufferDepth
                };
                VkFramebufferCreateInfo offscreenFramebufferInfo{};
                offscreenFramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                offscreenFramebufferInfo.renderPass = m_GBufferRenderPass;
                offscreenFramebufferInfo.attachmentCount = 5;
                offscreenFramebufferInfo.pAttachments = offscreenAttachmentsList;
                offscreenFramebufferInfo.width = width;
                offscreenFramebufferInfo.height = height;
                offscreenFramebufferInfo.layers = 1;
                VK_CHECK(vkCreateFramebuffer(resources.device, &offscreenFramebufferInfo, nullptr, &m_OffscreenGBufferFramebuffers[i]));
            }
        }

        // Create HDR color texture (R16G16B16A16_SFLOAT)
        {
            VkImageCreateInfo hdrImgInfo{};
            hdrImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            hdrImgInfo.imageType = VK_IMAGE_TYPE_2D;
            hdrImgInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
            hdrImgInfo.extent.width = width;
            hdrImgInfo.extent.height = height;
            hdrImgInfo.extent.depth = 1;
            hdrImgInfo.mipLevels = 1;
            hdrImgInfo.arrayLayers = 1;
            hdrImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            hdrImgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            hdrImgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            hdrImgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            hdrImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            VmaAllocationCreateInfo hdrAllocInfo{};
            hdrAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

            VK_CHECK(vmaCreateImage(resources.allocator, &hdrImgInfo, &hdrAllocInfo, &m_HDRColorImages[i], &m_HDRColorAllocations[i], nullptr));
            ::eng::ResourceTracker::incImage();

            VkImageViewCreateInfo hdrViewInfo{};
            hdrViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            hdrViewInfo.image = m_HDRColorImages[i];
            hdrViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            hdrViewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
            hdrViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            hdrViewInfo.subresourceRange.baseMipLevel = 0;
            hdrViewInfo.subresourceRange.levelCount = 1;
            hdrViewInfo.subresourceRange.baseArrayLayer = 0;
            hdrViewInfo.subresourceRange.layerCount = 1;
            VK_CHECK(vkCreateImageView(resources.device, &hdrViewInfo, nullptr, &m_HDRColorImageViews[i]));
        }

        // Create HDR framebuffer (1 attachment m_HDRColorImageViews[i])
        {
            VkImageView attachmentsList[] = {
                m_HDRColorImageViews[i]
            };
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_HDRRenderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachmentsList;
            framebufferInfo.width = width;
            framebufferInfo.height = height;
            framebufferInfo.layers = 1;
            VK_CHECK(vkCreateFramebuffer(resources.device, &framebufferInfo, nullptr, &m_HDRColorFramebuffers[i]));
        }

        // Create Transparent framebuffer (HDR color + Depth buffer)
        {
            VkImageView transAttachmentsList[] = {
                m_HDRColorImageViews[i],
                m_DepthImageViews[i]
            };
            VkFramebufferCreateInfo transFramebufferInfo{};
            transFramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            transFramebufferInfo.renderPass = m_TransparentRenderPass;
            transFramebufferInfo.attachmentCount = 2;
            transFramebufferInfo.pAttachments = transAttachmentsList;
            transFramebufferInfo.width = width;
            transFramebufferInfo.height = height;
            transFramebufferInfo.layers = 1;
            VK_CHECK(vkCreateFramebuffer(resources.device, &transFramebufferInfo, nullptr, &m_TransparentFramebuffers[i]));
        }

        // Create offscreen Transparent framebuffer (HDR color + Offscreen Depth buffer)
        if (m_ViewportRenderer.isOffscreenRenderingEnabled()) {
            VkImageView offscreenTransDepth = m_ViewportRenderer.getOffscreenDepthImageView(i);
            if (offscreenTransDepth != VK_NULL_HANDLE) {
                VkImageView offscreenTransAttachmentsList[] = {
                    m_HDRColorImageViews[i],
                    offscreenTransDepth
                };
                VkFramebufferCreateInfo offscreenTransFramebufferInfo{};
                offscreenTransFramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                offscreenTransFramebufferInfo.renderPass = m_TransparentRenderPass;
                offscreenTransFramebufferInfo.attachmentCount = 2;
                offscreenTransFramebufferInfo.pAttachments = offscreenTransAttachmentsList;
                offscreenTransFramebufferInfo.width = width;
                offscreenTransFramebufferInfo.height = height;
                offscreenTransFramebufferInfo.layers = 1;
                VK_CHECK(vkCreateFramebuffer(resources.device, &offscreenTransFramebufferInfo, nullptr, &m_OffscreenTransparentFramebuffers[i]));
            }
        }
    }
    
    updateGBufferDescriptorSets();

    // Update PostProcess descriptor sets
    for (uint32_t i = 0; i < maxFrames; ++i) {
        if (m_HDRColorImageViews[i] == VK_NULL_HANDLE) continue;

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_HDRColorImageViews[i];
        imageInfo.sampler = m_GBufferSampler;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_PostProcessDescriptorSets[i];
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(resources.device, 1, &write, 0, nullptr);
    }

    m_DepthWidth = width;
    m_DepthHeight = height;

    if (m_ViewportRenderer.isOffscreenRenderingEnabled()) {
        recreateOffscreenPostProcessPipeline();
    }
}

void Renderer::createShadowResources()
{
    if (resources.device == VK_NULL_HANDLE) return;

    uint32_t maxFrames = resources.MAX_FRAMES_IN_FLIGHT;
    m_ShadowImages.resize(maxFrames, VK_NULL_HANDLE);
    m_ShadowAllocations.resize(maxFrames, VK_NULL_HANDLE);
    m_ShadowImageViews.resize(maxFrames, VK_NULL_HANDLE);
    m_ShadowFramebuffers.resize(maxFrames, VK_NULL_HANDLE);

    // Create Sampler (linear filtering, clamp-to-border with border color white)
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    VK_CHECK(vkCreateSampler(resources.device, &samplerInfo, nullptr, &m_ShadowSampler));

    // Create m_ShadowRenderPass (depth-only)
    VkAttachmentDescription depthAttachmentDesc{};
    depthAttachmentDesc.format = VK_FORMAT_D32_SFLOAT;
    depthAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL; // auto-transition to read-only

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 0;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription depthSubpass{};
    depthSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    depthSubpass.colorAttachmentCount = 0;
    depthSubpass.pColorAttachments = nullptr;
    depthSubpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkSubpassDependency dependency2{};
    dependency2.srcSubpass = 0;
    dependency2.dstSubpass = VK_SUBPASS_EXTERNAL;
    dependency2.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency2.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependency2.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    std::array<VkSubpassDependency, 2> dependencies = { dependency, dependency2 };

    VkRenderPassCreateInfo depthRenderPassInfo{};
    depthRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    depthRenderPassInfo.attachmentCount = 1;
    depthRenderPassInfo.pAttachments = &depthAttachmentDesc;
    depthRenderPassInfo.subpassCount = 1;
    depthRenderPassInfo.pSubpasses = &depthSubpass;
    depthRenderPassInfo.dependencyCount = 2;
    depthRenderPassInfo.pDependencies = dependencies.data();

    VK_CHECK(vkCreateRenderPass(resources.device, &depthRenderPassInfo, nullptr, &m_ShadowRenderPass));

    for (uint32_t i = 0; i < maxFrames; ++i) {
        VkImageCreateInfo imgInfo{};
        imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.imageType = VK_IMAGE_TYPE_2D;
        imgInfo.format = VK_FORMAT_D32_SFLOAT;
        imgInfo.extent.width = m_CurrentShadowResolution;
        imgInfo.extent.height = m_CurrentShadowResolution;
        imgInfo.extent.depth = 1;
        imgInfo.mipLevels = 1;
        imgInfo.arrayLayers = 1;
        imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VK_CHECK(vmaCreateImage(resources.allocator, &imgInfo, &allocInfo, &m_ShadowImages[i], &m_ShadowAllocations[i], nullptr));
        ::eng::ResourceTracker::incImage();

        // Image View
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_ShadowImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_D32_SFLOAT;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(resources.device, &viewInfo, nullptr, &m_ShadowImageViews[i]));

        // Framebuffer
        VkImageView shadowAttachment = m_ShadowImageViews[i];
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = m_ShadowRenderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &shadowAttachment;
        fbInfo.width = m_CurrentShadowResolution;
        fbInfo.height = m_CurrentShadowResolution;
        fbInfo.layers = 1;

        VK_CHECK(vkCreateFramebuffer(resources.device, &fbInfo, nullptr, &m_ShadowFramebuffers[i]));
    }
}

void Renderer::destroyShadowResources()
{
    if (resources.device == VK_NULL_HANDLE) return;

    for (auto fb : m_ShadowFramebuffers) {
        if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(resources.device, fb, nullptr);
    }
    m_ShadowFramebuffers.clear();

    for (auto view : m_ShadowImageViews) {
        if (view != VK_NULL_HANDLE) vkDestroyImageView(resources.device, view, nullptr);
    }
    m_ShadowImageViews.clear();

    for (size_t i = 0; i < m_ShadowImages.size(); ++i) {
        if (m_ShadowImages[i] != VK_NULL_HANDLE) {
            vmaDestroyImage(resources.allocator, m_ShadowImages[i], m_ShadowAllocations[i]);
            ::eng::ResourceTracker::decImage();
        }
    }
    m_ShadowImages.clear();
    m_ShadowAllocations.clear();

    if (m_ShadowRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(resources.device, m_ShadowRenderPass, nullptr);
        m_ShadowRenderPass = VK_NULL_HANDLE;
    }
    if (m_ShadowSampler != VK_NULL_HANDLE) {
        vkDestroySampler(resources.device, m_ShadowSampler, nullptr);
        m_ShadowSampler = VK_NULL_HANDLE;
    }
}

void Renderer::initGridPipeline()
{
    if (resources.device == VK_NULL_HANDLE) return;

    // Create Grid Pipeline Layout
    std::vector<VkDescriptorSetLayout> setLayouts = { resources.globalSetLayout };
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    layoutInfo.pSetLayouts = setLayouts.data();
    layoutInfo.pushConstantRangeCount = 0;
    layoutInfo.pPushConstantRanges = nullptr;
    VK_CHECK(vkCreatePipelineLayout(resources.device, &layoutInfo, nullptr, &m_GridPipelineLayout));

    // Load Grid Shaders
    VkShaderModule vertModule = resources.loadShaderModule("shaders/grid_vert.spv");
    VkShaderModule fragModule = resources.loadShaderModule("shaders/grid_frag.spv");

    if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE) {
        LOG_ERROR("Failed to load infinite grid shaders.");
        if (vertModule != VK_NULL_HANDLE) vkDestroyShaderModule(resources.device, vertModule, nullptr);
        if (fragModule != VK_NULL_HANDLE) vkDestroyShaderModule(resources.device, fragModule, nullptr);
        return;
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertModule;
    stages[0].pName = "main";

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragModule;
    stages[1].pName = "main";

    // Procedural vertices, no attributes
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr;

    VkPipelineInputAssemblyStateCreateInfo inputAsm{};
    inputAsm.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAsm.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAsm.primitiveRestartEnable = VK_FALSE;

    // Viewport State (dynamic)
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr;
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.cullMode = VK_CULL_MODE_NONE; // grid drawn as full screen quad, no culling needed
    raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
    raster.lineWidth = 1.0f;
    raster.polygonMode = VK_POLYGON_MODE_FILL;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth Stencil: Enable depth testing, disable depth writing
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    // Color Blending: Enable alpha blending
    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttachment.blendEnable = VK_TRUE;
    blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &blendAttachment;

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
    pipelineInfo.layout = m_GridPipelineLayout;
    pipelineInfo.renderPass = m_TransparentRenderPass;

    VK_CHECK(vkCreateGraphicsPipelines(resources.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_GridPipeline));

    vkDestroyShaderModule(resources.device, vertModule, nullptr);
    vkDestroyShaderModule(resources.device, fragModule, nullptr);
}

void Renderer::destroyGridPipeline()
{
    if (resources.device == VK_NULL_HANDLE) return;

    if (m_GridPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(resources.device, m_GridPipeline, nullptr);
        m_GridPipeline = VK_NULL_HANDLE;
    }
    if (m_GridPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(resources.device, m_GridPipelineLayout, nullptr);
        m_GridPipelineLayout = VK_NULL_HANDLE;
    }
}

uint32_t Renderer::PickEntity(uint32_t x, uint32_t y)
{
    if (x >= GetOffscreenWidth() || y >= GetOffscreenHeight()) {
        return 0;
    }

    if (frameIndex >= m_GBufferCImages.size() || m_GBufferCImages[frameIndex] == VK_NULL_HANDLE) {
        return 0;
    }

    VkImage gbufferImage = m_GBufferCImages[frameIndex];

    // Create staging buffer (4 bytes for R8G8B8A8 pixel)
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation stagingAllocation = VK_NULL_HANDLE;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = 4;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo allocationInfo{};
    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufferInfo, &allocInfo, &stagingBuffer, &stagingAllocation, &allocationInfo));

    // Transition GBufferC layout to TRANSFER_SRC_OPTIMAL, copy, then transition back to SHADER_READ_ONLY_OPTIMAL
    VkCommandBuffer cmd = resources.beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = gbufferImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &barrier);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { static_cast<int32_t>(x), static_cast<int32_t>(y), 0 };
    region.imageExtent = { 1, 1, 1 };

    vkCmdCopyImageToBuffer(cmd, gbufferImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1, &region);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0,
                         0, nullptr,
                         0, nullptr,
                         1, &barrier);

    resources.endSingleTimeCommands(cmd);

    // Read data
    uint8_t* mappedData = nullptr;
    vmaMapMemory(resources.allocator, stagingAllocation, reinterpret_cast<void**>(&mappedData));

    // Entity ID is in GBufferC's blue channel (channel 2)
    uint32_t entityID = mappedData[2];

    vmaUnmapMemory(resources.allocator, stagingAllocation);
    vmaDestroyBuffer(resources.allocator, stagingBuffer, stagingAllocation);

    return entityID;
}

} // namespace eng::renderer
