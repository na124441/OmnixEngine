#include "Core/pch.h"
#include "Rendering/Core/Renderer.h"
#include "Core/Diagnostics/Assert.h"
#include "stb/stb_image_write.h"
#include "Rendering/Geometry/Assets/RVGRegistry.h"
#include "Rendering/Geometry/Streaming/RVGPageStreamingManager.h"
#include "Runtime/OmnixMaterialFormat.h"
#include "Runtime/AssetRegistry.h"
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
#include "Rendering/GPUScene/GPUScene.h"
#include "Rendering/Geometry/MeshRenderer.h"
#include "Rendering/Geometry/VisibilitySystem.h"
#include "Rendering/Lighting/EnvironmentSystem.h"
#include "Rendering/Lighting/ReflectionProbeRegistry.h"
#include <random>
#include <fstream>
#include <filesystem>
#include <unordered_set>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace eng::renderer {

    static std::string ResolveTexturePath(const std::string& path) {
        if (path.empty()) return "";
        if (std::filesystem::exists(path)) {
            return path;
        }
        std::filesystem::path p(path);
        std::string filename = p.filename().string();
        std::string resolved = "Assets/Textures/" + filename;
        if (std::filesystem::exists(resolved)) {
            return resolved;
        }
        if (filename == "brick_albedo.png" || filename == "wood_albedo.png" || filename == "brick_normal.png") {
            return "";
        }
        return path;
    }

    static Renderer::FrameDiagnostics* s_CurrentDiagnostics = nullptr;

    Renderer::FrameDiagnostics* Renderer::GetCurrentDiagnostics() {
        return s_CurrentDiagnostics;
    }

    bool IsFinite(const glm::mat4& m) {
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                float val = m[col][row];
                if (std::isnan(val) || std::isinf(val)) {
                    return false;
                }
            }
        }
        for (int i = 0; i < 3; ++i) {
            float scaleVal = glm::length(glm::vec3(m[i]));
            if (scaleVal > 1e7f) {
                return false;
            }
        }
        float tx = m[3][0];
        float ty = m[3][1];
        float tz = m[3][2];
        if (std::abs(tx) > 1e7f || std::abs(ty) > 1e7f || std::abs(tz) > 1e7f) {
            return false;
        }
        return true;
    }

    bool ValidateRenderItem(const RenderItem& item, uint32_t instanceIndex, const GPUScene& gpuScene) {
        if (!item.mesh) {
            return false;
        }
        if (!item.mesh->isVirtualGeometry) {
            if (item.mesh->vertexBuffer == VK_NULL_HANDLE || item.mesh->indexBuffer == VK_NULL_HANDLE) {
                return false;
            }
        }
        if (item.mesh->indexCount == 0) {
            return false;
        }
        if (!item.material) {
            return false;
        }
        if (instanceIndex >= gpuScene.GetGPUInstances().size()) {
            return false;
        }
        if (!IsFinite(item.transform) || !IsFinite(item.model)) {
            return false;
        }
        return true;
    }

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
    uint32_t enableFog;
    float fogDensity;
    float fogHeightFalloff;

    // Color Grading
    float contrast;
    float saturation;
    float temp;
    float tint;

    glm::vec3 lift;
    float pad0;

    glm::vec3 gammaVal;
    float pad1;

    glm::vec3 gain;
    float pad2;

    glm::vec3 fogColor;
    float fogBaseHeight;
};

Renderer::~Renderer()
{
    Shutdown();
}

void Renderer::Initialize()
{
    resources.debugConfig = &m_DebugConfig;
    m_RenderTargetManager.Initialize(resources.device, resources.allocator);
    m_RenderTargetManager.SetFramebufferManager(&m_FramebufferManager);
    m_FramebufferManager.Initialize(resources.device, &m_RenderTargetManager);

    // RenderTargetManager Unit Test
    {
        LOG_INFO("UNIT TEST: Starting RenderTargetManager creation/destruction verification...");
        RenderTargetDesc testColor{};
        testColor.width = 128;
        testColor.height = 128;
        testColor.format = VK_FORMAT_R8G8B8A8_UNORM;
        testColor.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        testColor.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        testColor.colorAttachment = true;
        testColor.debugName = "UnitTest_ColorTarget";

        RenderTargetHandle colorHandle = m_RenderTargetManager.Create(testColor);
        if (m_RenderTargetManager.IsValid(colorHandle)) {
            LOG_INFO("UNIT TEST: Color target created successfully.");
        } else {
            LOG_ERROR("UNIT TEST: Color target creation failed.");
        }

        RenderTargetDesc testDepth{};
        testDepth.width = 128;
        testDepth.height = 128;
        testDepth.format = VK_FORMAT_D32_SFLOAT;
        testDepth.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        testDepth.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        testDepth.depthAttachment = true;
        testDepth.debugName = "UnitTest_DepthTarget";

        RenderTargetHandle depthHandle = m_RenderTargetManager.Create(testDepth);
        if (m_RenderTargetManager.IsValid(depthHandle)) {
            LOG_INFO("UNIT TEST: Depth target created successfully.");
        } else {
            LOG_ERROR("UNIT TEST: Depth target creation failed.");
        }

        // Dump targets to check
        m_RenderTargetManager.DumpAllTargets();

        // Destroy targets
        m_RenderTargetManager.Destroy(colorHandle);
        m_RenderTargetManager.Destroy(depthHandle);
        LOG_INFO("UNIT TEST: Color/Depth target destruction completed.");
    }

    m_CurrentShadowResolution = 2048;
    createShadowResources();

    m_ViewportRenderer.init(&resources);

    m_HZBPass.Initialize(resources);

    RVGRegistry::Get().Initialize(resources);
    RVGPageStreamingManager::Get().Initialize(resources);
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
    std::array<VkAttachmentDescription, 7> gbufferAttachments{};
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

    // 4: GBufferObjectID (Unsigned integer Entity ID)
    gbufferAttachments[4].format = VK_FORMAT_R32_UINT;
    gbufferAttachments[4].samples = VK_SAMPLE_COUNT_1_BIT;
    gbufferAttachments[4].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    gbufferAttachments[4].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    gbufferAttachments[4].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    gbufferAttachments[4].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    gbufferAttachments[4].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    gbufferAttachments[4].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // 5: GBufferVelocity
    gbufferAttachments[5].format = VK_FORMAT_R16G16_SFLOAT;
    gbufferAttachments[5].samples = VK_SAMPLE_COUNT_1_BIT;
    gbufferAttachments[5].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    gbufferAttachments[5].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    gbufferAttachments[5].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    gbufferAttachments[5].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    gbufferAttachments[5].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    gbufferAttachments[5].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // 6: Depth
    gbufferAttachments[6].format = VK_FORMAT_D32_SFLOAT;
    gbufferAttachments[6].samples = VK_SAMPLE_COUNT_1_BIT;
    gbufferAttachments[6].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // loaded from depth prepass
    gbufferAttachments[6].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    gbufferAttachments[6].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    gbufferAttachments[6].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    gbufferAttachments[6].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    gbufferAttachments[6].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    std::array<VkAttachmentReference, 6> colorRefs{};
    for (uint32_t i = 0; i < 6; ++i) {
        colorRefs[i].attachment = i;
        colorRefs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    VkAttachmentReference depthRef{};
    depthRef.attachment = 6;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription gbufferSubpass{};
    gbufferSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    gbufferSubpass.colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
    gbufferSubpass.pColorAttachments = colorRefs.data();
    gbufferSubpass.pDepthStencilAttachment = &depthRef;

    std::array<VkSubpassDependency, 2> gbufferDependencies{};
    // External to 0
    gbufferDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    gbufferDependencies[0].dstSubpass = 0;
    gbufferDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    gbufferDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    gbufferDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    gbufferDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // 0 to External
    gbufferDependencies[1].srcSubpass = 0;
    gbufferDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    gbufferDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    gbufferDependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    gbufferDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    gbufferDependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo gbufferRenderPassInfo{};
    gbufferRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    gbufferRenderPassInfo.attachmentCount = static_cast<uint32_t>(gbufferAttachments.size());
    gbufferRenderPassInfo.pAttachments = gbufferAttachments.data();
    gbufferRenderPassInfo.subpassCount = 1;
    gbufferRenderPassInfo.pSubpasses = &gbufferSubpass;
    gbufferRenderPassInfo.dependencyCount = 2;
    gbufferRenderPassInfo.pDependencies = gbufferDependencies.data();

    VK_CHECK(vkCreateRenderPass(resources.device, &gbufferRenderPassInfo, nullptr, &m_GBufferRenderPass));

    // Swap to new render pass for all subsequent materials/pipelines compatibility
    resources.renderPass = m_GBufferRenderPass;
    resources.gbufferRenderPass = m_GBufferRenderPass;

    // Create m_ViewportColorRenderPass (color only, loadOp = LOAD)
    VkAttachmentDescription vpAttachmentDesc{};
    vpAttachmentDesc.format = resources.swapChainImageFormat;
    vpAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    vpAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // Load copied scene color
    vpAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    vpAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    vpAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    vpAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    vpAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference vpColorRef{};
    vpColorRef.attachment = 0;
    vpColorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription vpSubpass{};
    vpSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    vpSubpass.colorAttachmentCount = 1;
    vpSubpass.pColorAttachments = &vpColorRef;

    VkRenderPassCreateInfo vpRenderPassInfo{};
    vpRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    vpRenderPassInfo.attachmentCount = 1;
    vpRenderPassInfo.pAttachments = &vpAttachmentDesc;
    vpRenderPassInfo.subpassCount = 1;
    vpRenderPassInfo.pSubpasses = &vpSubpass;

    VK_CHECK(vkCreateRenderPass(resources.device, &vpRenderPassInfo, nullptr, &m_ViewportColorRenderPass));

    // Create GBuffer descriptor set layout
    VkDescriptorSetLayoutBinding gbufferBindings[17]{};
    for (uint32_t i = 0; i < 16; ++i) {
        gbufferBindings[i].binding = i;
        gbufferBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        gbufferBindings[i].descriptorCount = 1;
        gbufferBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        gbufferBindings[i].pImmutableSamplers = nullptr;
    }
    gbufferBindings[16].binding = 16;
    gbufferBindings[16].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    gbufferBindings[16].descriptorCount = 4; // 4 local reflection probes
    gbufferBindings[16].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    gbufferBindings[16].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo gbufferSetLayoutInfo{};
    gbufferSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    gbufferSetLayoutInfo.bindingCount = 17;
    gbufferSetLayoutInfo.pBindings = gbufferBindings;
    VK_CHECK(vkCreateDescriptorSetLayout(resources.device, &gbufferSetLayoutInfo, nullptr, &m_GBufferDescriptorSetLayout));

    // Create GBuffer descriptor pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = resources.MAX_FRAMES_IN_FLIGHT * 20;

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

    // Initialize editor overlay passes
    m_EditorOverlayPass.Initialize(resources);
    m_SelectionOutlinePass.Initialize(
        resources, 
        m_ViewportRenderer.isOffscreenRenderingEnabled() ? m_ViewportRenderer.getOffscreenRenderPass() : m_SwapchainRenderPass, 
        m_GBufferDescriptorSetLayout
    );

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

    std::array<VkSubpassDependency, 2> hdrDependencies{};
    // External to 0
    hdrDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    hdrDependencies[0].dstSubpass = 0;
    hdrDependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    hdrDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    hdrDependencies[0].srcAccessMask = 0;
    hdrDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // 0 to External
    hdrDependencies[1].srcSubpass = 0;
    hdrDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    hdrDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    hdrDependencies[1].dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    hdrDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    hdrDependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo hdrRpInfo{};
    hdrRpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    hdrRpInfo.attachmentCount = 1;
    hdrRpInfo.pAttachments = &hdrAttachment;
    hdrRpInfo.subpassCount = 1;
    hdrRpInfo.pSubpasses = &hdrSubpass;
    hdrRpInfo.dependencyCount = 2;
    hdrRpInfo.pDependencies = hdrDependencies.data();

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

        std::array<VkSubpassDependency, 2> transDependencies{};
        // External to 0
        transDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        transDependencies[0].dstSubpass = 0;
        transDependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        transDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        transDependencies[0].srcAccessMask = 0;
        transDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        // 0 to External
        transDependencies[1].srcSubpass = 0;
        transDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        transDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        transDependencies[1].dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        transDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        transDependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        VkRenderPassCreateInfo transRpInfo{};
        transRpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        transRpInfo.attachmentCount = 2;
        transRpInfo.pAttachments = transAttachments.data();
        transRpInfo.subpassCount = 1;
        transRpInfo.pSubpasses = &transSubpass;
        transRpInfo.dependencyCount = 2;
        transRpInfo.pDependencies = transDependencies.data();

        VK_CHECK(vkCreateRenderPass(resources.device, &transRpInfo, nullptr, &m_TransparentRenderPass));
        resources.transparentRenderPass = m_TransparentRenderPass;
    }

    // -------------------------------------------------------------------------
    // Create m_PostProcessDescriptorSetLayout (binding 0: sampled HDR texture, binding 1: exposure, binding 2: depth)
    // -------------------------------------------------------------------------
    VkDescriptorSetLayoutBinding postProcessBindings[3]{};
    postProcessBindings[0].binding = 0;
    postProcessBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    postProcessBindings[0].descriptorCount = 1;
    postProcessBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    postProcessBindings[0].pImmutableSamplers = nullptr;

    postProcessBindings[1].binding = 1;
    postProcessBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    postProcessBindings[1].descriptorCount = 1;
    postProcessBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    postProcessBindings[1].pImmutableSamplers = nullptr;

    postProcessBindings[2].binding = 2;
    postProcessBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    postProcessBindings[2].descriptorCount = 1;
    postProcessBindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    postProcessBindings[2].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo postProcessLayoutInfo{};
    postProcessLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    postProcessLayoutInfo.bindingCount = 3;
    postProcessLayoutInfo.pBindings = postProcessBindings;
    VK_CHECK(vkCreateDescriptorSetLayout(resources.device, &postProcessLayoutInfo, nullptr, &m_PostProcessDescriptorSetLayout));

    // Create m_PostProcessDescriptorPool
    std::array<VkDescriptorPoolSize, 2> postProcessPoolSizes{};
    postProcessPoolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    postProcessPoolSizes[0].descriptorCount = resources.MAX_FRAMES_IN_FLIGHT * 2;
    postProcessPoolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    postProcessPoolSizes[1].descriptorCount = resources.MAX_FRAMES_IN_FLIGHT;

    VkDescriptorPoolCreateInfo postProcessPoolInfo{};
    postProcessPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    postProcessPoolInfo.poolSizeCount = 2;
    postProcessPoolInfo.pPoolSizes = postProcessPoolSizes.data();
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
    VkDescriptorSetLayout setLayouts[2] = {
        m_PostProcessDescriptorSetLayout,
        gpuScene.GetDescriptorSetLayout()
    };
    VkPipelineLayoutCreateInfo postProcessPipelineLayoutInfo{};
    postProcessPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    postProcessPipelineLayoutInfo.setLayoutCount = 2;
    postProcessPipelineLayoutInfo.pSetLayouts = setLayouts;
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
        resources.globalSetLayout,                         // Set 0: camera/lights
        m_GBufferDescriptorSetLayout,                       // Set 1: GBuffer textures
        resources.globalSetLayout,                         // Set 2: Placeholder
        gpuScene.GetLocalLightsDescriptorSetLayout()       // Set 3: Local lights
    };

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(uint32_t); // localLightCount

    VkPipelineLayoutCreateInfo deferredLayoutInfo{};
    deferredLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    deferredLayoutInfo.setLayoutCount = static_cast<uint32_t>(deferredSetLayouts.size());
    deferredLayoutInfo.pSetLayouts = deferredSetLayouts.data();
    deferredLayoutInfo.pushConstantRangeCount = 1;
    deferredLayoutInfo.pPushConstantRanges = &pushConstantRange;
    VK_CHECK(vkCreatePipelineLayout(resources.device, &deferredLayoutInfo, nullptr, &m_DeferredPipelineLayout));

    resources.createPipelineLayout();

    // 1. Geometry pipeline is already created by legacy engine
    geometryPipeline = VK_NULL_HANDLE;

    // 2. Create pipelines for other passes
    createSSAOResources();
    initPipelines();
    initSSRPipeline();
    initExposurePipeline();
    initTAAPipeline();

    // 3. Build the render-graph
    setupRenderGraph();

    // 4. Create default fallback assets
    m_DefaultMesh = scene.createMesh();
    const std::vector<PbrVertex> vertices = {
        {{ 0.0f, -0.5f,  0.0f}, {0.0f, -1.0f, 0.0f}, {0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f,  1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.0f,  1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f,  1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f,  1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}}
    };
    const std::vector<uint32_t> indices = {
        0,1,2, 0,3,1, 0,2,4, 0,4,3,
        1,3,4, 1,4,2
    };
    m_DefaultMesh->init(vertices.data(), vertices.size(), indices.data(), indices.size(), resources);

    m_DefaultMaterial = scene.createMaterial();
    bool ok = m_DefaultMaterial->create("shaders/gbuffer_vert.spv", 
                                        "shaders/gbuffer_frag.spv", 
                                        "Assets/Textures/Texturelabs_Concrete_129S.png", 
                                        "", 
                                        resources);
    if (!ok) {
        m_DefaultMaterial->setFallbackPipeline(m_GBufferPipeline);
    }

    initGridPipeline();
    ValidateStartupState();
}
void Renderer::Shutdown()
{
    destroyGridPipeline();

    m_SelectionOutlinePass.Shutdown(resources);
    m_EditorOverlayPass.Shutdown(resources);
    m_HZBPass.Shutdown(resources);
    destroySSRPipeline();
    destroyExposurePipeline();
    destroyTAAPipeline();

    if (resources.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(resources.device);
    }
    renderGraph.Shutdown(resources);

    gpuScene.Shutdown(resources);
    RVGPageStreamingManager::Get().Shutdown(resources);
    RVGRegistry::Get().Shutdown();

    if (m_GBufferPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(resources.device, m_GBufferPipeline, nullptr);
        m_GBufferPipeline = VK_NULL_HANDLE;
    }
    if (m_GBufferPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(resources.device, m_GBufferPipelineLayout, nullptr);
        m_GBufferPipelineLayout = VK_NULL_HANDLE;
    }

    if (m_ShadowPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(resources.device, m_ShadowPipeline, nullptr);
        m_ShadowPipeline = VK_NULL_HANDLE;
    }
    if (m_ShadowPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(resources.device, m_ShadowPipelineLayout, nullptr);
        m_ShadowPipelineLayout = VK_NULL_HANDLE;
    }
    destroyShadowResources();
    destroySSAOResources();

    m_ViewportRenderer.cleanup();

    if (m_GBufferRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(resources.device, m_GBufferRenderPass, nullptr);
        m_GBufferRenderPass = VK_NULL_HANDLE;
    }
    if (m_ViewportColorRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(resources.device, m_ViewportColorRenderPass, nullptr);
        m_ViewportColorRenderPass = VK_NULL_HANDLE;
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

    if (m_LightCullingPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(resources.device, m_LightCullingPipeline, nullptr);
        m_LightCullingPipeline = VK_NULL_HANDLE;
    }
    if (m_LightCullingPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(resources.device, m_LightCullingPipelineLayout, nullptr);
        m_LightCullingPipelineLayout = VK_NULL_HANDLE;
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
    
    m_FramebufferManager.Shutdown();
    m_RenderTargetManager.Shutdown();
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
    m_FrameDiagnostics = {};
    // Removed duplicate layout tracking initialization m_DepthImageLayout
    m_FrameDiagnostics.frameNumber = m_FrameCount;
    s_CurrentDiagnostics = &m_FrameDiagnostics;

    static eng::renderer::Renderer::VisibilityMode s_PrevMode = (eng::renderer::Renderer::VisibilityMode)-1;
    if (m_VisibilityMode != s_PrevMode) {
        std::string modeName = "Unknown";
        switch (m_VisibilityMode) {
            case VisibilityMode::CPUDriven: modeName = "CPU Driven"; break;
            case VisibilityMode::GPUFrustumOnly: modeName = "GPU Frustum Only"; break;
            case VisibilityMode::GPUFrustumIndirect: modeName = "GPU Frustum + Indirect (Experimental)"; break;
            case VisibilityMode::GPUFrustumOcclusion: modeName = "GPU Frustum + Occlusion (Experimental)"; break;
        }
        LOG_INFO("Visibility Mode changed to: " + modeName);
        if (m_VisibilityMode == VisibilityMode::GPUFrustumIndirect || m_VisibilityMode == VisibilityMode::GPUFrustumOcclusion) {
            LOG_WARN("Experimental visibility mode '" + modeName + "' active. This mode is experimental and may be unstable.");
        }
        s_PrevMode = m_VisibilityMode;
    }

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

    if (activeRenderScene.skyLight.mode == 1 && activeRenderScene.skyLight.environmentPath != m_LastEnvironmentPath) {
        vkDeviceWaitIdle(resources.device);
        if (!activeRenderScene.skyLight.environmentPath.empty()) {
            EnvironmentSystem::Get().GetOrCreateEnvironment(activeRenderScene.skyLight.environmentPath, resources);
        }
        updateGBufferDescriptorSets();
        m_LastEnvironmentPath = activeRenderScene.skyLight.environmentPath;
    } else if (activeRenderScene.skyLight.mode == 0 && !m_LastEnvironmentPath.empty()) {
        vkDeviceWaitIdle(resources.device);
        updateGBufferDescriptorSets();
        m_LastEnvironmentPath = "";
    }

    bool needsBake = false;
    for (const auto& probe : activeRenderScene.reflectionProbes) {
        if (!probe.capturePath.empty() && ReflectionProbeRegistry::Get().GetTexture(probe.capturePath) == nullptr) {
            needsBake = true;
            break;
        }
    }
    if (needsBake) {
        BakeReflectionProbes(world);
    }

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
    activeFrameContext.projectionMatrix[1][1] *= -1.0f;
    activeFrameContext.cameraPosition = activeRenderScene.camera.position;
    activeFrameContext.cameraNear = activeRenderScene.camera.nearPlane;
    activeFrameContext.cameraFar = activeRenderScene.camera.farPlane;
    activeFrameContext.cameraFov = activeRenderScene.camera.fov;

    // Sync legacy camera object for viewport panel queries
    this->camera.position = activeRenderScene.camera.position;
    this->camera.fovY = glm::radians(activeRenderScene.camera.fov);
    this->camera.nearPlane = activeRenderScene.camera.nearPlane;
    this->camera.farPlane = activeRenderScene.camera.farPlane;
    
    buildRenderQueue();

    m_FrameDiagnostics.entitiesExtracted = static_cast<uint32_t>(activeRenderScene.meshInstances.size());
    m_FrameDiagnostics.instancesUploaded = static_cast<uint32_t>(gpuScene.GetGPUInstances().size());
    m_FrameDiagnostics.opaqueItems = static_cast<uint32_t>(renderQueue.getItems().size());
    m_FrameDiagnostics.transparentItems = static_cast<uint32_t>(transparentRenderQueue.getItems().size());

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
            
            float aspect = activeFrameContext.viewportSize.y > 0 ? (float)activeFrameContext.viewportSize.x / (float)activeFrameContext.viewportSize.y : 16.0f / 9.0f;
            float fovRad = glm::radians(activeRenderScene.camera.fov);

            // Cascade split depths calculation
            float nearPlane = activeRenderScene.camera.nearPlane;
            float farPlane = activeRenderScene.camera.farPlane;
            float shadowDistance = dirLight.shadowDistance;
            if (shadowDistance <= nearPlane) shadowDistance = 100.0f;
            
            float clipRange = shadowDistance - nearPlane;
            float ratio = shadowDistance / nearPlane;
            float lambda = 0.95f; // Practical split blend factor

            float splits[4];
            for (uint32_t c = 0; c < 4; ++c) {
                float p = static_cast<float>(c + 1) / 4.0f;
                float logSplit = nearPlane * std::pow(ratio, p);
                float uniSplit = nearPlane + clipRange * p;
                splits[c] = lambda * logSplit + (1.0f - lambda) * uniSplit;
            }

            m_CascadeSplits = glm::vec4(splits[0], splits[1], splits[2], splits[3]);

            for (uint32_t c = 0; c < 4; ++c) {
                float cNear = (c == 0) ? nearPlane : splits[c - 1];
                float cFar = splits[c];

                float nearH = 2.0f * std::tan(fovRad / 2.0f) * cNear;
                float nearW = nearH * aspect;
                float farH = 2.0f * std::tan(fovRad / 2.0f) * cFar;
                float farW = farH * aspect;

                glm::mat4 invView = glm::inverse(activeRenderScene.camera.viewMatrix);
                glm::vec3 camRight = glm::normalize(glm::vec3(invView[0]));
                glm::vec3 camUp = glm::normalize(glm::vec3(invView[1]));
                glm::vec3 camFront = -glm::normalize(glm::vec3(invView[2]));

                glm::vec3 cameraPos = activeRenderScene.camera.position;
                glm::vec3 nearCenter = cameraPos + camFront * cNear;
                glm::vec3 farCenter = cameraPos + camFront * cFar;

                glm::vec3 corners[8] = {
                    nearCenter + (camUp * (nearH * 0.5f)) - (camRight * (nearW * 0.5f)),
                    nearCenter + (camUp * (nearH * 0.5f)) + (camRight * (nearW * 0.5f)),
                    nearCenter - (camUp * (nearH * 0.5f)) - (camRight * (nearW * 0.5f)),
                    nearCenter - (camUp * (nearH * 0.5f)) + (camRight * (nearW * 0.5f)),
                    farCenter + (camUp * (farH * 0.5f)) - (camRight * (farW * 0.5f)),
                    farCenter + (camUp * (farH * 0.5f)) + (camRight * (farW * 0.5f)),
                    farCenter - (camUp * (farH * 0.5f)) - (camRight * (farW * 0.5f)),
                    farCenter - (camUp * (farH * 0.5f)) + (camRight * (farW * 0.5f))
                };

                glm::vec3 frustumCenter(0.0f);
                for (int i = 0; i < 8; ++i) {
                    frustumCenter += corners[i];
                }
                frustumCenter /= 8.0f;

                float radius = 0.0f;
                for (int i = 0; i < 8; ++i) {
                    radius = std::max(radius, glm::distance(frustumCenter, corners[i]));
                }
                radius = std::ceil(radius * 16.0f) / 16.0f;

                glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
                if (std::abs(glm::dot(lightDir, up)) > 0.99f) {
                    up = glm::vec3(0.0f, 0.0f, 1.0f);
                }

                // Extends bounds to capture offscreen shadow casters
                glm::vec3 lightPos = frustumCenter - lightDir * (radius + 100.0f);
                glm::mat4 lightView = glm::lookAt(lightPos, frustumCenter, up);

                float minX = -radius;
                float maxX = radius;
                float minY = -radius;
                float maxY = radius;
                float minZ = -100.0f - radius;
                float maxZ = radius + 100.0f;

                glm::mat4 lightProj = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);
                lightProj[1][1] *= -1.0f; // Vulkan Y flip

                glm::mat4 lightVP = lightProj * lightView;
                glm::vec4 shadowOrigin = lightVP * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
                shadowOrigin = shadowOrigin * (static_cast<float>(dirLight.shadowResolution) / 2.0f);

                glm::vec4 roundedOrigin = glm::round(shadowOrigin);
                glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
                roundOffset = roundOffset * (2.0f / static_cast<float>(dirLight.shadowResolution));
                roundOffset.z = 0.0f;
                roundOffset.w = 0.0f;

                lightProj[3] += roundOffset;
                lightVP = lightProj * lightView;

                m_LastLightSpaceMatrices[c] = lightVP;
            }

            m_LastLightSpaceMatrix = m_LastLightSpaceMatrices[0];
            dirLight.lightSpaceMatrix = m_LastLightSpaceMatrices[0];
            for (int c = 0; c < 4; ++c) {
                dirLight.directionalLightProjViews[c] = m_LastLightSpaceMatrices[c];
            }
            dirLight.cascadeSplitDepths = m_CascadeSplits;
        }
    }

    std::vector<RenderItem> combinedItems = renderQueue.getItems();
    combinedItems.insert(combinedItems.end(), transparentRenderQueue.getItems().begin(), transparentRenderQueue.getItems().end());

    float timeSeconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - m_StartTime).count();
    Omnix::Radiance::RadianceFrameUBO radianceUBO{};
    UpdateRadianceFrameUBO(
        radianceUBO,
        activeRenderScene.camera,
        targetWidth,
        targetHeight,
        timeSeconds
    );

    m_CurrentFrameCount++;
    RVGPageStreamingManager::Get().ProcessGPURequests(resources, frameIndex, m_CurrentFrameCount);
    RVGPageStreamingManager::Get().Update(resources, frameIndex, m_CurrentFrameCount);

    gpuScene.UpdateFrame(
        resources,
        frameIndex,
        activeRenderScene,
        combinedItems,
        m_EcsMaterialCache,
        m_DefaultMaterial,
        radianceUBO,
        m_ShadingMode,
        m_ActiveScene
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
    updateSSRDescriptorSet(frameIndex);
    setupRenderGraph();
    renderGraph.Compile(resources);

    static bool s_DebugPrinted = false;
    if (!s_DebugPrinted) {
        renderGraph.PrintDebug();
        s_DebugPrinted = true;
    }

    // Log frame resources ownership (Phase 3 part 1)
    static bool s_LogOwnershipOnce = false;
    if (!s_LogOwnershipOnce) {
        std::string ownershipLog = "[Frame Resource Acquisition Log]\n"
                                   "Current Frame:   " + std::to_string(frameIndex) + "\n" +
                                   "DepthBuffer Idx: " + std::to_string(m_DepthHandles[frameIndex].index) + "\n" +
                                   "VkImage:         " + std::to_string((uintptr_t)m_DepthImages[frameIndex]) + "\n" +
                                   "Framebuffer:     " + std::to_string((uintptr_t)(frameIndex < m_GBufferFramebuffers.size() ? m_GBufferFramebuffers[frameIndex] : VK_NULL_HANDLE)) + "\n" +
                                   "Swapchain Image: " + std::to_string(currentSwapchainImageIndex);
        LOG_INFO(ownershipLog);
        s_LogOwnershipOnce = true;
    }

    bool graphExecutionFailed = false;
    bool ok = renderGraph.ExecuteWithValidation(resources, frameIndex, m_RenderTargetManager, graphExecutionFailed);
    if (!ok || graphExecutionFailed) {
        if (m_DebugConfig.disableFallback) {
            OMNIX_FATAL_ASSERT(false, "RenderGraph execution failed under Phase 0 debug baseline! Fatal validation errors detected.");
        }
        LOG_WARN("RenderGraph execution failed! Running fallback clear pass to safely present...");
        
        // Safety Fallback: Acquire command buffer for PassID::UI to perform clear/Imgui and transition swapchain to PRESENT_SRC
        VkCommandBuffer cmd = resources.commandBuffers[frameIndex][static_cast<size_t>(PassID::UI)];
        
        // Reset command buffers for other passes to prevent stale recording or double submission
        for (uint32_t i = 0; i < PASS_COUNT; ++i) {
            if (i != static_cast<uint32_t>(PassID::UI)) {
                VkCommandBuffer passCmd = resources.commandBuffers[frameIndex][i];
                VkCommandBufferBeginInfo begin{};
                begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                vkBeginCommandBuffer(passCmd, &begin);
                vkEndCommandBuffer(passCmd);
            }
        }

        // Record fallback clear and layout transitions on the UI command buffer
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cmd, &begin));

        // Transition swapchain image to COLOR_ATTACHMENT_OPTIMAL
        if (currentSwapchainImageIndex < resources.swapChainImages.size()) {
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
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

            // Execute a simple clear render pass on swapchain to a warning red/dark gray color
            VkRenderPassBeginInfo rpInfo{};
            rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpInfo.renderPass = m_SwapchainRenderPass;
            rpInfo.framebuffer = currentSwapchainImageIndex < resources.swapChainFramebuffers.size() ? resources.swapChainFramebuffers[currentSwapchainImageIndex] : VK_NULL_HANDLE;
            rpInfo.renderArea.offset = {0, 0};
            rpInfo.renderArea.extent = resources.swapChainExtent;

            VkClearValue clearValue{};
            clearValue.color = {{0.15f, 0.02f, 0.02f, 1.0f}}; // Dark red warning background
            rpInfo.clearValueCount = 1;
            rpInfo.pClearValues = &clearValue;

            vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

            // We can draw an ImGui error panel if uiCallback is valid
            if (resources.uiCallback) {
                // Inside UI callback or custom ImGui we can show error message:
                // ImGui::Begin("Graph Execution Error"); ImGui::Text("Render pass validation failed!"); ImGui::End();
                // However, we just execute the normal UI layout code since ImGui handles itself.
            }

            vkCmdEndRenderPass(cmd);

            // Transition to PRESENT_SRC_KHR
            VkImageMemoryBarrier presentBarrier = barrier;
            presentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            presentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            presentBarrier.dstAccessMask = 0;

            vkCmdPipelineBarrier(cmd,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                 0,
                                 0, nullptr,
                                 0, nullptr,
                                 1, &presentBarrier);
        }

        VK_CHECK(vkEndCommandBuffer(cmd));
    }
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

    // Detailed vkQueueSubmit logging (Phase 6 part 1)
    static bool s_LogSubmitOnce = false;
    if (!s_LogSubmitOnce) {
        std::string submitLog = "[vkQueueSubmit Log]\n"
                                 "Frame:          " + std::to_string(frameIndex) + "\n" +
                                 "Fence:          " + std::to_string((uintptr_t)resources.inFlightFences.at(frameIndex)) + "\n" +
                                 "Wait Semaphore: " + std::to_string((uintptr_t)waitSem) + "\n" +
                                 "Signal Sem:     " + std::to_string((uintptr_t)signalSem) + "\n" +
                                 "Cmd Buffers:    " + std::to_string(submitCmds.size());
        LOG_INFO(submitLog);
        s_LogSubmitOnce = true;
    }

    VkResult submitResult = vkQueueSubmit(resources.graphicsQueue,
                                          1,
                                          &submitInfo,
                                          resources.inFlightFences.at(frameIndex));
    if (submitResult == VK_ERROR_DEVICE_LOST) {
        renderGraph.LogDeviceLostReport(frameIndex);
    }
    VK_CHECK(submitResult);

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

    // Log frame diagnostic report
    LOG_INFO("Frame " + std::to_string(m_FrameDiagnostics.frameNumber));
    LOG_INFO("Entities extracted: " + std::to_string(m_FrameDiagnostics.entitiesExtracted));
    LOG_INFO("Opaque items: " + std::to_string(m_FrameDiagnostics.opaqueItems));
    LOG_INFO("Transparent items: " + std::to_string(m_FrameDiagnostics.transparentItems));
    LOG_INFO("Instances uploaded: " + std::to_string(m_FrameDiagnostics.instancesUploaded));
    LOG_INFO("Draw calls: " + std::to_string(m_FrameDiagnostics.drawCalls));
    LOG_INFO("Triangles: " + std::to_string(m_FrameDiagnostics.triangles));
    LOG_INFO("Rejected items: " + std::to_string(m_FrameDiagnostics.rejectedItems));
    LOG_INFO("Validation errors: " + std::to_string(m_FrameDiagnostics.validationErrors));

    s_CurrentDiagnostics = nullptr;

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

        VkVertexInputBindingDescription bindingDesc = PbrVertex::GetBindingDescription();
        auto attrDescs = PbrVertex::GetAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDescs.size());
        vertexInputInfo.pVertexAttributeDescriptions = attrDescs.data();

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
        raster.cullMode = RasterConvention::CullMode;
        raster.frontFace = RasterConvention::FrontFace;
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

        VkVertexInputBindingDescription bindingDesc = PbrVertex::GetBindingDescription();
        auto attrDescs = PbrVertex::GetAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDescs.size());
        vertexInputInfo.pVertexAttributeDescriptions = attrDescs.data();

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
        raster.cullMode = RasterConvention::CullMode;
        raster.frontFace = RasterConvention::FrontFace;
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

        // Shadow render pass is depth-only (0 color attachments).
        // VkPipelineColorBlendStateCreateInfo must have attachmentCount = 0 to match.
        VkPipelineColorBlendStateCreateInfo colorBlend{};
        colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlend.attachmentCount = 0;
        colorBlend.pAttachments = nullptr;

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

        VK_CHECK(vkCreateGraphicsPipelines(resources.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_ShadowPipeline.pipeline));
        m_ShadowPipeline.name = "ShadowPipeline";
        m_ShadowPipeline.layout = m_ShadowPipelineLayout;
        m_ShadowPipeline.compatibleRenderPass = m_ShadowRenderPass;
        m_ShadowPipeline.colorAttachmentCount = 0;
        m_ShadowPipeline.vertexStride = sizeof(PbrVertex);
        shadowPipeline = m_ShadowPipeline.pipeline;

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

        VkVertexInputBindingDescription bindingDesc = PbrVertex::GetBindingDescription();
        auto attrDescs = PbrVertex::GetAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDescs.size());
        vertexInputInfo.pVertexAttributeDescriptions = attrDescs.data();

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

        VK_CHECK(vkCreateGraphicsPipelines(resources.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_DepthPipeline.pipeline));
        m_DepthPipeline.name = "DepthPipeline";
        m_DepthPipeline.layout = resources.pipelineLayout;
        m_DepthPipeline.compatibleRenderPass = m_DepthRenderPass;
        m_DepthPipeline.colorAttachmentCount = 0;
        m_DepthPipeline.vertexStride = sizeof(PbrVertex);

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

    // Create light culling compute pipeline
    if (m_LightCullingPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(resources.device, m_LightCullingPipeline, nullptr);
        m_LightCullingPipeline = VK_NULL_HANDLE;
    }
    if (m_LightCullingPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(resources.device, m_LightCullingPipelineLayout, nullptr);
        m_LightCullingPipelineLayout = VK_NULL_HANDLE;
    }

    VkDescriptorSetLayout cullingSetLayout = gpuScene.GetLightCullingDescriptorSetLayout();
    if (cullingSetLayout != VK_NULL_HANDLE) {
        VkPipelineLayoutCreateInfo cullingLayoutInfo{};
        cullingLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        cullingLayoutInfo.setLayoutCount = 1;
        cullingLayoutInfo.pSetLayouts = &cullingSetLayout;
        cullingLayoutInfo.pushConstantRangeCount = 0;
        cullingLayoutInfo.pPushConstantRanges = nullptr;

        VK_CHECK(vkCreatePipelineLayout(resources.device, &cullingLayoutInfo, nullptr, &m_LightCullingPipelineLayout));

        VkShaderModule cullingComputeShader = resources.loadShaderModule("shaders/light_culling.spv");
        if (cullingComputeShader != VK_NULL_HANDLE) {
            VkComputePipelineCreateInfo computePipelineInfo{};
            computePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            computePipelineInfo.layout = m_LightCullingPipelineLayout;
            computePipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            computePipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            computePipelineInfo.stage.module = cullingComputeShader;
            computePipelineInfo.stage.pName = "main";

            VK_CHECK(vkCreateComputePipelines(resources.device, VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &m_LightCullingPipeline));

            vkDestroyShaderModule(resources.device, cullingComputeShader, nullptr);
            LOG_INFO("Light culling compute pipeline created successfully.");
        } else {
            LOG_ERROR("Failed to load compute shader: shaders/light_culling.spv");
        }
    } else {
        LOG_ERROR("Light culling descriptor set layout is NULL!");
    }
    CreateGBufferPipeline();

    // Always attempt to build the offscreen post-process pipeline at the end of
    // initPipelines(). initPipelines() destroys m_OffscreenPostProcessPipeline early in the
    // function; if getOffscreenRenderPass() was null at that point the inline creation
    // was skipped, leaving the pipeline null for the whole session.  Calling
    // recreateOffscreenPostProcessPipeline() here ensures it is rebuilt whenever the
    // offscreen render pass is already available (it no-ops if still null).
    if (m_ViewportRenderer.isOffscreenRenderingEnabled()) {
        recreateOffscreenPostProcessPipeline();
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
    if (m_DepthHandles.empty() || m_ShadowHandles.empty() || frameIndex >= m_DepthHandles.size() || frameIndex >= m_ShadowHandles.size()) {
        return;
    }
    renderGraph.Clear();

    // Declare graph resources (Textures)
    TextureResourceDesc texDesc{};
    texDesc.width = 1920;
    texDesc.height = 1080;
    texDesc.format = VK_FORMAT_R8G8B8A8_UNORM;

    renderGraph.DeclareTexture("ShadowMap", texDesc);
    renderGraph.DeclareTexture("ShadowMapCascade1", texDesc);
    renderGraph.DeclareTexture("ShadowMapCascade2", texDesc);
    renderGraph.DeclareTexture("ShadowMapCascade3", texDesc);
    TextureResourceDesc atlasTexDesc = texDesc;
    atlasTexDesc.width = 2048;
    atlasTexDesc.height = 2048;
    atlasTexDesc.format = VK_FORMAT_D32_SFLOAT;
    renderGraph.DeclareTexture("ShadowAtlas", atlasTexDesc);
    renderGraph.DeclareTexture("DepthBuffer", texDesc);
    renderGraph.DeclareTexture("GBufferA", texDesc);
    renderGraph.DeclareTexture("GBufferB", texDesc);
    renderGraph.DeclareTexture("GBufferC", texDesc);
    renderGraph.DeclareTexture("GBufferD", texDesc);
    renderGraph.DeclareTexture("SSAO", texDesc);
    renderGraph.DeclareTexture("AOBlur", texDesc);
    
    TextureResourceDesc objIdDesc = texDesc;
    objIdDesc.format = VK_FORMAT_R32_UINT;
    renderGraph.DeclareTexture("GBufferObjectID", objIdDesc);

    TextureResourceDesc velDesc = texDesc;
    velDesc.format = VK_FORMAT_R16G16_SFLOAT;
    renderGraph.DeclareTexture("GBufferVelocity", velDesc);

    renderGraph.DeclareTexture("HZBImage", texDesc);

    TextureResourceDesc hdrDesc = texDesc;
    hdrDesc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    renderGraph.DeclareTexture("HDRColor", hdrDesc);
    renderGraph.DeclareTexture("HDRColorComposed", hdrDesc);
    renderGraph.DeclareTexture("BloomExtract", hdrDesc);
    renderGraph.DeclareTexture("BloomBlurA", hdrDesc);
    renderGraph.DeclareTexture("BloomBlurB", hdrDesc);
    renderGraph.DeclareTexture("BloomComposite", hdrDesc);
    renderGraph.DeclareTexture("LDRColor", texDesc);
    renderGraph.DeclareTexture("ViewportColor", texDesc);
    renderGraph.DeclareTexture("Swapchain", texDesc);
    renderGraph.DeclareTexture("Backbuffer", texDesc);

    // Register all 11 required passes in dependency order (topological sort will verify)
    
    // 1. Shadow Pass
    if (m_DebugConfig.enableShadowPass) {
        std::string passNames[4] = { "ShadowPass", "ShadowPassCascade1", "ShadowPassCascade2", "ShadowPassCascade3" };
        std::string textureNames[4] = { "ShadowMap", "ShadowMapCascade1", "ShadowMapCascade2", "ShadowMapCascade3" };
        for (uint32_t c = 0; c < 4; ++c) {
            renderGraph.RegisterPass(
                passNames[c],
                {},
                { textureNames[c] },
                PassID::Shadow,
                [this, c](VkCommandBuffer cmd) {
                    if (activeRenderScene.directionalLights.empty() || activeRenderScene.directionalLights[0].castShadows <= 0.0f) {
                        RenderTarget* target = m_RenderTargetManager.Get(m_ShadowHandlesCascades[c][frameIndex]);
                        if (target && target->currentLayout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) {
                            m_RenderTargetManager.Transition(cmd, m_ShadowHandlesCascades[c][frameIndex],
                                VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                0, VK_ACCESS_SHADER_READ_BIT);
                        }
                        return;
                    }

                    VkImage shadowImg = m_ShadowImagesCascades[c][frameIndex];
                    if (shadowImg == VK_NULL_HANDLE) return;

                    VkRenderPassBeginInfo rpInfo{};
                    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                    rpInfo.renderPass = m_ShadowRenderPass;
                    rpInfo.framebuffer = m_ShadowFramebuffersCascades[c][frameIndex];
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

                    if (m_ShadowPipeline.pipeline != VK_NULL_HANDLE) {
#ifndef NDEBUG
                        assert(m_ShadowPipeline.compatibleRenderPass == m_ShadowRenderPass);
                        assert(m_ShadowPipeline.vertexStride == sizeof(PbrVertex));
#endif
                        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ShadowPipeline.pipeline);

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
                            if (!item.mesh || !item.castShadows) {
                                continue;
                            }

                            if (!ValidateRenderItem(item, i, gpuScene)) {
                                m_FrameDiagnostics.rejectedItems++;
                                m_FrameDiagnostics.validationErrors++;
                                continue;
                            }

                            m_FrameDiagnostics.drawCalls++;
                            m_FrameDiagnostics.triangles += item.mesh->indexCount / 3;

                            ShadowPushConstants push{};
                            push.instanceIndex = i;
                            push.lightSpaceMatrix = m_LastLightSpaceMatrices[c];

                            vkCmdPushConstants(cmd, m_ShadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowPushConstants), &push);

                            item.mesh->bind(cmd);

                            bool useArena = GeometryArena::IsInitialized() && GeometryArena::IsEnabled() && item.mesh->handle.IsValid();
                            if (useArena) {
                                auto arenaPtr = GeometryArena::GetInstance();
                                const GeometryAllocation* alloc = arenaPtr->GetAllocation(item.mesh->handle);
                                if (alloc) {
                                    vkCmdDrawIndexed(cmd, alloc->indexCount, 1, alloc->firstIndex, alloc->vertexOffset, 0);
                                }
                            } else {
                                vkCmdDrawIndexed(cmd, item.mesh->indexCount, 1, 0, 0, 0);
                            }
                        }
                    }

                    vkCmdEndRenderPass(cmd);

                    RenderTarget* target = m_RenderTargetManager.Get(m_ShadowHandlesCascades[c][frameIndex]);
                    if (target) {
                        target->currentLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                    }
                },
                true,
                frameIndex < m_ShadowFramebuffersCascades[c].size() ? m_ShadowFramebuffersCascades[c][frameIndex] : VK_NULL_HANDLE,
                false,   // requiresPipeline: lambda guards null pipeline internally
                m_ShadowPipeline,
                {},
                { m_ShadowHandlesCascades[c][frameIndex] }
            );
        }
    }

    // 1.5 Shadow Atlas Pass
    renderGraph.RegisterPass(
        "ShadowAtlasPass",
        {},
        { "ShadowAtlas" },
        PassID::Shadow,
        [this](VkCommandBuffer cmd) {
            const auto& scheduled = gpuScene.GetScheduledShadows();
            if (scheduled.empty()) {
                VkRenderPassBeginInfo rpInfo{};
                rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                rpInfo.renderPass = m_ShadowRenderPass;
                rpInfo.framebuffer = m_ShadowAtlasFramebuffers[frameIndex];
                rpInfo.renderArea.offset = {0, 0};
                rpInfo.renderArea.extent = { 2048, 2048 };
                VkClearValue clearValue{};
                clearValue.depthStencil = { 1.0f, 0 };
                rpInfo.clearValueCount = 1;
                rpInfo.pClearValues = &clearValue;
                vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
                vkCmdEndRenderPass(cmd);

                RenderTarget* target = m_RenderTargetManager.Get(m_ShadowAtlasHandles[frameIndex]);
                if (target) {
                    target->currentLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                }
                return;
            }

            VkRenderPassBeginInfo rpInfo{};
            rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpInfo.renderPass = m_ShadowRenderPass;
            rpInfo.framebuffer = m_ShadowAtlasFramebuffers[frameIndex];
            rpInfo.renderArea.offset = {0, 0};
            rpInfo.renderArea.extent = { 2048, 2048 };

            VkClearValue clearValue{};
            clearValue.depthStencil = { 1.0f, 0 };
            rpInfo.clearValueCount = 1;
            rpInfo.pClearValues = &clearValue;

            vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

            if (m_ShadowPipeline.pipeline != VK_NULL_HANDLE) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ShadowPipeline.pipeline);

                VkDescriptorSet gpuSet = gpuScene.GetDescriptorSet(frameIndex);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ShadowPipelineLayout, 0, 1, &gpuSet, 0, nullptr);

                struct ShadowPushConstants {
                    uint32_t instanceIndex;
                    uint32_t pad0, pad1, pad2;
                    glm::mat4 lightSpaceMatrix;
                };

                const auto& items = renderQueue.getItems();

                for (const auto& sched : scheduled) {
                    VkViewport viewport{};
                    viewport.x = static_cast<float>(sched.tileX);
                    viewport.y = static_cast<float>(sched.tileY);
                    viewport.width = static_cast<float>(sched.tileSize);
                    viewport.height = static_cast<float>(sched.tileSize);
                    viewport.minDepth = 0.0f;
                    viewport.maxDepth = 1.0f;
                    vkCmdSetViewport(cmd, 0, 1, &viewport);

                    VkRect2D scissor{};
                    scissor.offset = { static_cast<int32_t>(sched.tileX), static_cast<int32_t>(sched.tileY) };
                    scissor.extent = { sched.tileSize, sched.tileSize };
                    vkCmdSetScissor(cmd, 0, 1, &scissor);

                    glm::mat4 lightSpaceMatrix = sched.projMatrix * sched.viewMatrix;

                    for (uint32_t i = 0; i < items.size(); ++i) {
                        const RenderItem& item = items[i];
                        if (!item.mesh || !item.castShadows) {
                            continue;
                        }

                        if (!ValidateRenderItem(item, i, gpuScene)) {
                            continue;
                        }

                        ShadowPushConstants push{};
                        push.instanceIndex = i;
                        push.lightSpaceMatrix = lightSpaceMatrix;

                        vkCmdPushConstants(cmd, m_ShadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowPushConstants), &push);

                        item.mesh->bind(cmd);
                        bool useArena = GeometryArena::IsInitialized() && GeometryArena::IsEnabled() && item.mesh->handle.IsValid();
                        if (useArena) {
                            auto arenaPtr = GeometryArena::GetInstance();
                            const GeometryAllocation* alloc = arenaPtr->GetAllocation(item.mesh->handle);
                            if (alloc) {
                                vkCmdDrawIndexed(cmd, alloc->indexCount, 1, alloc->firstIndex, alloc->vertexOffset, 0);
                            }
                        } else {
                            vkCmdDrawIndexed(cmd, item.mesh->indexCount, 1, 0, 0, 0);
                        }
                    }
                }
            }

            vkCmdEndRenderPass(cmd);

            RenderTarget* target = m_RenderTargetManager.Get(m_ShadowAtlasHandles[frameIndex]);
            if (target) {
                target->currentLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            }
        },
        true,
        m_ShadowAtlasFramebuffers[frameIndex],
        false,   // requiresPipeline: lambda already guards null pipeline (just clears atlas)
        m_ShadowPipeline,
        {},
        { m_ShadowAtlasHandles[frameIndex] }
    );

    // 2. Depth Prepass
    if (m_DebugConfig.enableDepthPrepass) {
        VkFramebuffer depthFb = VK_NULL_HANDLE;
        if (m_ViewportRenderer.isOffscreenRenderingEnabled() && frameIndex < m_OffscreenDepthFramebuffers.size()) {
            depthFb = m_OffscreenDepthFramebuffers[frameIndex];
        } else if (frameIndex < m_DepthFramebuffers.size()) {
            depthFb = m_DepthFramebuffers[frameIndex];
        }

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

#ifndef NDEBUG
                if (m_DepthPipeline.pipeline != VK_NULL_HANDLE) {
                    assert(m_DepthPipeline.compatibleRenderPass == m_DepthRenderPass);
                    assert(m_DepthPipeline.vertexStride == sizeof(PbrVertex));
                }
#endif

                // Draw meshes in queue using MeshRenderer with depth pipeline override
                MeshRenderer::DrawQueue(cmd, resources.pipelineLayout, renderQueue, m_DepthPipeline, &gpuScene);

                vkCmdEndRenderPass(cmd);
                RenderTarget* depthTarget = m_RenderTargetManager.Get(m_DepthHandles[frameIndex]);
                if (depthTarget) {
                    depthTarget->currentLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                }
            },
            true,
            depthFb,
            true,
            m_DepthPipeline,
            { m_ShadowHandles[frameIndex] },
            { m_DepthHandles[frameIndex] }
        );
    }

    // 3. GBuffer Pass (performs geometry queue rendering)
    VkFramebuffer gbufferFb = VK_NULL_HANDLE;
    if (m_ViewportRenderer.isOffscreenRenderingEnabled() && frameIndex < m_OffscreenGBufferFramebuffers.size()) {
        gbufferFb = m_OffscreenGBufferFramebuffers[frameIndex];
    } else if (frameIndex < m_GBufferFramebuffers.size()) {
        gbufferFb = m_GBufferFramebuffers[frameIndex];
    }

    renderGraph.RegisterPass(
        "GBufferPass",
        {"DepthBuffer"},
        {"GBufferA", "GBufferB", "GBufferC", "GBufferD", "GBufferObjectID", "GBufferVelocity"},
        PassID::Geometry,
        [this](VkCommandBuffer cmd) {
            if (!m_DebugConfig.enableDepthPrepass) {
                RenderTarget* depthTarget = m_RenderTargetManager.Get(m_DepthHandles[frameIndex]);
                if (depthTarget && depthTarget->currentLayout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
                    m_RenderTargetManager.Transition(
                        cmd,
                        m_DepthHandles[frameIndex],
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                        VK_ACCESS_SHADER_READ_BIT,
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                        "GBufferPass",
                        frameIndex
                    );
                }
            }
            m_RenderTargetManager.AssertLayout(m_DepthHandles[frameIndex], VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

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

            std::array<VkClearValue, 6> clearVals{};
            clearVals[0].color = {{0.035f, 0.040f, 0.050f, 1.0f}};
            clearVals[1].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
            clearVals[2].color = {{0.0f, 1.0f, 0.0f, 1.0f}};
            clearVals[3].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
            clearVals[4].color.uint32[0] = 0xFFFFFFFF;
            clearVals[4].color.uint32[1] = 0xFFFFFFFF;
            clearVals[4].color.uint32[2] = 0xFFFFFFFF;
            clearVals[4].color.uint32[3] = 0xFFFFFFFF;
            clearVals[5].depthStencil = {1.0f, 0};
            rpInfo.clearValueCount = 6;
            rpInfo.pClearValues = clearVals.data();

            vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

            if (!m_DebugConfig.enableDepthPrepass) {
                VkClearAttachment clearAttachment{};
                clearAttachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                clearAttachment.clearValue.depthStencil = { 1.0f, 0 };

                VkClearRect rect{};
                rect.rect.offset = {0, 0};
                rect.rect.extent = { m_DepthWidth, m_DepthHeight };
                rect.baseArrayLayer = 0;
                rect.layerCount = 1;

                vkCmdClearAttachments(cmd, 1, &clearAttachment, 1, &rect);
            }

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

#ifndef NDEBUG
            if (m_GBufferPipeline.pipeline != VK_NULL_HANDLE) {
                assert(m_GBufferPipeline.compatibleRenderPass == m_GBufferRenderPass);
                assert(m_GBufferPipeline.vertexStride == sizeof(PbrVertex));
            }
#endif
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GBufferPipeline.pipeline);

            // Bind GPUScene descriptor set to Set 0
            VkDescriptorSet gpuSet = gpuScene.GetDescriptorSet(frameIndex);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.pipelineLayout, 0, 1, &gpuSet, 0, nullptr);

            // Draw meshes in queue using MeshRenderer
            MeshRenderer::DrawQueue(cmd, resources.pipelineLayout, renderQueue, m_GBufferPipeline, &gpuScene);

            vkCmdEndRenderPass(cmd);
            std::array<RenderTargetHandle, 6> gbufferHandles = {
                m_GBufferAHandles[frameIndex],
                m_GBufferBHandles[frameIndex],
                m_GBufferCHandles[frameIndex],
                m_GBufferDHandles[frameIndex],
                m_ObjectIDHandles[frameIndex],
                m_GBufferVelocityHandles[frameIndex]
            };
            for (auto h : gbufferHandles) {
                RenderTarget* target = m_RenderTargetManager.Get(h);
                if (target) {
                    target->currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                }
            }
            RenderTarget* depthTarget = m_RenderTargetManager.Get(m_DepthHandles[frameIndex]);
            if (depthTarget) {
                depthTarget->currentLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }
        },
        true,
        gbufferFb,
        true,
        m_GBufferPipeline,
        { m_DepthHandles[frameIndex] },
        { m_GBufferAHandles[frameIndex], m_GBufferBHandles[frameIndex], m_GBufferCHandles[frameIndex], m_GBufferDHandles[frameIndex], m_ObjectIDHandles[frameIndex], m_GBufferVelocityHandles[frameIndex] }
    );

    // 3b. HZB Generation Pass
    renderGraph.RegisterPass(
        "HZBPass",
        {"DepthBuffer"},
        {"HZBImage"},
        PassID::Lighting,
        [this](VkCommandBuffer cmd) {
            RenderTarget* depthTarget = m_RenderTargetManager.Get(m_DepthHandles[frameIndex]);
            if (depthTarget && depthTarget->currentLayout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) {
                m_RenderTargetManager.Transition(
                    cmd,
                    m_DepthHandles[frameIndex],
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                    VK_ACCESS_SHADER_READ_BIT,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    "HZBPass",
                    frameIndex
                );
            }
            m_RenderTargetManager.AssertLayout(m_DepthHandles[frameIndex], VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

            VkImage depthImage = m_ViewportRenderer.isOffscreenRenderingEnabled() ?
                m_ViewportRenderer.getOffscreenDepthImage(frameIndex) : (frameIndex < m_DepthImages.size() ? m_DepthImages[frameIndex] : VK_NULL_HANDLE);
            VkImageView depthView = m_ViewportRenderer.isOffscreenRenderingEnabled() ?
                m_ViewportRenderer.getOffscreenDepthImageView(frameIndex) : (frameIndex < m_DepthImageViews.size() ? m_DepthImageViews[frameIndex] : VK_NULL_HANDLE);

            if (depthImage != VK_NULL_HANDLE && depthView != VK_NULL_HANDLE) {
                m_HZBPass.Execute(cmd, resources, frameIndex, depthView, depthImage, m_DepthWidth, m_DepthHeight);
            }
        },
        false,            // isRasterPass
        VK_NULL_HANDLE,   // fb
        false,            // requiresPipeline: HZBPass binds its own pipeline internally
        VK_NULL_HANDLE,   // handled internally
        {},
        {}
    );

    // 4. SSAO Pass
    if (m_DebugConfig.enableSSAO) {
        renderGraph.RegisterPass(
            "SSAOPass",
            {"DepthBuffer", "GBufferA", "GBufferB"},
            {"SSAO"},
            PassID::Lighting,
            [this](VkCommandBuffer cmd) {
                // Update constant buffer first
                if (frameIndex < m_SSAOConstantAllocations.size() && m_SSAOConstantAllocations[frameIndex] != nullptr) {
                    struct SSAOConstantBufferData {
                        glm::vec4 samples[64];
                        glm::mat4 projection;
                        float radius;
                        float bias;
                        float intensity;
                        float screenWidth;
                        float screenHeight;
                        float enabled;
                        float pad0, pad1;
                    } ubo{};
                    
                    std::memcpy(ubo.samples, m_SSAOKernel.data(), 64 * sizeof(glm::vec4));
                    ubo.projection = activeFrameContext.projectionMatrix;
                    ubo.radius = m_SSAOSettings.radius;
                    ubo.bias = m_SSAOSettings.bias;
                    ubo.intensity = m_SSAOSettings.intensity;
                    ubo.screenWidth = static_cast<float>(m_DepthWidth);
                    ubo.screenHeight = static_cast<float>(m_DepthHeight);
                    ubo.enabled = m_SSAOSettings.enabled ? 1.0f : 0.0f;
                    
                    void* mappedData = nullptr;
                    vmaMapMemory(resources.allocator, m_SSAOConstantAllocations[frameIndex], &mappedData);
                    std::memcpy(mappedData, &ubo, sizeof(ubo));
                    vmaUnmapMemory(resources.allocator, m_SSAOConstantAllocations[frameIndex]);
                }

                // Transition depth image to DEPTH_STENCIL_READ_ONLY_OPTIMAL for sampling
                RenderTarget* depthTarget = m_RenderTargetManager.Get(m_DepthHandles[frameIndex]);
                if (depthTarget && depthTarget->currentLayout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) {
                    m_RenderTargetManager.Transition(
                        cmd,
                        m_DepthHandles[frameIndex],
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        VK_ACCESS_SHADER_READ_BIT,
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                        "SSAOPass",
                        frameIndex
                    );
                }
                m_RenderTargetManager.AssertLayout(m_DepthHandles[frameIndex], VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

                VkRenderPassBeginInfo rpInfo{};
                rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                rpInfo.renderPass = m_SSAORenderPass;
                rpInfo.framebuffer = frameIndex < m_SSAOFramebuffers.size() ? m_SSAOFramebuffers[frameIndex] : VK_NULL_HANDLE;
                rpInfo.renderArea.offset = {0, 0};
                rpInfo.renderArea.extent = { m_DepthWidth, m_DepthHeight };

                VkClearValue clearValue{};
                clearValue.color = {{1.0f, 1.0f, 1.0f, 1.0f}};
                rpInfo.clearValueCount = 1;
                rpInfo.pClearValues = &clearValue;

                if (rpInfo.framebuffer != VK_NULL_HANDLE && m_SSAOPipeline != VK_NULL_HANDLE) {
                    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

                    VkViewport viewport{};
                    viewport.x = 0.0f; viewport.y = 0.0f;
                    viewport.width = static_cast<float>(m_DepthWidth);
                    viewport.height = static_cast<float>(m_DepthHeight);
                    viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f;
                    vkCmdSetViewport(cmd, 0, 1, &viewport);

                    VkRect2D scissor{};
                    scissor.offset = {0, 0};
                    scissor.extent = { m_DepthWidth, m_DepthHeight };
                    vkCmdSetScissor(cmd, 0, 1, &scissor);

                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_SSAOPipeline);

                    // Bind Set 0: GPUScene (camera, lights)
                    VkDescriptorSet gpuSet = gpuScene.GetDescriptorSet(frameIndex);
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_SSAOPipelineLayout, 0, 1, &gpuSet, 0, nullptr);

                    // Bind Set 1: SSAO resources
                    VkDescriptorSet ssaoSet = frameIndex < m_SSAODescriptorSets.size() ? m_SSAODescriptorSets[frameIndex] : VK_NULL_HANDLE;
                    if (ssaoSet != VK_NULL_HANDLE) {
                        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_SSAOPipelineLayout, 1, 1, &ssaoSet, 0, nullptr);
                    }

                    vkCmdDraw(cmd, 3, 1, 0, 0);
                    vkCmdEndRenderPass(cmd);
                    RenderTarget* target = m_RenderTargetManager.Get(m_SSAOHandles[frameIndex]);
                    if (target) {
                        target->currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    }
                }
            },
            false,
            VK_NULL_HANDLE,
            false,
            VK_NULL_HANDLE,
            { m_DepthHandles[frameIndex], m_GBufferAHandles[frameIndex], m_GBufferBHandles[frameIndex] },
            { m_SSAOHandles[frameIndex] }
        );
    }
 
    // 5. AO Blur Pass
    if (m_DebugConfig.enableSSAO) {
        renderGraph.RegisterPass(
            "AOBlurPass",
            {"SSAO"},
            {"AOBlur"},
            PassID::Lighting,
            [this](VkCommandBuffer cmd) {
                VkRenderPassBeginInfo rpInfo{};
                rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                rpInfo.renderPass = m_SSAORenderPass;
                rpInfo.framebuffer = frameIndex < m_SSAOBlurredFramebuffers.size() ? m_SSAOBlurredFramebuffers[frameIndex] : VK_NULL_HANDLE;
                rpInfo.renderArea.offset = {0, 0};
                rpInfo.renderArea.extent = { m_DepthWidth, m_DepthHeight };

                VkClearValue clearValue{};
                clearValue.color = {{1.0f, 1.0f, 1.0f, 1.0f}};
                rpInfo.clearValueCount = 1;
                rpInfo.pClearValues = &clearValue;

                if (rpInfo.framebuffer != VK_NULL_HANDLE && m_SSAOBlurPipeline != VK_NULL_HANDLE) {
                    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

                    VkViewport viewport{};
                    viewport.x = 0.0f; viewport.y = 0.0f;
                    viewport.width = static_cast<float>(m_DepthWidth);
                    viewport.height = static_cast<float>(m_DepthHeight);
                    viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f;
                    vkCmdSetViewport(cmd, 0, 1, &viewport);

                    VkRect2D scissor{};
                    scissor.offset = {0, 0};
                    scissor.extent = { m_DepthWidth, m_DepthHeight };
                    vkCmdSetScissor(cmd, 0, 1, &scissor);

                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_SSAOBlurPipeline);

                    // Bind Set 0: SSAO Blur resources
                    VkDescriptorSet blurSet = frameIndex < m_SSAOBlurDescriptorSets.size() ? m_SSAOBlurDescriptorSets[frameIndex] : VK_NULL_HANDLE;
                    if (blurSet != VK_NULL_HANDLE) {
                        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_SSAOBlurPipelineLayout, 0, 1, &blurSet, 0, nullptr);
                    }

                    vkCmdDraw(cmd, 3, 1, 0, 0);
                    vkCmdEndRenderPass(cmd);
                    RenderTarget* target = m_RenderTargetManager.Get(m_SSAOBlurredHandles[frameIndex]);
                    if (target) {
                        target->currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    }
                }
            },
            false,
            VK_NULL_HANDLE,
            false,
            VK_NULL_HANDLE,
            { m_SSAOHandles[frameIndex] },
            { m_SSAOBlurredHandles[frameIndex] }
        );
    }

    // 5.5 Light Culling Compute Pass
    if (m_DebugConfig.enableLightCulling) {
        renderGraph.RegisterPass(
            "LightCullingPass",
            {},
            {},
            PassID::Lighting,
            [this](VkCommandBuffer cmd) {
                if (m_LightCullingPipeline == VK_NULL_HANDLE) {
                    return;
                }

                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_LightCullingPipeline);

                VkDescriptorSet cullingSet = gpuScene.GetLightCullingDescriptorSet(frameIndex);
                if (cullingSet != VK_NULL_HANDLE) {
                    vkCmdBindDescriptorSets(
                        cmd,
                        VK_PIPELINE_BIND_POINT_COMPUTE,
                        m_LightCullingPipelineLayout,
                        0, 1, &cullingSet,
                        0, nullptr
                    );
                }

                Omnix::Radiance::ClusterSettings settings{};
                uint32_t viewportWidth = m_DepthWidth;
                uint32_t viewportHeight = m_DepthHeight;
                if (viewportWidth == 0) viewportWidth = 1280;
                if (viewportHeight == 0) viewportHeight = 720;
                uint32_t tileCountX = (viewportWidth + settings.tileSizeX - 1) / settings.tileSizeX;
                uint32_t tileCountY = (viewportHeight + settings.tileSizeY - 1) / settings.tileSizeY;
                uint32_t depthSliceCount = settings.depthSliceCount;
                uint32_t clusterCount = tileCountX * tileCountY * depthSliceCount;
                uint32_t groupCount = (clusterCount + 63) / 64;

                vkCmdDispatch(cmd, groupCount, 1, 1);

                // Add pipeline barrier to transition cluster range/index buffer writes to fragment shader reads
                const auto& frameRes = gpuScene.GetFrameResources(frameIndex);
                if (frameRes.clusterRangeBuffer != VK_NULL_HANDLE && frameRes.clusterLightIndexBuffer != VK_NULL_HANDLE) {
                    std::array<VkBufferMemoryBarrier, 2> barriers{};

                    barriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                    barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                    barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barriers[0].buffer = frameRes.clusterRangeBuffer;
                    barriers[0].offset = 0;
                    barriers[0].size = VK_WHOLE_SIZE;

                    barriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                    barriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                    barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barriers[1].buffer = frameRes.clusterLightIndexBuffer;
                    barriers[1].offset = 0;
                    barriers[1].size = VK_WHOLE_SIZE;

                    vkCmdPipelineBarrier(
                        cmd,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        0,
                        0, nullptr,
                        static_cast<uint32_t>(barriers.size()), barriers.data(),
                        0, nullptr
                    );
                }
            },
            false,            // isRasterPass
            VK_NULL_HANDLE,   // fb
            true,             // bindsPipeline
            m_LightCullingPipeline,
            {},               // readTargets
            {}                // writeTargets
        );
    }

    // 6. Deferred Lighting Pass
    if (m_DebugConfig.enableDeferredLighting) {
        renderGraph.RegisterPass(
            "DeferredLightingPass",
            // Only list resources that are always produced this frame.
            // AOBlur, ShadowMapCascadeN, GBufferObjectID, GBufferVelocity are optional
            // (produced only when their source passes are enabled); omit them here
            // to prevent ValidatePass from aborting when those passes are disabled.
            {"GBufferA", "GBufferB", "GBufferC", "GBufferD", "DepthBuffer", "ShadowAtlas"},
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
     
                // Transition depth from DEPTH_STENCIL_ATTACHMENT_OPTIMAL to DEPTH_STENCIL_READ_ONLY_OPTIMAL
                // so it can be sampled by the deferred lighting fragment shader.
                RenderTarget* depthTarget = m_RenderTargetManager.Get(m_DepthHandles[frameIndex]);
                if (depthTarget && depthTarget->currentLayout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) {
                    m_RenderTargetManager.Transition(
                        cmd,
                        m_DepthHandles[frameIndex],
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        VK_ACCESS_SHADER_READ_BIT,
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                        "DeferredLightingPass",
                        frameIndex
                    );
                }
                m_RenderTargetManager.AssertLayout(m_DepthHandles[frameIndex], VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

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

                // Bind Set 3: Local Lights
                VkDescriptorSet localLightsSet = gpuScene.GetLocalLightsDescriptorSet(frameIndex);
                if (localLightsSet != VK_NULL_HANDLE) {
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DeferredPipelineLayout, 3, 1, &localLightsSet, 0, nullptr);
                }

                // Push Constants: Local Light Count
                uint32_t localLightCount = gpuScene.GetLocalLightCount(frameIndex);
                vkCmdPushConstants(cmd, m_DeferredPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t), &localLightCount);

                // Draw fullscreen triangle
                vkCmdDraw(cmd, 3, 1, 0, 0);

                vkCmdEndRenderPass(cmd);
                // NOTE: m_HDRRenderPass has finalLayout = SHADER_READ_ONLY_OPTIMAL, so
                // HDRColor is already in SHADER_READ_ONLY after vkCmdEndRenderPass.
                // No explicit HDR barrier is needed here.
                RenderTarget* target = m_RenderTargetManager.Get(m_HDRColorHandles[frameIndex]);
                if (target) {
                    target->currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                }
                // NOTE: Depth is NOT used by this render pass (pDepthStencilAttachment = nullptr).
                // Depth stays in SHADER_READ_ONLY_OPTIMAL (from GBufferPass) so SSRPass can
                // read it. TransparentPass will transition depth in its own barrier.
            },
            false,            // requiresFramebuffer: resolved at execute-time inside lambda
            VK_NULL_HANDLE,   // framebuffer resolved at execute-time inside lambda
            true,
            m_DeferredLightingPipeline,
            { m_GBufferAHandles[frameIndex], m_GBufferBHandles[frameIndex], m_GBufferCHandles[frameIndex], m_GBufferDHandles[frameIndex], m_DepthHandles[frameIndex] },
            { m_HDRColorHandles[frameIndex] }
        );
    }

    // 6b. SSR Pass
    if (m_DebugConfig.enableDeferredLighting) {
        renderGraph.RegisterPass(
            "SSRPass",
            {"GBufferA", "GBufferB", "GBufferC", "GBufferD", "DepthBuffer", "HZBImage"},
            {"HDRColorComposed"},
            PassID::Lighting,
            [this](VkCommandBuffer cmd) {
            // Assert that DepthBuffer is in SHADER_READ_ONLY (since it was transitioned by GBuffer / HZB)
            m_RenderTargetManager.AssertLayout(m_DepthHandles[frameIndex], VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

            // Transition HDRColor (which is SHADER_READ_ONLY_OPTIMAL) from color attachment write in DeferredLighting
            // to compute shader read in SSR
            m_RenderTargetManager.Transition(
                cmd,
                m_HDRColorHandles[frameIndex],
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                "SSRPass",
                frameIndex
            );
            m_RenderTargetManager.AssertLayout(m_HDRColorHandles[frameIndex], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            // Transition HDRColorComposed layout to GENERAL
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = frameIndex < m_HDRColorComposedImages.size() ? m_HDRColorComposedImages[frameIndex] : VK_NULL_HANDLE;
            barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

                // Bind pipeline and descriptor sets
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_SSRPipeline);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_SSRPipelineLayout, 0, 1, &m_SSRDescriptorSets[frameIndex], 0, nullptr);

                uint32_t groupX = (m_DepthWidth + 15) / 16;
                uint32_t groupY = (m_DepthHeight + 15) / 16;
                vkCmdDispatch(cmd, groupX, groupY, 1);

                RenderTarget* target = m_RenderTargetManager.Get(m_HDRColorComposedHandles[frameIndex]);
                if (target) {
                    target->currentLayout = VK_IMAGE_LAYOUT_GENERAL;
                }
            },
            false,
            VK_NULL_HANDLE,
            true,
            m_SSRPipeline,
            {},
            {}
        );

        renderGraph.RegisterPass(
            "SSRComposePass",
            {"HDRColorComposed"},
            {"HDRColor"},
            PassID::Lighting,
            [this](VkCommandBuffer cmd) {
                const RenderTarget* srcRT = m_RenderTargetManager.Get(m_HDRColorComposedHandles[frameIndex]);
                const RenderTarget* dstRT = m_RenderTargetManager.Get(m_HDRColorHandles[frameIndex]);
                if (srcRT && dstRT) {
                    // Transition src to TRANSFER_SRC
                    VkImageMemoryBarrier barrierSrc{};
                    barrierSrc.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    barrierSrc.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                    barrierSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                    barrierSrc.image = srcRT->image;
                    barrierSrc.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                    barrierSrc.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                    barrierSrc.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

                    // Transition dst to TRANSFER_DST
                    VkImageMemoryBarrier barrierDst{};
                    barrierDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    barrierDst.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    barrierDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                    barrierDst.image = dstRT->image;
                    barrierDst.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                    barrierDst.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    barrierDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

                    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrierSrc);
                    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrierDst);

                    VkImageCopy copyRegion{};
                    copyRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                    copyRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                    copyRegion.extent = {m_DepthWidth, m_DepthHeight, 1};

                    vkCmdCopyImage(cmd, srcRT->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstRT->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

                    // Transition dst back to SHADER_READ_ONLY_OPTIMAL
                    barrierDst.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                    barrierDst.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    barrierDst.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                    barrierDst.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrierDst);
                    RenderTarget* target = m_RenderTargetManager.Get(m_HDRColorHandles[frameIndex]);
                    if (target) {
                        target->currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    }
                }
            },
            false,
            VK_NULL_HANDLE,
            false,
            VK_NULL_HANDLE,
            {},
            {}
        );
    }

    // 7. Transparent Pass
    if (m_DebugConfig.enableTransparentPass) {
        VkFramebuffer transFb = VK_NULL_HANDLE;
        if (m_ViewportRenderer.isOffscreenRenderingEnabled() && frameIndex < m_OffscreenTransparentFramebuffers.size()) {
            transFb = m_OffscreenTransparentFramebuffers[frameIndex];
        } else if (frameIndex < m_TransparentFramebuffers.size()) {
            transFb = m_TransparentFramebuffers[frameIndex];
        }

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

                // Transition depth: DEPTH_STENCIL_READ_ONLY_OPTIMAL → DEPTH_STENCIL_ATTACHMENT_OPTIMAL.
                // SSRPass (compute) has finished reading depth. TransparentPass renders
                // geometry with depth test, so it needs DEPTH_STENCIL_ATTACHMENT_OPTIMAL.
                // m_TransparentRenderPass expects initialLayout = DEPTH_STENCIL_ATTACHMENT_OPTIMAL.
                RenderTarget* depthTarget = m_RenderTargetManager.Get(m_DepthHandles[frameIndex]);
                if (depthTarget) {
                    m_RenderTargetManager.Transition(
                        cmd,
                        m_DepthHandles[frameIndex],
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                        VK_ACCESS_SHADER_READ_BIT,
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                        "TransparentPass_Pre",
                        frameIndex
                    );
                }
                m_RenderTargetManager.AssertLayout(m_DepthHandles[frameIndex], VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

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
                if (m_RadianceSettings.showGrid && m_GridPipeline != VK_NULL_HANDLE) {
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

                    GBufferPushConstants push{};
                    push.instanceIndex = opaqueCount + i;
                    vkCmdPushConstants(cmd, resources.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GBufferPushConstants), &push);

                    item.mesh->bind(cmd);

                    bool useArena = GeometryArena::IsInitialized() && GeometryArena::IsEnabled() && item.mesh->handle.IsValid();
                    if (useArena) {
                        auto arenaPtr = GeometryArena::GetInstance();
                        const GeometryAllocation* alloc = arenaPtr->GetAllocation(item.mesh->handle);
                        if (alloc) {
                            vkCmdDrawIndexed(cmd, alloc->indexCount, 1, alloc->firstIndex, alloc->vertexOffset, 0);
                        }
                    } else {
                        vkCmdDrawIndexed(cmd, item.mesh->indexCount, 1, 0, 0, 0);
                    }
                }

                vkCmdEndRenderPass(cmd);

                // Transition depth back to DEPTH_STENCIL_READ_ONLY_OPTIMAL so that subsequent compute
                // passes (TAAPass, HZBPass, SSRPass, etc. on next frame) and PostProcessPass can read it.
                if (depthTarget) {
                    m_RenderTargetManager.Transition(
                        cmd,
                        m_DepthHandles[frameIndex],
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        VK_ACCESS_SHADER_READ_BIT,
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                        "TransparentPass_Post",
                        frameIndex
                    );
                }
                m_RenderTargetManager.AssertLayout(m_DepthHandles[frameIndex], VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

                RenderTarget* hdrTarget = m_RenderTargetManager.Get(m_HDRColorHandles[frameIndex]);
                if (hdrTarget) {
                    hdrTarget->currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                }
            },
            true,
            transFb,
            false,
            VK_NULL_HANDLE,
            { m_DepthHandles[frameIndex] },
            { m_HDRColorHandles[frameIndex] }
        );
    }

    // 7.4. Temporal Anti-Aliasing (TAA) Compute Pass
    if (m_TAAPipeline != VK_NULL_HANDLE && m_TAAHistoryImages[0] != VK_NULL_HANDLE) {
        renderGraph.RegisterPass(
            "TAAPass",
            {"HDRColor", "GBufferVelocity", "DepthBuffer"},
            {},
            PassID::Lighting,
            [this](VkCommandBuffer cmd) {
                uint32_t readIndex = m_TAAHistoryIndex;
                uint32_t writeIndex = 1 - m_TAAHistoryIndex;

                VkDescriptorSet taaSet = m_TAADescriptorSets[frameIndex * 2 + readIndex];

                // Assert that DepthBuffer is in SHADER_READ_ONLY (since it was transitioned by GBuffer / HZB)
                m_RenderTargetManager.AssertLayout(m_DepthHandles[frameIndex], VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

                // Transition currentImage (HDRColor) to SHADER_READ_ONLY_OPTIMAL for compute read
                m_RenderTargetManager.Transition(
                    cmd,
                    m_HDRColorHandles[frameIndex],
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_ACCESS_SHADER_READ_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    "TAAPass",
                    frameIndex
                );
                m_RenderTargetManager.AssertLayout(m_HDRColorHandles[frameIndex], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

                VkImageMemoryBarrier barriers[2]{};
                for (int b = 0; b < 2; ++b) {
                    barriers[b].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    barriers[b].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barriers[b].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    barriers[b].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    barriers[b].subresourceRange.baseMipLevel = 0;
                    barriers[b].subresourceRange.levelCount = 1;
                    barriers[b].subresourceRange.baseArrayLayer = 0;
                    barriers[b].subresourceRange.layerCount = 1;
                }

                // 1. History Read Image — previous frame left this at TRANSFER_SRC_OPTIMAL
                barriers[0].image = m_TAAHistoryImages[readIndex];
                barriers[0].oldLayout = m_TAAInitialized ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
                barriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barriers[0].srcAccessMask = m_TAAInitialized ? VK_ACCESS_TRANSFER_READ_BIT : 0;
                barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                // 2. History Write Image — always transition from UNDEFINED (discard old)
                barriers[1].image = m_TAAHistoryImages[writeIndex];
                barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                barriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
                barriers[1].srcAccessMask = 0;
                barriers[1].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

                vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers);

                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_TAAPipeline);

                VkDescriptorSet sets[2] = {
                    taaSet,
                    gpuScene.GetDescriptorSet(frameIndex)
                };
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_TAAPipelineLayout, 0, 2, sets, 0, nullptr);

                struct TAAPushConstants {
                    glm::mat4 prevViewProjection;
                    float blendAlpha;
                    float padding[3];
                } pushConstants;

                pushConstants.prevViewProjection = m_PrevViewProjection;
                pushConstants.blendAlpha = 0.07f;

                vkCmdPushConstants(cmd, m_TAAPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(TAAPushConstants), &pushConstants);

                uint32_t groupX = (m_DepthWidth + 15) / 16;
                uint32_t groupY = (m_DepthHeight + 15) / 16;
                vkCmdDispatch(cmd, groupX, groupY, 1);

                // TAA always operates in HDR space — never use the offscreen (LDR/sRGB) image here.
                VkImage currentImage = (frameIndex < m_HDRColorImages.size()) ? m_HDRColorImages[frameIndex] : VK_NULL_HANDLE;

                VkImageMemoryBarrier copyBarrier{};
                copyBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                copyBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                copyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                copyBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyBarrier.subresourceRange.baseMipLevel = 0;
                copyBarrier.subresourceRange.levelCount = 1;
                copyBarrier.subresourceRange.baseArrayLayer = 0;
                copyBarrier.subresourceRange.layerCount = 1;
                copyBarrier.image = m_TAAHistoryImages[writeIndex];
                copyBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                copyBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                copyBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                copyBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

                vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &copyBarrier);

                m_RenderTargetManager.Transition(
                    cmd,
                    m_HDRColorHandles[frameIndex],
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_ACCESS_SHADER_READ_BIT,
                    VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    "TAAPass_CopyPrep",
                    frameIndex
                );

                VkImageCopy copyRegion{};
                copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.srcSubresource.mipLevel = 0;
                copyRegion.srcSubresource.baseArrayLayer = 0;
                copyRegion.srcSubresource.layerCount = 1;
                copyRegion.srcOffset = { 0, 0, 0 };
                copyRegion.dstSubresource = copyRegion.srcSubresource;
                copyRegion.dstOffset = { 0, 0, 0 };
                copyRegion.extent = { m_DepthWidth, m_DepthHeight, 1 };

                vkCmdCopyImage(cmd, m_TAAHistoryImages[writeIndex], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, currentImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

                m_RenderTargetManager.Transition(
                    cmd,
                    m_HDRColorHandles[frameIndex],
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_ACCESS_SHADER_READ_BIT,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    "TAAPass_PostCopy",
                    frameIndex
                );
                m_RenderTargetManager.AssertLayout(m_HDRColorHandles[frameIndex], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

                m_PrevViewProjection = m_CurrentViewProjection;
                m_TAAHistoryIndex = writeIndex;
                m_TAAInitialized  = true; // next frame barriers use correct oldLayout
            }
        );
    }

    // 7.5. Auto Exposure Compute Pass
    if (m_ExposurePipeline != VK_NULL_HANDLE && m_ExposureBuffer != VK_NULL_HANDLE) {
        renderGraph.RegisterPass(
            "ExposurePass",
            {"HDRColor"},
            {},
            PassID::Lighting,
            [this](VkCommandBuffer cmd) {
                m_RenderTargetManager.AssertLayout(m_HDRColorHandles[frameIndex], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

                // Buffer memory barrier to transition ExposureBuffer for writing
                VkBufferMemoryBarrier barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                barrier.buffer = m_ExposureBuffer;
                barrier.offset = 0;
                barrier.size = sizeof(float) * 2;
                barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

                vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);

                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_ExposurePipeline);

                // Bind Set 0
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_ExposurePipelineLayout, 0, 1, &m_ExposureDescriptorSets[frameIndex], 0, nullptr);

                // Push Constants
                struct ExposurePushConstants {
                    float deltaTime;
                    float minExposure;
                    float maxExposure;
                    float adaptationSpeed;
                    uint32_t autoExposureEnabled;
                } pushConstants;

                pushConstants.deltaTime = static_cast<float>(activeFrameContext.deltaTime);
                pushConstants.minExposure = m_RadianceSettings.exposure.minExposure;
                pushConstants.maxExposure = m_RadianceSettings.exposure.maxExposure;
                pushConstants.adaptationSpeed = m_RadianceSettings.exposure.adaptationSpeed;
                pushConstants.autoExposureEnabled = m_RadianceSettings.exposure.autoExposure ? 1u : 0u;

                vkCmdPushConstants(cmd, m_ExposurePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ExposurePushConstants), &pushConstants);

                // Dispatch 1 workgroup to perform the reduction
                vkCmdDispatch(cmd, 1, 1, 1);

                // Transition ExposureBuffer back for shader reading in post-processing
                VkBufferMemoryBarrier readBarrier{};
                readBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                readBarrier.buffer = m_ExposureBuffer;
                readBarrier.offset = 0;
                readBarrier.size = sizeof(float) * 2;
                readBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                readBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                readBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                readBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

                vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &readBarrier, 0, nullptr);
            }
        );
    }

    // 8. Post Process Pass
    if (m_DebugConfig.enablePostProcessing) {
        VkFramebuffer postProcessFb = VK_NULL_HANDLE;
        VkPipeline postProcessPipeline = VK_NULL_HANDLE;
        if (m_ViewportRenderer.isOffscreenRenderingEnabled()) {
            postProcessFb = m_ViewportRenderer.getOffscreenFramebuffer(frameIndex);
            postProcessPipeline = m_OffscreenPostProcessPipeline;
        } else {
            postProcessFb = currentSwapchainImageIndex < resources.swapChainFramebuffers.size() ? resources.swapChainFramebuffers[currentSwapchainImageIndex] : VK_NULL_HANDLE;
            postProcessPipeline = m_PostProcessPipeline;
        }

        renderGraph.RegisterPass(
            "PostProcessPass",
            {"HDRColor"},
            {"LDRColor"},
            PassID::PostProcess,
            [this](VkCommandBuffer cmd) {
                m_RenderTargetManager.AssertLayout(m_HDRColorHandles[frameIndex], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

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

                // Bind Set 1: Camera/Radiance descriptor set
                VkDescriptorSet cameraSet = gpuScene.GetDescriptorSet(frameIndex);
                if (cameraSet != VK_NULL_HANDLE) {
                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PostProcessPipelineLayout, 1, 1, &cameraSet, 0, nullptr);
                }

                m_PostProcessSettings.exposureMode = m_RadianceSettings.exposure.autoExposure ? ExposureMode::Auto : ExposureMode::Manual;
                m_PostProcessSettings.exposure = m_RadianceSettings.exposure.manualExposure;

                PostProcessPushConstants postConstants{};
                postConstants.exposure = m_PostProcessSettings.exposure;
                postConstants.gamma = m_PostProcessSettings.gamma;
                postConstants.bloomThreshold = m_PostProcessSettings.bloomThreshold;
                postConstants.bloomIntensity = m_PostProcessSettings.bloomIntensity;
                postConstants.exposureMode = static_cast<uint32_t>(m_PostProcessSettings.exposureMode);
                postConstants.enableTonemapping = m_PostProcessSettings.enableTonemapping ? 1u : 0u;
                postConstants.enableGammaCorrection = m_PostProcessSettings.enableGammaCorrection ? 1u : 0u;
                postConstants.debugBeforePostProcess = m_PostProcessSettings.debugBeforePostProcess ? 1u : 0u;
                postConstants.autoExposure = m_RadianceSettings.exposure.autoExposure ? 1.0f : 0.0f;
                postConstants.enableFog = m_PostProcessSettings.enableFog;
                postConstants.fogDensity = m_PostProcessSettings.fogDensity;
                postConstants.fogHeightFalloff = m_PostProcessSettings.fogHeightFalloff;

                // Color Grading
                postConstants.contrast = m_PostProcessSettings.contrast;
                postConstants.saturation = m_PostProcessSettings.saturation;
                postConstants.temp = m_PostProcessSettings.whiteBalanceTemp;
                postConstants.tint = m_PostProcessSettings.whiteBalanceTint;
                postConstants.lift = m_PostProcessSettings.lift;
                postConstants.pad0 = 0.0f;
                postConstants.gammaVal = m_PostProcessSettings.gammaVal;
                postConstants.pad1 = 0.0f;
                postConstants.gain = m_PostProcessSettings.gain;
                postConstants.pad2 = 0.0f;

                postConstants.fogColor = m_PostProcessSettings.fogColor;
                postConstants.fogBaseHeight = m_PostProcessSettings.fogBaseHeight;

                vkCmdPushConstants(cmd, m_PostProcessPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PostProcessPushConstants), &postConstants);

                // Draw fullscreen triangle
                vkCmdDraw(cmd, 3, 1, 0, 0);

                vkCmdEndRenderPass(cmd);
                RenderTarget* target = m_RenderTargetManager.Get(m_LDRColorHandles[frameIndex]);
                if (target) {
                    target->currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                }
            },
            true,
            postProcessFb,
            true,
            postProcessPipeline,
            { m_HDRColorHandles[frameIndex] },
            { m_LDRColorHandles[frameIndex] }
        );
    }

    // 9. Editor Overlay Pass
    renderGraph.RegisterPass(
        "EditorOverlayPass",
        {"LDRColor"},
        {"ViewportColor"},
        PassID::PostProcess,
        [this](VkCommandBuffer cmd) {
            // 1. Copy/Blit LDRColor to ViewportColor
            const RenderTarget* ldrTarget = m_RenderTargetManager.Get(m_LDRColorHandles[frameIndex]);
            VkImage srcImage = ldrTarget ? ldrTarget->image : VK_NULL_HANDLE;

            const RenderTarget* vpTarget = m_RenderTargetManager.Get(m_ViewportColorHandles[frameIndex]);
            VkImage dstImage = vpTarget ? vpTarget->image : VK_NULL_HANDLE;

            if (srcImage != VK_NULL_HANDLE && dstImage != VK_NULL_HANDLE) {
                VkImageMemoryBarrier srcBarrierStart{};
                srcBarrierStart.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                srcBarrierStart.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                srcBarrierStart.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                srcBarrierStart.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                srcBarrierStart.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                srcBarrierStart.image = srcImage;
                srcBarrierStart.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                srcBarrierStart.subresourceRange.baseMipLevel = 0;
                srcBarrierStart.subresourceRange.levelCount = 1;
                srcBarrierStart.subresourceRange.baseArrayLayer = 0;
                srcBarrierStart.subresourceRange.layerCount = 1;
                srcBarrierStart.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                srcBarrierStart.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

                VkImageMemoryBarrier dstBarrierStart{};
                dstBarrierStart.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                dstBarrierStart.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                dstBarrierStart.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                dstBarrierStart.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                dstBarrierStart.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                dstBarrierStart.image = dstImage;
                dstBarrierStart.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                dstBarrierStart.subresourceRange.baseMipLevel = 0;
                dstBarrierStart.subresourceRange.levelCount = 1;
                dstBarrierStart.subresourceRange.baseArrayLayer = 0;
                dstBarrierStart.subresourceRange.layerCount = 1;
                dstBarrierStart.srcAccessMask = 0;
                dstBarrierStart.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

                std::array<VkImageMemoryBarrier, 2> startBarriers = { srcBarrierStart, dstBarrierStart };
                vkCmdPipelineBarrier(cmd,
                                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     0,
                                     0, nullptr,
                                     0, nullptr,
                                     static_cast<uint32_t>(startBarriers.size()), startBarriers.data());

                VkImageCopy copyRegion{};
                copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.srcSubresource.mipLevel = 0;
                copyRegion.srcSubresource.baseArrayLayer = 0;
                copyRegion.srcSubresource.layerCount = 1;
                copyRegion.srcOffset = { 0, 0, 0 };
                copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.dstSubresource.mipLevel = 0;
                copyRegion.dstSubresource.baseArrayLayer = 0;
                copyRegion.dstSubresource.layerCount = 1;
                copyRegion.dstOffset = { 0, 0, 0 };
                copyRegion.extent = { m_DepthWidth, m_DepthHeight, 1 };

                vkCmdCopyImage(cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

                VkImageMemoryBarrier srcBarrierEnd = srcBarrierStart;
                srcBarrierEnd.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                srcBarrierEnd.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                srcBarrierEnd.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                srcBarrierEnd.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                VkImageMemoryBarrier dstBarrierEnd = dstBarrierStart;
                dstBarrierEnd.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                dstBarrierEnd.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                dstBarrierEnd.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                dstBarrierEnd.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                std::array<VkImageMemoryBarrier, 2> endBarriers = { srcBarrierEnd, dstBarrierEnd };
                vkCmdPipelineBarrier(cmd,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                     0,
                                     0, nullptr,
                                     0, nullptr,
                                     static_cast<uint32_t>(endBarriers.size()), endBarriers.data());
            }

            // 2. Execute EditorOverlayPass on ViewportColor
            VkFramebuffer fb = frameIndex < m_ViewportColorFramebuffers.size() ? m_ViewportColorFramebuffers[frameIndex] : VK_NULL_HANDLE;
            if (fb != VK_NULL_HANDLE && m_ViewportColorRenderPass != VK_NULL_HANDLE) {
                ViewportOverlaySettings overlaySettings{};
                overlaySettings.showGrid = false; // Disabled to prevent incompatible pipeline drawing in LDR pass (handled in TransparentPass)
                overlaySettings.showSelectionOutline = m_DebugConfig.enableEditorOverlay;

                m_EditorOverlayPass.Execute(
                    cmd,
                    resources,
                    frameIndex,
                    m_ViewportColorRenderPass,
                    fb,
                    { m_DepthWidth, m_DepthHeight },
                    gpuScene.GetDescriptorSet(frameIndex),
                    frameIndex < m_GBufferDescriptorSets.size() ? m_GBufferDescriptorSets[frameIndex] : VK_NULL_HANDLE,
                    m_GridPipeline,
                    m_GridPipelineLayout,
                    m_SelectionOutlinePass,
                    m_SelectedEntityID,
                    overlaySettings
                );
            }
            RenderTarget* ldrTargetTracked = m_RenderTargetManager.Get(m_LDRColorHandles[frameIndex]);
            if (ldrTargetTracked) {
                ldrTargetTracked->currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }
            RenderTarget* vpTargetTracked = m_RenderTargetManager.Get(m_ViewportColorHandles[frameIndex]);
            if (vpTargetTracked) {
                vpTargetTracked->currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }
        },
        false,
        VK_NULL_HANDLE,
        false,
        VK_NULL_HANDLE,
        { m_LDRColorHandles[frameIndex] },
        { m_ViewportColorHandles[frameIndex] }
    );

    // 10. UI Pass (records ImGui UI overlay)
    renderGraph.RegisterPass(
        "UIPass",
        {"ViewportColor"},
        {"Swapchain"},
        PassID::UI,
        [this](VkCommandBuffer cmd) {
            if (m_DebugConfig.showGBufferAlbedo && m_ViewportRenderer.isOffscreenRenderingEnabled()) {
                VkImage srcImage = m_GBufferAImages[frameIndex];
                VkImage dstImage = m_ViewportRenderer.getOffscreenImage(frameIndex);

                if (srcImage != VK_NULL_HANDLE && dstImage != VK_NULL_HANDLE) {
                    VkImageMemoryBarrier srcBarrierStart{};
                    srcBarrierStart.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    srcBarrierStart.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    srcBarrierStart.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                    srcBarrierStart.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    srcBarrierStart.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    srcBarrierStart.image = srcImage;
                    srcBarrierStart.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    srcBarrierStart.subresourceRange.baseMipLevel = 0;
                    srcBarrierStart.subresourceRange.levelCount = 1;
                    srcBarrierStart.subresourceRange.baseArrayLayer = 0;
                    srcBarrierStart.subresourceRange.layerCount = 1;
                    srcBarrierStart.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    srcBarrierStart.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

                    VkImageMemoryBarrier dstBarrierStart{};
                    dstBarrierStart.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    dstBarrierStart.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    dstBarrierStart.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                    dstBarrierStart.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    dstBarrierStart.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    dstBarrierStart.image = dstImage;
                    dstBarrierStart.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    dstBarrierStart.subresourceRange.baseMipLevel = 0;
                    dstBarrierStart.subresourceRange.levelCount = 1;
                    dstBarrierStart.subresourceRange.baseArrayLayer = 0;
                    dstBarrierStart.subresourceRange.layerCount = 1;
                    dstBarrierStart.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    dstBarrierStart.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

                    std::array<VkImageMemoryBarrier, 2> startBarriers = { srcBarrierStart, dstBarrierStart };
                    vkCmdPipelineBarrier(cmd,
                                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                                         0,
                                         0, nullptr,
                                         0, nullptr,
                                         static_cast<uint32_t>(startBarriers.size()), startBarriers.data());

                    VkImageBlit blit{};
                    blit.srcOffsets[0] = { 0, 0, 0 };
                    blit.srcOffsets[1] = { static_cast<int32_t>(m_DepthWidth), static_cast<int32_t>(m_DepthHeight), 1 };
                    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    blit.srcSubresource.mipLevel = 0;
                    blit.srcSubresource.baseArrayLayer = 0;
                    blit.srcSubresource.layerCount = 1;

                    blit.dstOffsets[0] = { 0, 0, 0 };
                    blit.dstOffsets[1] = { static_cast<int32_t>(m_ViewportRenderer.getOffscreenWidth()), static_cast<int32_t>(m_ViewportRenderer.getOffscreenHeight()), 1 };
                    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    blit.dstSubresource.mipLevel = 0;
                    blit.dstSubresource.baseArrayLayer = 0;
                    blit.dstSubresource.layerCount = 1;

                    vkCmdBlitImage(cmd,
                                   srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   1, &blit,
                                   VK_FILTER_LINEAR);

                    VkImageMemoryBarrier srcBarrierEnd = srcBarrierStart;
                    srcBarrierEnd.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                    srcBarrierEnd.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    srcBarrierEnd.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                    srcBarrierEnd.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                    VkImageMemoryBarrier dstBarrierEnd = dstBarrierStart;
                    dstBarrierEnd.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                    dstBarrierEnd.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    dstBarrierEnd.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                    dstBarrierEnd.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                    std::array<VkImageMemoryBarrier, 2> endBarriers = { srcBarrierEnd, dstBarrierEnd };
                    vkCmdPipelineBarrier(cmd,
                                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                         0,
                                         0, nullptr,
                                         0, nullptr,
                                         static_cast<uint32_t>(endBarriers.size()), endBarriers.data());
                }
            }

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
        },
        false,
        VK_NULL_HANDLE,
        false,
        VK_NULL_HANDLE,
        { m_ViewportColorHandles[frameIndex] },
        { m_LDRColorHandles[frameIndex] } // Swapchain target abstraction (we reuse LDRColor index as proxy)
    );

    // 11. Present Pass (performs swapchain image layout transitions)
    renderGraph.RegisterPass(
        "PresentPass",
        {"Swapchain"},
        {"Backbuffer"},
        PassID::UI,
        [this](VkCommandBuffer cmd) {
            // No transition needed here; UIPass transitions the swapchain image to PRESENT_SRC_KHR beforehand,
            // and ImGui's renderpass transitions it to COLOR_ATTACHMENT_OPTIMAL and back to PRESENT_SRC_KHR on exit.
        },
        false,
        VK_NULL_HANDLE,
        false,
        VK_NULL_HANDLE,
        { m_LDRColorHandles[frameIndex] },
        { m_LDRColorHandles[frameIndex] }
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
        if (m_DebugConfig.forceDefaultMaterial && m_DefaultMaterial) {
            item.material = m_DefaultMaterial;
        }
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
            item.layerMask = instance.layerMask;

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
                            
                            std::string albedoPath = ResolveTexturePath(omnixMat.albedoTexturePath);
                            std::string normalPath = ResolveTexturePath(omnixMat.normalTexturePath);
                            std::string metallicRoughnessPath = ResolveTexturePath(omnixMat.metallicRoughnessTexturePath);
                            std::string aoPath = ResolveTexturePath(omnixMat.aoTexturePath);
                            std::string emissivePath = ResolveTexturePath(omnixMat.emissiveTexturePath);

                            if (albedoPath.empty()) {
                                albedoPath = "Assets/Textures/Texturelabs_Concrete_129S.png";
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

            if (m_DebugConfig.forceDefaultMaterial && m_DefaultMaterial) {
                item.material = m_DefaultMaterial;
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
                              "Assets/Textures/Texturelabs_Concrete_129S.png", 
                              "", 
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

static float Halton(uint32_t index, uint32_t base) {
    float result = 0.0f;
    float f = 1.0f;
    while (index > 0) {
        f = f / static_cast<float>(base);
        result += f * static_cast<float>(index % base);
        index = index / base;
    }
    return result;
}

void Renderer::UpdateRadianceFrameUBO(
    Omnix::Radiance::RadianceFrameUBO& ubo,
    const CameraData& camera,
    uint32_t viewportWidth,
    uint32_t viewportHeight,
    float timeSeconds)
{
    const auto& settings = m_RadianceSettings;

    ubo.view = camera.viewMatrix;
    float aspect = viewportHeight > 0 ? static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight) : 16.0f / 9.0f;
    ubo.projection = glm::perspective(glm::radians(camera.fov), aspect, camera.nearPlane, camera.farPlane);
    ubo.projection[1][1] *= -1.0f;

    // Apply TAA camera jitter
    if (m_TAAPipeline != VK_NULL_HANDLE) {
        m_JitterIndex = (m_JitterIndex + 1) % 16;
        m_CurrentJitter.x = Halton(m_JitterIndex + 1, 2) - 0.5f;
        m_CurrentJitter.y = Halton(m_JitterIndex + 1, 3) - 0.5f;

        float jitterX = m_CurrentJitter.x / static_cast<float>(viewportWidth);
        float jitterY = m_CurrentJitter.y / static_cast<float>(viewportHeight);

        ubo.projection[2][0] += jitterX * 2.0f;
        ubo.projection[2][1] += jitterY * 2.0f;
    } else {
        m_CurrentJitter = glm::vec2(0.0f);
    }

    m_CurrentViewProjection = ubo.projection * ubo.view;

    ubo.inverseView = glm::inverse(ubo.view);
    ubo.inverseProjection = glm::inverse(ubo.projection);
    ubo.inverseViewProjection = glm::inverse(ubo.projection * ubo.view);

    ubo.cameraPosition = glm::vec4(camera.position, timeSeconds);

    ubo.viewportSize = glm::vec4(
        static_cast<float>(viewportWidth),
        static_cast<float>(viewportHeight),
        1.0f / static_cast<float>(viewportWidth),
        1.0f / static_cast<float>(viewportHeight)
    );

    ubo.skyTopColorIntensity = glm::vec4(
        settings.sky.skyTopColor,
        settings.sky.skyIntensity
    );

    ubo.skyHorizonColorBlend = glm::vec4(
        settings.sky.horizonColor,
        settings.sky.horizonBlend
    );

    ubo.skyGroundColorIntensity = glm::vec4(
        settings.sky.groundColor,
        settings.sky.groundIntensity
    );

    ubo.sunDirectionIntensity = glm::vec4(
        glm::normalize(settings.sun.direction),
        settings.sun.intensity
    );

    ubo.sunColorAngularSize = glm::vec4(
        settings.sun.color,
        settings.sun.angularSize
    );

    ubo.exposureSettings = glm::vec4(
        settings.exposure.manualExposure,
        settings.exposure.minExposure,
        settings.exposure.maxExposure,
        settings.exposure.autoExposure ? 1.0f : 0.0f
    );

    ubo.renderFlags = glm::uvec4(
        static_cast<uint32_t>(settings.exposure.toneMappingMode),
        settings.cinematicPreview ? 1u : 0u,
        camera.selectedEntityID,
        settings.forceReferenceLighting ? 1u : 0u
    );
}

void Renderer::updateLightingUBO()
{
    LightData uboData{};
    uboData.shadingMode = m_ShadingMode;

    if (!activeRenderScene.directionalLights.empty()) {
        const auto& dirLight = activeRenderScene.directionalLights[0];
        uboData.directionalDirectionIntensity = glm::vec4(dirLight.direction, dirLight.intensity);
        uboData.directionalColor = glm::vec4(dirLight.color, 1.0f);
        uboData.directionalLightProjView = dirLight.lightSpaceMatrix;
        uboData.shadowBias = dirLight.shadowBias;
        uboData.shadowSlopeBias = dirLight.shadowSlopeBias;
        uboData.shadowNormalBias = dirLight.shadowNormalBias;
        uboData.shadowStrength = dirLight.shadowStrength;
        uboData.shadowLightCast = dirLight.castShadows > 0.0f ? 1 : 0;
        uboData.pcfKernelSize = dirLight.pcfKernelSize;
        uboData.shadowResolution = dirLight.shadowResolution;

        // ShadowGPUSettings
        uboData.shadowSettings.shadowParams.x = dirLight.shadowStrength;
        uboData.shadowSettings.shadowParams.y = dirLight.shadowBias;
        uboData.shadowSettings.shadowParams.z = dirLight.shadowSlopeBias;
        uboData.shadowSettings.shadowParams.w = 1.0f; // softness default to 1.0f

        uboData.shadowSettings.shadowFlags.x = static_cast<uint32_t>(dirLight.pcfKernelSize);
        uboData.shadowSettings.shadowFlags.y = 0;
        uboData.shadowSettings.shadowFlags.z = 0;
        uboData.shadowSettings.shadowFlags.w = 0;
    } else {
        uboData.directionalDirectionIntensity = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f);
        uboData.directionalColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        uboData.directionalLightProjView = glm::mat4(1.0f);
        uboData.shadowBias = 0.003f;
        uboData.shadowNormalBias = 0.0f;
        uboData.shadowSlopeBias = 0.01f;
        uboData.shadowStrength = 1.0f;
        uboData.shadowLightCast = 0;
        uboData.pcfKernelSize = 3;
        uboData.shadowResolution = 2048;

        // ShadowGPUSettings fallback
        uboData.shadowSettings.shadowParams = glm::vec4(1.0f, 0.003f, 0.01f, 1.0f);
        uboData.shadowSettings.shadowFlags = glm::uvec4(3, 0, 0, 0);
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

void Renderer::updateSSRDescriptorSet(uint32_t frameIndex)
{
    if (resources.device == VK_NULL_HANDLE || m_SSRDescriptorSets.empty() || frameIndex >= m_SSRDescriptorSets.size()) return;

    auto fallback2D = Texture::getWhiteTexture(resources);
    VkImageView fallbackView = fallback2D ? fallback2D->view() : VK_NULL_HANDLE;

    // Update descriptor set for SSR
    VkDescriptorImageInfo gbufferAInfo{};
    gbufferAInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    gbufferAInfo.imageView = (frameIndex < m_GBufferAImageViews.size() && m_GBufferAImageViews[frameIndex] != VK_NULL_HANDLE) ? m_GBufferAImageViews[frameIndex] : fallbackView;
    gbufferAInfo.sampler = m_GBufferSampler;

    VkDescriptorImageInfo gbufferBInfo{};
    gbufferBInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    gbufferBInfo.imageView = (frameIndex < m_GBufferBImageViews.size() && m_GBufferBImageViews[frameIndex] != VK_NULL_HANDLE) ? m_GBufferBImageViews[frameIndex] : fallbackView;
    gbufferBInfo.sampler = m_GBufferSampler;

    VkDescriptorImageInfo gbufferCInfo{};
    gbufferCInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    gbufferCInfo.imageView = (frameIndex < m_GBufferCImageViews.size() && m_GBufferCImageViews[frameIndex] != VK_NULL_HANDLE) ? m_GBufferCImageViews[frameIndex] : fallbackView;
    gbufferCInfo.sampler = m_GBufferSampler;

    VkDescriptorImageInfo gbufferDInfo{};
    gbufferDInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    gbufferDInfo.imageView = (frameIndex < m_GBufferDImageViews.size() && m_GBufferDImageViews[frameIndex] != VK_NULL_HANDLE) ? m_GBufferDImageViews[frameIndex] : fallbackView;
    gbufferDInfo.sampler = m_GBufferSampler;

    VkDescriptorImageInfo depthInfo{};
    depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    VkImageView depthView = m_ViewportRenderer.isOffscreenRenderingEnabled() ?
        m_ViewportRenderer.getOffscreenDepthImageView(frameIndex) : (frameIndex < m_DepthImageViews.size() ? m_DepthImageViews[frameIndex] : VK_NULL_HANDLE);
    depthInfo.imageView = depthView != VK_NULL_HANDLE ? depthView : fallbackView;
    depthInfo.sampler = m_GBufferSampler;

    VkDescriptorImageInfo hdrColorInfo{};
    hdrColorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    hdrColorInfo.imageView = (frameIndex < m_HDRColorImageViews.size() && m_HDRColorImageViews[frameIndex] != VK_NULL_HANDLE) ? m_HDRColorImageViews[frameIndex] : fallbackView;
    hdrColorInfo.sampler = m_GBufferSampler;

    VkDescriptorImageInfo hzbInfo{};
    hzbInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkImageView hzbSRV = m_HZBPass.GetHZBSRV(frameIndex);
    hzbInfo.imageView = hzbSRV != VK_NULL_HANDLE ? hzbSRV : fallbackView;
    hzbInfo.sampler = m_HZBPass.GetSampler() ? m_HZBPass.GetSampler() : m_GBufferSampler;

    VkDescriptorImageInfo outputInfo{};
    outputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    outputInfo.imageView = (frameIndex < m_HDRColorComposedImageViews.size() && m_HDRColorComposedImageViews[frameIndex] != VK_NULL_HANDLE) ? m_HDRColorComposedImageViews[frameIndex] : fallbackView;

    VkDescriptorBufferInfo uboInfo{};
    uboInfo.buffer = gpuScene.GetFrameResources(frameIndex).lightBuffer;
    uboInfo.offset = 0;
    uboInfo.range = sizeof(LightData);

    // localReflectionProbes[4] (from Registry / fallback)
    VkDescriptorImageInfo localProbesInfo[4]{};
    auto fallbackCubemap = Texture::getWhiteCubemap(resources);
    for (uint32_t p = 0; p < 4; ++p) {
        localProbesInfo[p].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        localProbesInfo[p].sampler = m_GBufferSampler;
        localProbesInfo[p].imageView = fallbackCubemap->view();
    }
    const auto& activeProbes = activeRenderScene.reflectionProbes;
    for (size_t p = 0; p < std::min(activeProbes.size(), size_t(4)); ++p) {
        Texture* tex = ReflectionProbeRegistry::Get().GetTexture(activeProbes[p].capturePath);
        if (tex != nullptr) {
            localProbesInfo[p].imageView = tex->view();
        }
    }

    // prefilterMap and brdfLUT
    EnvironmentMap* activeEnv = nullptr;
    if (activeRenderScene.skyLight.mode == 1 && !activeRenderScene.skyLight.environmentPath.empty()) {
        activeEnv = EnvironmentSystem::Get().GetOrCreateEnvironment(activeRenderScene.skyLight.environmentPath, resources);
    }

    VkDescriptorImageInfo prefilterInfo{};
    prefilterInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (activeEnv && activeEnv->prefilterCube) {
        prefilterInfo.imageView = activeEnv->prefilterCube->view();
        prefilterInfo.sampler = activeEnv->prefilterCube->sampler();
    } else {
        prefilterInfo.imageView = fallbackCubemap->view();
        prefilterInfo.sampler = fallbackCubemap->sampler();
    }

    auto brdfLutTex = EnvironmentSystem::Get().GetBRDFLUT(resources);
    VkDescriptorImageInfo brdfInfo{};
    brdfInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (brdfLutTex) {
        brdfInfo.imageView = brdfLutTex->view();
        brdfInfo.sampler = brdfLutTex->sampler();
    } else {
        auto fallback2D = Texture::getWhiteTexture(resources);
        brdfInfo.imageView = fallback2D->view();
        brdfInfo.sampler = fallback2D->sampler();
    }

    // Build writes
    std::array<VkWriteDescriptorSet, 13> writes{};
    
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_SSRDescriptorSets[frameIndex];
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo = &gbufferAInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_SSRDescriptorSets[frameIndex];
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo = &gbufferBInfo;

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = m_SSRDescriptorSets[frameIndex];
    writes[2].dstBinding = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[2].pImageInfo = &gbufferCInfo;

    writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet = m_SSRDescriptorSets[frameIndex];
    writes[3].dstBinding = 3;
    writes[3].descriptorCount = 1;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[3].pImageInfo = &gbufferDInfo;

    writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[4].dstSet = m_SSRDescriptorSets[frameIndex];
    writes[4].dstBinding = 4;
    writes[4].descriptorCount = 1;
    writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[4].pImageInfo = &depthInfo;

    writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[5].dstSet = m_SSRDescriptorSets[frameIndex];
    writes[5].dstBinding = 5;
    writes[5].descriptorCount = 1;
    writes[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[5].pImageInfo = &hdrColorInfo;

    writes[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[6].dstSet = m_SSRDescriptorSets[frameIndex];
    writes[6].dstBinding = 6;
    writes[6].descriptorCount = 1;
    writes[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[6].pImageInfo = &hzbInfo;

    writes[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[7].dstSet = m_SSRDescriptorSets[frameIndex];
    writes[7].dstBinding = 7;
    writes[7].descriptorCount = 1;
    writes[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[7].pImageInfo = &outputInfo;

    writes[8].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[8].dstSet = m_SSRDescriptorSets[frameIndex];
    writes[8].dstBinding = 8;
    writes[8].descriptorCount = 1;
    writes[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[8].pBufferInfo = &uboInfo;

    writes[9].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[9].dstSet = m_SSRDescriptorSets[frameIndex];
    writes[9].dstBinding = 9;
    writes[9].descriptorCount = 4;
    writes[9].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[9].pImageInfo = localProbesInfo;

    writes[10].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[10].dstSet = m_SSRDescriptorSets[frameIndex];
    writes[10].dstBinding = 10;
    writes[10].descriptorCount = 1;
    writes[10].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[10].pImageInfo = &prefilterInfo;

    writes[11].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[11].dstSet = m_SSRDescriptorSets[frameIndex];
    writes[11].dstBinding = 11;
    writes[11].descriptorCount = 1;
    writes[11].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[11].pImageInfo = &brdfInfo;

    VkDescriptorBufferInfo cameraInfo{};
    cameraInfo.buffer = gpuScene.GetFrameResources(frameIndex).cameraBuffer;
    cameraInfo.offset = 0;
    cameraInfo.range = sizeof(Omnix::Radiance::RadianceFrameUBO);

    writes[12].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[12].dstSet = m_SSRDescriptorSets[frameIndex];
    writes[12].dstBinding = 12;
    writes[12].descriptorCount = 1;
    writes[12].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[12].pBufferInfo = &cameraInfo;

    vkUpdateDescriptorSets(resources.device, 13, writes.data(), 0, nullptr);
}

void Renderer::updateGBufferDescriptorSets()
{
    if (resources.device == VK_NULL_HANDLE) return;

    // Fallback textures used for optional resources (SSAO, ObjectID, Velocity) when disabled.
    auto fallback2D   = Texture::getWhiteTexture(resources);
    VkImageView  fbView    = fallback2D ? fallback2D->view()    : VK_NULL_HANDLE;
    VkSampler    fbSampler = fallback2D ? fallback2D->sampler() : VK_NULL_HANDLE;

    uint32_t maxFrames = resources.MAX_FRAMES_IN_FLIGHT;
    for (uint32_t i = 0; i < maxFrames; ++i) {
        // Mandatory: core GBuffer textures and depth must exist.
        if (m_GBufferAImageViews[i] == VK_NULL_HANDLE || m_GBufferBImageViews[i] == VK_NULL_HANDLE ||
            m_GBufferCImageViews[i] == VK_NULL_HANDLE || m_GBufferDImageViews[i] == VK_NULL_HANDLE ||
            m_DepthImageViews[i] == VK_NULL_HANDLE) {
            continue;
        }

        // Optional SSAO views - use white 1x1 fallback when SSAO is disabled.
        auto ssaoTarget = m_RenderTargetManager.Get(m_SSAOHandles[i]);
        VkImageView ssaoView = (m_DebugConfig.enableSSAO && ssaoTarget && ssaoTarget->view != VK_NULL_HANDLE) ? ssaoTarget->view : fbView;
        auto ssaoBlurTarget  = m_RenderTargetManager.Get(m_SSAOBlurredHandles[i]);
        VkImageView ssaoBlurView = (m_DebugConfig.enableSSAO && ssaoBlurTarget && ssaoBlurTarget->view != VK_NULL_HANDLE) ? ssaoBlurTarget->view : fbView;
        if (ssaoView == VK_NULL_HANDLE || ssaoBlurView == VK_NULL_HANDLE) continue;

        // Optional ObjectID / GBufferVelocity - use fallback when not allocated.
        VkImageView objIdView  = (m_ObjectIDImageViews[i]        != VK_NULL_HANDLE) ? m_ObjectIDImageViews[i]        : fbView;
        VkImageView velView    = (m_GBufferVelocityImageViews[i] != VK_NULL_HANDLE) ? m_GBufferVelocityImageViews[i] : fbView;

        // Optional shadow cascade views - use depth sampler fallback when shadow pass is disabled.
        auto getShadowView = [&](uint32_t cascade) -> VkImageView {
            return (m_ShadowImageViewsCascades[cascade][i] != VK_NULL_HANDLE)
                   ? m_ShadowImageViewsCascades[cascade][i] : fbView;
        };

        std::array<VkWriteDescriptorSet, 17> writes{};

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

        VkDescriptorImageInfo imageInfoShadow0{};
        imageInfoShadow0.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        imageInfoShadow0.imageView = getShadowView(0);
        imageInfoShadow0.sampler = (m_ShadowSampler != VK_NULL_HANDLE) ? m_ShadowSampler : m_GBufferSampler;

        writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[5].dstSet = m_GBufferDescriptorSets[i];
        writes[5].dstBinding = 5;
        writes[5].dstArrayElement = 0;
        writes[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[5].descriptorCount = 1;
        writes[5].pImageInfo = &imageInfoShadow0;

        VkDescriptorImageInfo imageInfoSSAO{};
        imageInfoSSAO.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfoSSAO.imageView = ssaoBlurView;
        imageInfoSSAO.sampler = m_GBufferSampler;

        writes[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[6].dstSet = m_GBufferDescriptorSets[i];
        writes[6].dstBinding = 6;
        writes[6].dstArrayElement = 0;
        writes[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[6].descriptorCount = 1;
        writes[6].pImageInfo = &imageInfoSSAO;

        VkDescriptorImageInfo imageInfoObjectID{};
        imageInfoObjectID.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfoObjectID.imageView = objIdView;  // fallback when ObjectID pass disabled
        imageInfoObjectID.sampler = m_GBufferSampler;

        VkDescriptorImageInfo imageInfoVelocity{};
        imageInfoVelocity.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfoVelocity.imageView = velView;    // fallback when velocity pass disabled
        imageInfoVelocity.sampler = m_GBufferSampler;

        VkSampler shadowSampler = (m_ShadowSampler != VK_NULL_HANDLE) ? m_ShadowSampler : m_GBufferSampler;

        VkDescriptorImageInfo imageInfoShadow1{};
        imageInfoShadow1.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        imageInfoShadow1.imageView = getShadowView(1);
        imageInfoShadow1.sampler = shadowSampler;

        writes[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[7].dstSet = m_GBufferDescriptorSets[i];
        writes[7].dstBinding = 7;
        writes[7].dstArrayElement = 0;
        writes[7].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[7].descriptorCount = 1;
        writes[7].pImageInfo = &imageInfoShadow1;

        VkDescriptorImageInfo imageInfoShadow2{};
        imageInfoShadow2.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        imageInfoShadow2.imageView = getShadowView(2);
        imageInfoShadow2.sampler = shadowSampler;

        writes[8].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[8].dstSet = m_GBufferDescriptorSets[i];
        writes[8].dstBinding = 8;
        writes[8].dstArrayElement = 0;
        writes[8].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[8].descriptorCount = 1;
        writes[8].pImageInfo = &imageInfoShadow2;

        VkDescriptorImageInfo imageInfoShadow3{};
        imageInfoShadow3.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        imageInfoShadow3.imageView = getShadowView(3);
        imageInfoShadow3.sampler = shadowSampler;

        writes[9].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[9].dstSet = m_GBufferDescriptorSets[i];
        writes[9].dstBinding = 9;
        writes[9].dstArrayElement = 0;
        writes[9].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[9].descriptorCount = 1;
        writes[9].pImageInfo = &imageInfoShadow3;

        writes[10].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[10].dstSet = m_GBufferDescriptorSets[i];
        writes[10].dstBinding = 10;
        writes[10].dstArrayElement = 0;
        writes[10].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[10].descriptorCount = 1;
        writes[10].pImageInfo = &imageInfoObjectID;

        writes[11].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[11].dstSet = m_GBufferDescriptorSets[i];
        writes[11].dstBinding = 11;
        writes[11].dstArrayElement = 0;
        writes[11].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[11].descriptorCount = 1;
        writes[11].pImageInfo = &imageInfoVelocity;

        EnvironmentMap* activeEnv = nullptr;
        if (activeRenderScene.skyLight.mode == 1 && !activeRenderScene.skyLight.environmentPath.empty()) {
            activeEnv = EnvironmentSystem::Get().GetOrCreateEnvironment(activeRenderScene.skyLight.environmentPath, resources);
        }

        VkDescriptorImageInfo imageInfoIrradiance{};
        imageInfoIrradiance.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        if (activeEnv && activeEnv->irradianceCube) {
            imageInfoIrradiance.imageView = activeEnv->irradianceCube->view();
            imageInfoIrradiance.sampler = activeEnv->irradianceCube->sampler();
        } else {
            auto fallback = Texture::getWhiteCubemap(resources);
            imageInfoIrradiance.imageView = fallback->view();
            imageInfoIrradiance.sampler = fallback->sampler();
        }

        writes[12].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[12].dstSet = m_GBufferDescriptorSets[i];
        writes[12].dstBinding = 12;
        writes[12].dstArrayElement = 0;
        writes[12].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[12].descriptorCount = 1;
        writes[12].pImageInfo = &imageInfoIrradiance;

        VkDescriptorImageInfo imageInfoPrefilter{};
        imageInfoPrefilter.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        if (activeEnv && activeEnv->prefilterCube) {
            imageInfoPrefilter.imageView = activeEnv->prefilterCube->view();
            imageInfoPrefilter.sampler = activeEnv->prefilterCube->sampler();
        } else {
            auto fallback = Texture::getWhiteCubemap(resources);
            imageInfoPrefilter.imageView = fallback->view();
            imageInfoPrefilter.sampler = fallback->sampler();
        }

        writes[13].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[13].dstSet = m_GBufferDescriptorSets[i];
        writes[13].dstBinding = 13;
        writes[13].dstArrayElement = 0;
        writes[13].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[13].descriptorCount = 1;
        writes[13].pImageInfo = &imageInfoPrefilter;

        VkDescriptorImageInfo imageInfoBRDF{};
        imageInfoBRDF.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        auto brdfLutTex = EnvironmentSystem::Get().GetBRDFLUT(resources);
        imageInfoBRDF.imageView = brdfLutTex->view();
        imageInfoBRDF.sampler = brdfLutTex->sampler();

        writes[14].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[14].dstSet = m_GBufferDescriptorSets[i];
        writes[14].dstBinding = 14;
        writes[14].dstArrayElement = 0;
        writes[14].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[14].descriptorCount = 1;
        writes[14].pImageInfo = &imageInfoBRDF;

        VkDescriptorImageInfo imageInfoAtlas{};
        imageInfoAtlas.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        imageInfoAtlas.imageView = m_ShadowAtlasImageViews[i];
        imageInfoAtlas.sampler = m_ShadowSampler;

        writes[15].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[15].dstSet = m_GBufferDescriptorSets[i];
        writes[15].dstBinding = 15;
        writes[15].dstArrayElement = 0;
        writes[15].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[15].descriptorCount = 1;
        writes[15].pImageInfo = &imageInfoAtlas;

        VkDescriptorImageInfo probeInfos[4]{};
        for (uint32_t slot = 0; slot < 4; ++slot) {
            probeInfos[slot].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            Texture* bakedTex = nullptr;
            if (slot < activeRenderScene.reflectionProbes.size()) {
                const auto& probe = activeRenderScene.reflectionProbes[slot];
                bakedTex = ReflectionProbeRegistry::Get().GetTexture(probe.capturePath);
            }

            if (bakedTex && bakedTex->view() != VK_NULL_HANDLE) {
                probeInfos[slot].imageView = bakedTex->view();
                probeInfos[slot].sampler = bakedTex->sampler();
            } else {
                auto fallback = Texture::getWhiteCubemap(resources);
                probeInfos[slot].imageView = fallback->view();
                probeInfos[slot].sampler = fallback->sampler();
            }
        }

        writes[16].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[16].dstSet = m_GBufferDescriptorSets[i];
        writes[16].dstBinding = 16;
        writes[16].dstArrayElement = 0;
        writes[16].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[16].descriptorCount = 4;
        writes[16].pImageInfo = probeInfos;

        vkUpdateDescriptorSets(resources.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

        // Update raw SSAO descriptors
        VkDescriptorImageInfo depthInfo{};
        depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        depthInfo.imageView = m_ViewportRenderer.isOffscreenRenderingEnabled() ?
            m_ViewportRenderer.getOffscreenDepthImageView(i) : m_DepthImageViews[i];
        depthInfo.sampler = m_GBufferSampler;

        VkDescriptorImageInfo normalInfo{};
        normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        normalInfo.imageView = m_GBufferBImageViews[i];
        normalInfo.sampler = m_GBufferSampler;

        VkDescriptorImageInfo noiseInfo{};
        noiseInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        noiseInfo.imageView = m_SSAONoiseImageView;
        noiseInfo.sampler = m_SSAONoiseSampler;

        VkDescriptorBufferInfo ssaoConstInfo{};
        ssaoConstInfo.buffer = m_SSAOConstantBuffers[i];
        ssaoConstInfo.offset = 0;
        ssaoConstInfo.range = 1120;

        std::array<VkWriteDescriptorSet, 4> ssaoWrites{};
        ssaoWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        ssaoWrites[0].dstSet = m_SSAODescriptorSets[i];
        ssaoWrites[0].dstBinding = 0;
        ssaoWrites[0].dstArrayElement = 0;
        ssaoWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        ssaoWrites[0].descriptorCount = 1;
        ssaoWrites[0].pImageInfo = &depthInfo;

        ssaoWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        ssaoWrites[1].dstSet = m_SSAODescriptorSets[i];
        ssaoWrites[1].dstBinding = 1;
        ssaoWrites[1].dstArrayElement = 0;
        ssaoWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        ssaoWrites[1].descriptorCount = 1;
        ssaoWrites[1].pImageInfo = &normalInfo;

        ssaoWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        ssaoWrites[2].dstSet = m_SSAODescriptorSets[i];
        ssaoWrites[2].dstBinding = 2;
        ssaoWrites[2].dstArrayElement = 0;
        ssaoWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        ssaoWrites[2].descriptorCount = 1;
        ssaoWrites[2].pImageInfo = &noiseInfo;

        ssaoWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        ssaoWrites[3].dstSet = m_SSAODescriptorSets[i];
        ssaoWrites[3].dstBinding = 3;
        ssaoWrites[3].dstArrayElement = 0;
        ssaoWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ssaoWrites[3].descriptorCount = 1;
        ssaoWrites[3].pBufferInfo = &ssaoConstInfo;

        vkUpdateDescriptorSets(resources.device, static_cast<uint32_t>(ssaoWrites.size()), ssaoWrites.data(), 0, nullptr);

        // Update SSAO Blur descriptors
        VkDescriptorImageInfo rawSsaoInfo{};
        rawSsaoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        rawSsaoInfo.imageView = ssaoView;
        rawSsaoInfo.sampler = m_GBufferSampler;
        std::array<VkWriteDescriptorSet, 3> blurWrites{};
        blurWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        blurWrites[0].dstSet = m_SSAOBlurDescriptorSets[i];
        blurWrites[0].dstBinding = 0;
        blurWrites[0].dstArrayElement = 0;
        blurWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        blurWrites[0].descriptorCount = 1;
        blurWrites[0].pImageInfo = &rawSsaoInfo;

        blurWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        blurWrites[1].dstSet = m_SSAOBlurDescriptorSets[i];
        blurWrites[1].dstBinding = 1;
        blurWrites[1].dstArrayElement = 0;
        blurWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        blurWrites[1].descriptorCount = 1;
        blurWrites[1].pImageInfo = &depthInfo;

        blurWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        blurWrites[2].dstSet = m_SSAOBlurDescriptorSets[i];
        blurWrites[2].dstBinding = 2;
        blurWrites[2].dstArrayElement = 0;
        blurWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        blurWrites[2].descriptorCount = 1;
        blurWrites[2].pImageInfo = &normalInfo;

        vkUpdateDescriptorSets(resources.device, static_cast<uint32_t>(blurWrites.size()), blurWrites.data(), 0, nullptr);
    }
}
void Renderer::recreateDepthResources(uint32_t width, uint32_t height)
{
    if (resources.device == VK_NULL_HANDLE) return;

    // Destroy existing managed render targets
    for (auto h : m_DepthHandles) m_RenderTargetManager.Destroy(h);
    m_DepthHandles.clear();

    for (auto h : m_GBufferAHandles) m_RenderTargetManager.Destroy(h);
    m_GBufferAHandles.clear();

    for (auto h : m_GBufferBHandles) m_RenderTargetManager.Destroy(h);
    m_GBufferBHandles.clear();

    for (auto h : m_GBufferCHandles) m_RenderTargetManager.Destroy(h);
    m_GBufferCHandles.clear();

    for (auto h : m_GBufferDHandles) m_RenderTargetManager.Destroy(h);
    m_GBufferDHandles.clear();

    for (auto h : m_ObjectIDHandles) m_RenderTargetManager.Destroy(h);
    m_ObjectIDHandles.clear();

    for (auto h : m_GBufferVelocityHandles) m_RenderTargetManager.Destroy(h);
    m_GBufferVelocityHandles.clear();

    for (auto h : m_HDRColorHandles) m_RenderTargetManager.Destroy(h);
    m_HDRColorHandles.clear();

    for (auto h : m_HDRColorComposedHandles) m_RenderTargetManager.Destroy(h);
    m_HDRColorComposedHandles.clear();

    for (auto h : m_LDRColorHandles) m_RenderTargetManager.Destroy(h);
    m_LDRColorHandles.clear();

    for (auto h : m_ViewportColorHandles) m_RenderTargetManager.Destroy(h);
    m_ViewportColorHandles.clear();

    for (auto h : m_SSAOHandles) m_RenderTargetManager.Destroy(h);
    m_SSAOHandles.clear();

    for (auto h : m_SSAOBlurredHandles) m_RenderTargetManager.Destroy(h);
    m_SSAOBlurredHandles.clear();
    m_SSAOBlurredImages.clear();
    m_SSAOBlurredAllocations.clear();
    m_SSAOBlurredImageViews.clear();

    for (auto h : m_SSAOFbHandles) m_FramebufferManager.Destroy(h);
    m_SSAOFbHandles.clear();
    m_SSAOFramebuffers.clear();

    for (auto h : m_SSAOBlurredFbHandles) m_FramebufferManager.Destroy(h);
    m_SSAOBlurredFbHandles.clear();
    m_SSAOBlurredFramebuffers.clear();

    // Destroy existing managed framebuffers
    for (auto h : m_DepthFbHandles) m_FramebufferManager.Destroy(h);
    m_DepthFbHandles.clear();
    m_DepthFramebuffers.clear();

    for (auto h : m_OffscreenDepthFbHandles) m_FramebufferManager.Destroy(h);
    m_OffscreenDepthFbHandles.clear();
    m_OffscreenDepthFramebuffers.clear();

    for (auto h : m_GeometryFbHandles) m_FramebufferManager.Destroy(h);
    m_GeometryFbHandles.clear();
    m_GeometryFramebuffers.clear();

    for (auto h : m_GBufferFbHandles) m_FramebufferManager.Destroy(h);
    m_GBufferFbHandles.clear();
    m_GBufferFramebuffers.clear();

    for (auto h : m_OffscreenGBufferFbHandles) m_FramebufferManager.Destroy(h);
    m_OffscreenGBufferFbHandles.clear();
    m_OffscreenGBufferFramebuffers.clear();

    for (auto h : m_HDRColorFbHandles) m_FramebufferManager.Destroy(h);
    m_HDRColorFbHandles.clear();
    m_HDRColorFramebuffers.clear();

    for (auto h : m_TransparentFbHandles) m_FramebufferManager.Destroy(h);
    m_TransparentFbHandles.clear();
    m_TransparentFramebuffers.clear();

    for (auto h : m_OffscreenTransparentFbHandles) m_FramebufferManager.Destroy(h);
    m_OffscreenTransparentFbHandles.clear();
    m_OffscreenTransparentFramebuffers.clear();

    m_DepthImageViews.clear();
    m_DepthImages.clear();
    m_DepthAllocations.clear();

    m_GBufferAImageViews.clear();
    m_GBufferAImages.clear();
    m_GBufferAAllocations.clear();

    m_GBufferBImageViews.clear();
    m_GBufferBImages.clear();
    m_GBufferBAllocations.clear();

    m_GBufferCImageViews.clear();
    m_GBufferCImages.clear();
    m_GBufferCAllocations.clear();

    m_GBufferDImageViews.clear();
    m_GBufferDImages.clear();
    m_GBufferDAllocations.clear();

    m_ObjectIDImageViews.clear();
    m_ObjectIDImages.clear();
    m_ObjectIDAllocations.clear();

    m_GBufferVelocityImageViews.clear();
    m_GBufferVelocityImages.clear();
    m_GBufferVelocityAllocations.clear();

    m_HDRColorImageViews.clear();
    m_HDRColorImages.clear();
    m_HDRColorAllocations.clear();

    m_HDRColorComposedImageViews.clear();
    m_HDRColorComposedImages.clear();
    m_HDRColorComposedAllocations.clear();
    m_ViewportColorFramebuffers.clear();
    m_ViewportColorFbHandles.clear();

    if (width == 0 || height == 0) return;

    uint32_t maxFrames = resources.MAX_FRAMES_IN_FLIGHT;
    m_DepthHandles.resize(maxFrames);
    m_GBufferAHandles.resize(maxFrames);
    m_GBufferBHandles.resize(maxFrames);
    m_GBufferCHandles.resize(maxFrames);
    m_GBufferDHandles.resize(maxFrames);
    m_ObjectIDHandles.resize(maxFrames);
    m_GBufferVelocityHandles.resize(maxFrames);
    m_HDRColorHandles.resize(maxFrames);
    m_HDRColorComposedHandles.resize(maxFrames);
    m_LDRColorHandles.resize(maxFrames);
    m_ViewportColorHandles.resize(maxFrames);
    m_SSAOHandles.resize(maxFrames);
    m_SSAOBlurredHandles.resize(maxFrames);
    m_SSAOBlurredImages.resize(maxFrames, VK_NULL_HANDLE);
    m_SSAOBlurredAllocations.resize(maxFrames, nullptr);
    m_SSAOBlurredImageViews.resize(maxFrames, VK_NULL_HANDLE);
    m_SSAOFbHandles.resize(maxFrames);
    m_SSAOFramebuffers.resize(maxFrames, VK_NULL_HANDLE);
    m_SSAOBlurredFbHandles.resize(maxFrames);
    m_SSAOFramebuffers.resize(maxFrames, VK_NULL_HANDLE); // wait, let's keep original names
    m_SSAOBlurredFramebuffers.resize(maxFrames, VK_NULL_HANDLE);

    m_DepthFbHandles.resize(maxFrames);
    m_OffscreenDepthFbHandles.resize(maxFrames);
    m_GeometryFbHandles.resize(maxFrames);
    m_GBufferFbHandles.resize(maxFrames);
    m_OffscreenGBufferFbHandles.resize(maxFrames);
    m_HDRColorFbHandles.resize(maxFrames);
    m_TransparentFbHandles.resize(maxFrames);
    m_OffscreenTransparentFbHandles.resize(maxFrames);
    m_ViewportColorFbHandles.resize(maxFrames);
    m_ViewportColorFramebuffers.resize(maxFrames, VK_NULL_HANDLE);

    m_DepthImages.resize(maxFrames, VK_NULL_HANDLE);
    m_DepthAllocations.resize(maxFrames, nullptr);
    m_DepthImageViews.resize(maxFrames, VK_NULL_HANDLE);
    m_DepthFramebuffers.resize(maxFrames, VK_NULL_HANDLE);
    m_OffscreenDepthFramebuffers.resize(maxFrames, VK_NULL_HANDLE);
    m_GeometryFramebuffers.resize(maxFrames, VK_NULL_HANDLE);

    m_GBufferAImages.resize(maxFrames, VK_NULL_HANDLE);
    m_GBufferAAllocations.resize(maxFrames, nullptr);
    m_GBufferAImageViews.resize(maxFrames, VK_NULL_HANDLE);

    m_GBufferBImages.resize(maxFrames, VK_NULL_HANDLE);
    m_GBufferBAllocations.resize(maxFrames, nullptr);
    m_GBufferBImageViews.resize(maxFrames, VK_NULL_HANDLE);

    m_GBufferCImages.resize(maxFrames, VK_NULL_HANDLE);
    m_GBufferCAllocations.resize(maxFrames, nullptr);
    m_GBufferCImageViews.resize(maxFrames, VK_NULL_HANDLE);

    m_GBufferDImages.resize(maxFrames, VK_NULL_HANDLE);
    m_GBufferDAllocations.resize(maxFrames, nullptr);
    m_GBufferDImageViews.resize(maxFrames, VK_NULL_HANDLE);

    m_ObjectIDImages.resize(maxFrames, VK_NULL_HANDLE);
    m_ObjectIDAllocations.resize(maxFrames, nullptr);
    m_ObjectIDImageViews.resize(maxFrames, VK_NULL_HANDLE);

    m_GBufferVelocityImages.resize(maxFrames, VK_NULL_HANDLE);
    m_GBufferVelocityAllocations.resize(maxFrames, nullptr);
    m_GBufferVelocityImageViews.resize(maxFrames, VK_NULL_HANDLE);

    m_GBufferFramebuffers.resize(maxFrames, VK_NULL_HANDLE);
    m_OffscreenGBufferFramebuffers.resize(maxFrames, VK_NULL_HANDLE);

    m_HDRColorImages.resize(maxFrames, VK_NULL_HANDLE);
    m_HDRColorAllocations.resize(maxFrames, nullptr);
    m_HDRColorImageViews.resize(maxFrames, VK_NULL_HANDLE);

    m_HDRColorComposedImages.resize(maxFrames, VK_NULL_HANDLE);
    m_HDRColorComposedAllocations.resize(maxFrames, nullptr);
    m_HDRColorComposedImageViews.resize(maxFrames, VK_NULL_HANDLE);
    m_HDRColorFramebuffers.resize(maxFrames, VK_NULL_HANDLE);

    m_TransparentFramebuffers.resize(maxFrames, VK_NULL_HANDLE);
    m_OffscreenTransparentFramebuffers.resize(maxFrames, VK_NULL_HANDLE);

    for (uint32_t i = 0; i < maxFrames; ++i) {
        // 1. Depth Target
        RenderTargetDesc depthDesc{};
        depthDesc.width = width;
        depthDesc.height = height;
        depthDesc.format = VK_FORMAT_D32_SFLOAT;
        depthDesc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        depthDesc.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthDesc.depthAttachment = true;
        depthDesc.debugName = "DepthBuffer_" + std::to_string(i);
        m_DepthHandles[i] = m_RenderTargetManager.Create(depthDesc);

        const RenderTarget* depthTarget = m_RenderTargetManager.Get(m_DepthHandles[i]);
        m_DepthImages[i] = depthTarget->image;
        m_DepthImageViews[i] = depthTarget->view;
        m_DepthAllocations[i] = depthTarget->allocation;

        // Transition layout to DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        VkCommandBuffer cmd = resources.beginSingleTimeCommands();
        m_RenderTargetManager.Transition(
            cmd,
            m_DepthHandles[i],
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            0,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
        );
        resources.endSingleTimeCommands(cmd);

        // Create depth prepass framebuffer
        FramebufferDesc depthFbDesc{};
        depthFbDesc.renderPass = m_DepthRenderPass;
        depthFbDesc.attachments = { m_DepthHandles[i] };
        depthFbDesc.width = width;
        depthFbDesc.height = height;
        depthFbDesc.layers = 1;
        depthFbDesc.debugName = "DepthPrepassFb_" + std::to_string(i);
        m_DepthFbHandles[i] = m_FramebufferManager.Create(depthFbDesc);
        m_DepthFramebuffers[i] = m_FramebufferManager.Get(m_DepthFbHandles[i]);

        // Create offscreen depth prepass framebuffer
        if (m_ViewportRenderer.isOffscreenRenderingEnabled()) {
            VkImageView offscreenDepthAttachment = m_ViewportRenderer.getOffscreenDepthImageView(i);
            if (offscreenDepthAttachment != VK_NULL_HANDLE) {
                FramebufferDesc offscreenDepthFbDesc{};
                offscreenDepthFbDesc.renderPass = m_DepthRenderPass;
                offscreenDepthFbDesc.rawAttachments = { offscreenDepthAttachment };
                offscreenDepthFbDesc.width = width;
                offscreenDepthFbDesc.height = height;
                offscreenDepthFbDesc.layers = 1;
                offscreenDepthFbDesc.debugName = "OffscreenDepthPrepassFb_" + std::to_string(i);
                m_OffscreenDepthFbHandles[i] = m_FramebufferManager.Create(offscreenDepthFbDesc);
                m_OffscreenDepthFramebuffers[i] = m_FramebufferManager.Get(m_OffscreenDepthFbHandles[i]);
            }
        }

        // Create geometry framebuffer
        if (!m_ViewportRenderer.isOffscreenRenderingEnabled() && i < resources.swapChainImageViews.size()) {
            VkImageView geomAttachments[] = { resources.swapChainImageViews[i], m_DepthImageViews[i] };
            FramebufferDesc geomFbDesc{};
            geomFbDesc.renderPass = m_GeometryRenderPass;
            geomFbDesc.rawAttachments = { geomAttachments[0], geomAttachments[1] };
            geomFbDesc.width = resources.swapChainExtent.width;
            geomFbDesc.height = resources.swapChainExtent.height;
            geomFbDesc.layers = 1;
            geomFbDesc.debugName = "GeometryFb_" + std::to_string(i);
            m_GeometryFbHandles[i] = m_FramebufferManager.Create(geomFbDesc);
            m_GeometryFramebuffers[i] = m_FramebufferManager.Get(m_GeometryFbHandles[i]);
        }

        // 2. GBufferA
        RenderTargetDesc gbufferADesc{};
        gbufferADesc.width = width;
        gbufferADesc.height = height;
        gbufferADesc.format = VK_FORMAT_R8G8B8A8_UNORM;
        gbufferADesc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        gbufferADesc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        gbufferADesc.colorAttachment = true;
        gbufferADesc.debugName = "GBufferA_" + std::to_string(i);
        m_GBufferAHandles[i] = m_RenderTargetManager.Create(gbufferADesc);

        const RenderTarget* gbufferATarget = m_RenderTargetManager.Get(m_GBufferAHandles[i]);
        m_GBufferAImages[i] = gbufferATarget->image;
        m_GBufferAImageViews[i] = gbufferATarget->view;
        m_GBufferAAllocations[i] = gbufferATarget->allocation;

        // 3. GBufferB
        RenderTargetDesc gbufferBDesc{};
        gbufferBDesc.width = width;
        gbufferBDesc.height = height;
        gbufferBDesc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        gbufferBDesc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        gbufferBDesc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        gbufferBDesc.colorAttachment = true;
        gbufferBDesc.debugName = "GBufferB_" + std::to_string(i);
        m_GBufferBHandles[i] = m_RenderTargetManager.Create(gbufferBDesc);

        const RenderTarget* gbufferBTarget = m_RenderTargetManager.Get(m_GBufferBHandles[i]);
        m_GBufferBImages[i] = gbufferBTarget->image;
        m_GBufferBImageViews[i] = gbufferBTarget->view;
        m_GBufferBAllocations[i] = gbufferBTarget->allocation;

        // 4. GBufferC
        RenderTargetDesc gbufferCDesc{};
        gbufferCDesc.width = width;
        gbufferCDesc.height = height;
        gbufferCDesc.format = VK_FORMAT_R8G8B8A8_UNORM;
        gbufferCDesc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        gbufferCDesc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        gbufferCDesc.colorAttachment = true;
        gbufferCDesc.debugName = "GBufferC_" + std::to_string(i);
        m_GBufferCHandles[i] = m_RenderTargetManager.Create(gbufferCDesc);

        const RenderTarget* gbufferCTarget = m_RenderTargetManager.Get(m_GBufferCHandles[i]);
        m_GBufferCImages[i] = gbufferCTarget->image;
        m_GBufferCImageViews[i] = gbufferCTarget->view;
        m_GBufferCAllocations[i] = gbufferCTarget->allocation;

        // 5. GBufferD
        RenderTargetDesc gbufferDDesc{};
        gbufferDDesc.width = width;
        gbufferDDesc.height = height;
        gbufferDDesc.format = VK_FORMAT_R8G8B8A8_UNORM;
        gbufferDDesc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        gbufferDDesc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        gbufferDDesc.colorAttachment = true;
        gbufferDDesc.debugName = "GBufferD_" + std::to_string(i);
        m_GBufferDHandles[i] = m_RenderTargetManager.Create(gbufferDDesc);

        const RenderTarget* gbufferDTarget = m_RenderTargetManager.Get(m_GBufferDHandles[i]);
        m_GBufferDImages[i] = gbufferDTarget->image;
        m_GBufferDImageViews[i] = gbufferDTarget->view;
        m_GBufferDAllocations[i] = gbufferDTarget->allocation;

        // 6. GBufferObjectID
        RenderTargetDesc objectIDDesc{};
        objectIDDesc.width = width;
        objectIDDesc.height = height;
        objectIDDesc.format = VK_FORMAT_R32_UINT;
        objectIDDesc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        objectIDDesc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        objectIDDesc.colorAttachment = true;
        objectIDDesc.debugName = "GBufferObjectID_" + std::to_string(i);
        m_ObjectIDHandles[i] = m_RenderTargetManager.Create(objectIDDesc);

        const RenderTarget* objectIDTarget = m_RenderTargetManager.Get(m_ObjectIDHandles[i]);
        m_ObjectIDImages[i] = objectIDTarget->image;
        m_ObjectIDImageViews[i] = objectIDTarget->view;        m_ObjectIDAllocations[i] = objectIDTarget->allocation;

        // 7. GBufferVelocity
        RenderTargetDesc velocityDesc{};
        velocityDesc.width = width;
        velocityDesc.height = height;
        velocityDesc.format = VK_FORMAT_R16G16_SFLOAT;
        velocityDesc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        velocityDesc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        velocityDesc.colorAttachment = true;
        velocityDesc.debugName = "GBufferVelocity_" + std::to_string(i);
        m_GBufferVelocityHandles[i] = m_RenderTargetManager.Create(velocityDesc);

        const RenderTarget* velocityTarget = m_RenderTargetManager.Get(m_GBufferVelocityHandles[i]);
        m_GBufferVelocityImages[i] = velocityTarget->image;
        m_GBufferVelocityImageViews[i] = velocityTarget->view;
        m_GBufferVelocityAllocations[i] = velocityTarget->allocation;

        // Create GBuffer framebuffer
        FramebufferDesc gbufferFbDesc{};
        gbufferFbDesc.renderPass = m_GBufferRenderPass;
        gbufferFbDesc.attachments = {
            m_GBufferAHandles[i],
            m_GBufferBHandles[i],
            m_GBufferCHandles[i],
            m_GBufferDHandles[i],
            m_ObjectIDHandles[i],
            m_GBufferVelocityHandles[i],
            m_DepthHandles[i]
        };
        gbufferFbDesc.width = width;
        gbufferFbDesc.height = height;
        gbufferFbDesc.layers = 1;
        gbufferFbDesc.debugName = "GBufferFb_" + std::to_string(i);
        m_GBufferFbHandles[i] = m_FramebufferManager.Create(gbufferFbDesc);
        m_GBufferFramebuffers[i] = m_FramebufferManager.Get(m_GBufferFbHandles[i]);

        // Create offscreen GBuffer framebuffer
        if (m_ViewportRenderer.isOffscreenRenderingEnabled()) {
            VkImageView offscreenGBufferDepth = m_ViewportRenderer.getOffscreenDepthImageView(i);
            if (offscreenGBufferDepth != VK_NULL_HANDLE) {
                FramebufferDesc offscreenGBufferFbDesc{};
                offscreenGBufferFbDesc.renderPass = m_GBufferRenderPass;
                offscreenGBufferFbDesc.rawAttachments = {
                    m_GBufferAImageViews[i],
                    m_GBufferBImageViews[i],
                    m_GBufferCImageViews[i],
                    m_GBufferDImageViews[i],
                    m_ObjectIDImageViews[i],
                    m_GBufferVelocityImageViews[i],
                    offscreenGBufferDepth
                };
                offscreenGBufferFbDesc.width = width;
                offscreenGBufferFbDesc.height = height;
                offscreenGBufferFbDesc.layers = 1;
                offscreenGBufferFbDesc.debugName = "OffscreenGBufferFb_" + std::to_string(i);
                m_OffscreenGBufferFbHandles[i] = m_FramebufferManager.Create(offscreenGBufferFbDesc);
                m_OffscreenGBufferFramebuffers[i] = m_FramebufferManager.Get(m_OffscreenGBufferFbHandles[i]);
            }
        }

        // 6. HDR Color Target
        RenderTargetDesc hdrDesc{};
        hdrDesc.width = width;
        hdrDesc.height = height;
        hdrDesc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        hdrDesc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        hdrDesc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        hdrDesc.colorAttachment = true;
        hdrDesc.debugName = "HDRColor_" + std::to_string(i);
        m_HDRColorHandles[i] = m_RenderTargetManager.Create(hdrDesc);

        const RenderTarget* hdrTarget = m_RenderTargetManager.Get(m_HDRColorHandles[i]);
        m_HDRColorImages[i] = hdrTarget->image;
        m_HDRColorImageViews[i] = hdrTarget->view;
        m_HDRColorAllocations[i] = hdrTarget->allocation;

        // 6b. HDR Color Composed Target
        RenderTargetDesc composedDesc{};
        composedDesc.width = width;
        composedDesc.height = height;
        composedDesc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        composedDesc.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        composedDesc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        composedDesc.colorAttachment = false;
        composedDesc.debugName = "HDRColorComposed_" + std::to_string(i);
        m_HDRColorComposedHandles[i] = m_RenderTargetManager.Create(composedDesc);

        const RenderTarget* composedTarget = m_RenderTargetManager.Get(m_HDRColorComposedHandles[i]);
        m_HDRColorComposedImages[i] = composedTarget->image;
        m_HDRColorComposedImageViews[i] = composedTarget->view;
        m_HDRColorComposedAllocations[i] = composedTarget->allocation;

        // Create HDR framebuffer
        FramebufferDesc hdrFbDesc{};
        hdrFbDesc.renderPass = m_HDRRenderPass;
        hdrFbDesc.attachments = { m_HDRColorHandles[i] };
        hdrFbDesc.width = width;
        hdrFbDesc.height = height;
        hdrFbDesc.layers = 1;
        hdrFbDesc.debugName = "HDRColorFb_" + std::to_string(i);
        m_HDRColorFbHandles[i] = m_FramebufferManager.Create(hdrFbDesc);
        m_HDRColorFramebuffers[i] = m_FramebufferManager.Get(m_HDRColorFbHandles[i]);

        // Create Transparent framebuffer
        FramebufferDesc transFbDesc{};
        transFbDesc.renderPass = m_TransparentRenderPass;
        transFbDesc.attachments = { m_HDRColorHandles[i], m_DepthHandles[i] };
        transFbDesc.width = width;
        transFbDesc.height = height;
        transFbDesc.layers = 1;
        transFbDesc.debugName = "TransparentFb_" + std::to_string(i);
        m_TransparentFbHandles[i] = m_FramebufferManager.Create(transFbDesc);
        m_TransparentFramebuffers[i] = m_FramebufferManager.Get(m_TransparentFbHandles[i]);

        // Create offscreen Transparent framebuffer
        if (m_ViewportRenderer.isOffscreenRenderingEnabled()) {
            VkImageView offscreenTransDepth = m_ViewportRenderer.getOffscreenDepthImageView(i);
            if (offscreenTransDepth != VK_NULL_HANDLE) {
                FramebufferDesc offscreenTransFbDesc{};
                offscreenTransFbDesc.renderPass = m_TransparentRenderPass;
                offscreenTransFbDesc.rawAttachments = { m_HDRColorImageViews[i], offscreenTransDepth };
                offscreenTransFbDesc.width = width;
                offscreenTransFbDesc.height = height;
                offscreenTransFbDesc.layers = 1;
                offscreenTransFbDesc.debugName = "OffscreenTransparentFb_" + std::to_string(i);
                m_OffscreenTransparentFbHandles[i] = m_FramebufferManager.Create(offscreenTransFbDesc);
                m_OffscreenTransparentFramebuffers[i] = m_FramebufferManager.Get(m_OffscreenTransparentFbHandles[i]);
            }
        }

        // 7. LDR Color Target
        RenderTargetDesc ldrDesc{};
        ldrDesc.width = width;
        ldrDesc.height = height;
        ldrDesc.format = resources.swapChainImageFormat;
        ldrDesc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        ldrDesc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        ldrDesc.colorAttachment = true;
        ldrDesc.debugName = "LDRColor_" + std::to_string(i);
        m_LDRColorHandles[i] = m_RenderTargetManager.Create(ldrDesc);

        // 8. ViewportColor Target
        RenderTargetDesc vpDesc{};
        vpDesc.width = width;
        vpDesc.height = height;
        vpDesc.format = resources.swapChainImageFormat;
        vpDesc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        vpDesc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        vpDesc.colorAttachment = true;
        vpDesc.debugName = "ViewportColor_" + std::to_string(i);
        m_ViewportColorHandles[i] = m_RenderTargetManager.Create(vpDesc);

        // Create ViewportColor framebuffer
        FramebufferDesc vpFbDesc{};
        vpFbDesc.renderPass = m_ViewportColorRenderPass;
        vpFbDesc.attachments = { m_ViewportColorHandles[i] };
        vpFbDesc.width = width;
        vpFbDesc.height = height;
        vpFbDesc.layers = 1;
        vpFbDesc.debugName = "ViewportColorFb_" + std::to_string(i);
        m_ViewportColorFbHandles[i] = m_FramebufferManager.Create(vpFbDesc);
        m_ViewportColorFramebuffers[i] = m_FramebufferManager.Get(m_ViewportColorFbHandles[i]);

        // 9. SSAO Foundation Target (Format R8_UNORM)
        RenderTargetDesc ssaoDesc{};
        ssaoDesc.width = width;
        ssaoDesc.height = height;
        ssaoDesc.format = VK_FORMAT_R8_UNORM;
        ssaoDesc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ssaoDesc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        ssaoDesc.colorAttachment = true;
        ssaoDesc.debugName = "SSAO_" + std::to_string(i);
        m_SSAOHandles[i] = m_RenderTargetManager.Create(ssaoDesc);

        // SSAO Framebuffer
        FramebufferDesc ssaoFbDesc{};
        ssaoFbDesc.renderPass = m_SSAORenderPass;
        ssaoFbDesc.attachments = { m_SSAOHandles[i] };
        ssaoFbDesc.width = width;
        ssaoFbDesc.height = height;
        ssaoFbDesc.layers = 1;
        ssaoFbDesc.debugName = "SSAOFb_" + std::to_string(i);
        m_SSAOFbHandles[i] = m_FramebufferManager.Create(ssaoFbDesc);
        m_SSAOFramebuffers[i] = m_FramebufferManager.Get(m_SSAOFbHandles[i]);

        // SSAO Blurred Target (Format R8_UNORM)
        RenderTargetDesc ssaoBlurDesc{};
        ssaoBlurDesc.width = width;
        ssaoBlurDesc.height = height;
        ssaoBlurDesc.format = VK_FORMAT_R8_UNORM;
        ssaoBlurDesc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ssaoBlurDesc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        ssaoBlurDesc.colorAttachment = true;
        ssaoBlurDesc.debugName = "SSAOBlurred_" + std::to_string(i);
        m_SSAOBlurredHandles[i] = m_RenderTargetManager.Create(ssaoBlurDesc);

        const RenderTarget* ssaoBlurTarget = m_RenderTargetManager.Get(m_SSAOBlurredHandles[i]);
        if (ssaoBlurTarget) {
            m_SSAOBlurredImages[i] = ssaoBlurTarget->image;
            m_SSAOBlurredImageViews[i] = ssaoBlurTarget->view;
            m_SSAOBlurredAllocations[i] = ssaoBlurTarget->allocation;
        }

        // SSAO Blurred Framebuffer
        FramebufferDesc ssaoBlurFbDesc{};
        ssaoBlurFbDesc.renderPass = m_SSAORenderPass;
        ssaoBlurFbDesc.attachments = { m_SSAOBlurredHandles[i] };
        ssaoBlurFbDesc.width = width;
        ssaoBlurFbDesc.height = height;
        ssaoBlurFbDesc.layers = 1;
        ssaoBlurFbDesc.debugName = "SSAOBlurredFb_" + std::to_string(i);
        m_SSAOBlurredFbHandles[i] = m_FramebufferManager.Create(ssaoBlurFbDesc);
        m_SSAOBlurredFramebuffers[i] = m_FramebufferManager.Get(m_SSAOBlurredFbHandles[i]);
    }
    
    updateGBufferDescriptorSets();
    updateExposureDescriptorSets();
    recreateTAAResources(width, height);
    updateTAADescriptorSets();
    m_HZBPass.RecreateResources(resources, width, height);

    // Update PostProcess descriptor sets
    for (uint32_t i = 0; i < maxFrames; ++i) {
        if (m_HDRColorImageViews[i] == VK_NULL_HANDLE || m_ExposureBuffer == VK_NULL_HANDLE) continue;

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_HDRColorImageViews[i];
        imageInfo.sampler = m_GBufferSampler;

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_ExposureBuffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(float) * 2;

        VkDescriptorImageInfo depthInfo{};
        depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        depthInfo.imageView = m_ViewportRenderer.isOffscreenRenderingEnabled() ?
            m_ViewportRenderer.getOffscreenDepthImageView(i) : (i < m_DepthImageViews.size() ? m_DepthImageViews[i] : VK_NULL_HANDLE);
        depthInfo.sampler = m_GBufferSampler;

        std::array<VkWriteDescriptorSet, 3> writes{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = m_PostProcessDescriptorSets[i];
        writes[0].dstBinding = 0;
        writes[0].dstArrayElement = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].descriptorCount = 1;
        writes[0].pImageInfo = &imageInfo;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = m_PostProcessDescriptorSets[i];
        writes[1].dstBinding = 1;
        writes[1].dstArrayElement = 0;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[1].descriptorCount = 1;
        writes[1].pBufferInfo = &bufferInfo;

        writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2].dstSet = m_PostProcessDescriptorSets[i];
        writes[2].dstBinding = 2;
        writes[2].dstArrayElement = 0;
        writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[2].descriptorCount = 1;
        writes[2].pImageInfo = &depthInfo;

        vkUpdateDescriptorSets(resources.device, 3, writes.data(), 0, nullptr);
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

    m_ShadowHandles.resize(maxFrames);
    m_ShadowFbHandles.resize(maxFrames);
    m_ShadowImages.resize(maxFrames, VK_NULL_HANDLE);
    m_ShadowImageViews.resize(maxFrames, VK_NULL_HANDLE);
    m_ShadowAllocations.resize(maxFrames, VK_NULL_HANDLE);
    m_ShadowFramebuffers.resize(maxFrames, VK_NULL_HANDLE);

    for (uint32_t c = 0; c < 4; ++c) {
        m_ShadowHandlesCascades[c].resize(maxFrames);
        m_ShadowFbHandlesCascades[c].resize(maxFrames);
        m_ShadowImagesCascades[c].resize(maxFrames, VK_NULL_HANDLE);
        m_ShadowImageViewsCascades[c].resize(maxFrames, VK_NULL_HANDLE);
        m_ShadowAllocationsCascades[c].resize(maxFrames, VK_NULL_HANDLE);
        m_ShadowFramebuffersCascades[c].resize(maxFrames, VK_NULL_HANDLE);
    }

    for (uint32_t i = 0; i < maxFrames; ++i) {
        for (uint32_t c = 0; c < 4; ++c) {
            RenderTargetDesc shadowDesc{};
            shadowDesc.width = m_CurrentShadowResolution;
            shadowDesc.height = m_CurrentShadowResolution;
            shadowDesc.format = VK_FORMAT_D32_SFLOAT;
            shadowDesc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            shadowDesc.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
            shadowDesc.depthAttachment = true;
            shadowDesc.debugName = "ShadowMap_Cascade_" + std::to_string(c) + "_" + std::to_string(i);
            m_ShadowHandlesCascades[c][i] = m_RenderTargetManager.Create(shadowDesc);

            const RenderTarget* shadowTarget = m_RenderTargetManager.Get(m_ShadowHandlesCascades[c][i]);
            m_ShadowImagesCascades[c][i] = shadowTarget->image;
            m_ShadowImageViewsCascades[c][i] = shadowTarget->view;
            m_ShadowAllocationsCascades[c][i] = shadowTarget->allocation;

            // Framebuffer
            FramebufferDesc shadowFbDesc{};
            shadowFbDesc.renderPass = m_ShadowRenderPass;
            shadowFbDesc.attachments = { m_ShadowHandlesCascades[c][i] };
            shadowFbDesc.width = m_CurrentShadowResolution;
            shadowFbDesc.height = m_CurrentShadowResolution;
            shadowFbDesc.layers = 1;
            shadowFbDesc.debugName = "ShadowFb_Cascade_" + std::to_string(c) + "_" + std::to_string(i);
            m_ShadowFbHandlesCascades[c][i] = m_FramebufferManager.Create(shadowFbDesc);
            m_ShadowFramebuffersCascades[c][i] = m_FramebufferManager.Get(m_ShadowFbHandlesCascades[c][i]);
        }

        // Maintain aliases for cascade 0
        m_ShadowHandles[i] = m_ShadowHandlesCascades[0][i];
        m_ShadowImages[i] = m_ShadowImagesCascades[0][i];
        m_ShadowImageViews[i] = m_ShadowImageViewsCascades[0][i];
        m_ShadowAllocations[i] = m_ShadowAllocationsCascades[0][i];
        m_ShadowFbHandles[i] = m_ShadowFbHandlesCascades[0][i];
        m_ShadowFramebuffers[i] = m_ShadowFramebuffersCascades[0][i];
    }

    m_ShadowAtlasHandles.resize(maxFrames);
    m_ShadowAtlasFbHandles.resize(maxFrames);
    m_ShadowAtlasImages.resize(maxFrames, VK_NULL_HANDLE);
    m_ShadowAtlasImageViews.resize(maxFrames, VK_NULL_HANDLE);
    m_ShadowAtlasAllocations.resize(maxFrames, VK_NULL_HANDLE);
    m_ShadowAtlasFramebuffers.resize(maxFrames, VK_NULL_HANDLE);

    for (uint32_t i = 0; i < maxFrames; ++i) {
        RenderTargetDesc atlasDesc{};
        atlasDesc.width = 2048;
        atlasDesc.height = 2048;
        atlasDesc.format = VK_FORMAT_D32_SFLOAT;
        atlasDesc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        atlasDesc.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        atlasDesc.depthAttachment = true;
        atlasDesc.debugName = "ShadowAtlas_" + std::to_string(i);
        m_ShadowAtlasHandles[i] = m_RenderTargetManager.Create(atlasDesc);

        const RenderTarget* atlasTarget = m_RenderTargetManager.Get(m_ShadowAtlasHandles[i]);
        m_ShadowAtlasImages[i] = atlasTarget->image;
        m_ShadowAtlasImageViews[i] = atlasTarget->view;
        m_ShadowAtlasAllocations[i] = atlasTarget->allocation;

        FramebufferDesc atlasFbDesc{};
        atlasFbDesc.renderPass = m_ShadowRenderPass;
        atlasFbDesc.attachments = { m_ShadowAtlasHandles[i] };
        atlasFbDesc.width = 2048;
        atlasFbDesc.height = 2048;
        atlasFbDesc.layers = 1;
        atlasFbDesc.debugName = "ShadowAtlas_FB_" + std::to_string(i);
        m_ShadowAtlasFbHandles[i] = m_FramebufferManager.Create(atlasFbDesc);
        m_ShadowAtlasFramebuffers[i] = m_FramebufferManager.Get(m_ShadowAtlasFbHandles[i]);
    }
    m_ShadowImGuiTextures.assign(maxFrames, VK_NULL_HANDLE);
}

void Renderer::destroyShadowResources()
{
    if (resources.device == VK_NULL_HANDLE) return;

    for (auto tex : m_ShadowImGuiTextures) {
        if (tex != VK_NULL_HANDLE) {
            ImGui_ImplVulkan_RemoveTexture(tex);
        }
    }
    m_ShadowImGuiTextures.clear();

    for (uint32_t c = 0; c < 4; ++c) {
        for (auto h : m_ShadowFbHandlesCascades[c]) {
            m_FramebufferManager.Destroy(h);
        }
        m_ShadowFbHandlesCascades[c].clear();
        m_ShadowFramebuffersCascades[c].clear();

        for (auto h : m_ShadowHandlesCascades[c]) {
            m_RenderTargetManager.Destroy(h);
        }
        m_ShadowHandlesCascades[c].clear();
        m_ShadowImageViewsCascades[c].clear();
        m_ShadowImagesCascades[c].clear();
        m_ShadowAllocationsCascades[c].clear();
    }

    m_ShadowFbHandles.clear();
    m_ShadowFramebuffers.clear();
    m_ShadowHandles.clear();
    m_ShadowImageViews.clear();
    m_ShadowImages.clear();
    m_ShadowAllocations.clear();

    for (auto h : m_ShadowAtlasFbHandles) {
        m_FramebufferManager.Destroy(h);
    }
    m_ShadowAtlasFbHandles.clear();
    m_ShadowAtlasFramebuffers.clear();

    for (auto h : m_ShadowAtlasHandles) {
        m_RenderTargetManager.Destroy(h);
    }
    m_ShadowAtlasHandles.clear();
    m_ShadowAtlasImageViews.clear();
    m_ShadowAtlasImages.clear();
    m_ShadowAtlasAllocations.clear();

    if (m_ShadowRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(resources.device, m_ShadowRenderPass, nullptr);
        m_ShadowRenderPass = VK_NULL_HANDLE;
    }
    if (m_ShadowSampler != VK_NULL_HANDLE) {
        vkDestroySampler(resources.device, m_ShadowSampler, nullptr);
        m_ShadowSampler = VK_NULL_HANDLE;
    }
}

void Renderer::createSSAOResources()
{
    if (resources.device == VK_NULL_HANDLE) return;

    uint32_t maxFrames = resources.MAX_FRAMES_IN_FLIGHT;

    // 1. Create m_SSAORenderPass
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = VK_FORMAT_R8_UNORM;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = nullptr;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VK_CHECK(vkCreateRenderPass(resources.device, &renderPassInfo, nullptr, &m_SSAORenderPass));

    // 2. Generate kernel samples and noise
    std::default_random_engine generator(12345);
    std::uniform_real_distribution<float> randomFloats(0.0f, 1.0f);
    m_SSAOKernel.clear();
    for (uint32_t i = 0; i < 64; ++i) {
        glm::vec3 sample(
            randomFloats(generator) * 2.0f - 1.0f,
            randomFloats(generator) * 2.0f - 1.0f,
            randomFloats(generator)
        );
        sample = glm::normalize(sample);
        sample *= randomFloats(generator);

        float scale = (float)i / 64.0f;
        scale = glm::mix(0.1f, 1.0f, scale * scale);
        sample *= scale;

        m_SSAOKernel.push_back(glm::vec4(sample, 0.0f));
    }

    std::vector<glm::vec4> ssaoNoise;
    for (uint32_t i = 0; i < 16; ++i) {
        glm::vec3 noise(
            randomFloats(generator) * 2.0f - 1.0f,
            randomFloats(generator) * 2.0f - 1.0f,
            0.0f
        );
        noise = glm::normalize(noise);
        ssaoNoise.push_back(glm::vec4(noise, 0.0f));
    }

    VkDeviceSize noiseSize = 16 * sizeof(glm::vec4);

    VkBuffer stagingBuffer;
    VmaAllocation stagingAlloc;
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size  = noiseSize;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo bufAllocInfo{};
    bufAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &bufAllocInfo, &stagingBuffer, &stagingAlloc, nullptr));
    ::eng::ResourceTracker::incBuffer();

    void* data = nullptr;
    vmaMapMemory(resources.allocator, stagingAlloc, &data);
    std::memcpy(data, ssaoNoise.data(), noiseSize);
    vmaUnmapMemory(resources.allocator, stagingAlloc);

    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    imgInfo.extent = {4, 4, 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling  = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage   = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo imgAllocInfo{};
    imgAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VK_CHECK(vmaCreateImage(resources.allocator, &imgInfo, &imgAllocInfo, &m_SSAONoiseImage, &m_SSAONoiseAllocation, nullptr));
    ::eng::ResourceTracker::incImage();

    // Upload
    VkCommandBuffer cmd = resources.beginSingleTimeCommands();
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.image = m_SSAONoiseImage;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {4, 4, 1};
    vkCmdCopyBufferToImage(cmd, stagingBuffer, m_SSAONoiseImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    resources.endSingleTimeCommands(cmd);

    vmaDestroyBuffer(resources.allocator, stagingBuffer, stagingAlloc);
    ::eng::ResourceTracker::decBuffer();

    // View & Sampler
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_SSAONoiseImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VK_CHECK(vkCreateImageView(resources.device, &viewInfo, nullptr, &m_SSAONoiseImageView));

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VK_CHECK(vkCreateSampler(resources.device, &samplerInfo, nullptr, &m_SSAONoiseSampler));

    // 3. Constant Buffers
    m_SSAOConstantBuffers.resize(maxFrames);
    m_SSAOConstantAllocations.resize(maxFrames);

    VkDeviceSize constBufferSize = 1120;
    for (uint32_t i = 0; i < maxFrames; ++i) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = constBufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VK_CHECK(vmaCreateBuffer(resources.allocator, &bufferInfo, &allocInfo, &m_SSAOConstantBuffers[i], &m_SSAOConstantAllocations[i], nullptr));
        ::eng::ResourceTracker::incBuffer();
    }

    // 4. Create m_SSAODescriptorSetLayout
    VkDescriptorSetLayoutBinding bindings[4]{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[0].pImmutableSamplers = nullptr;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].pImmutableSamplers = nullptr;

    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[2].pImmutableSamplers = nullptr;

    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[3].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 4;
    layoutInfo.pBindings = bindings;
    VK_CHECK(vkCreateDescriptorSetLayout(resources.device, &layoutInfo, nullptr, &m_SSAODescriptorSetLayout));

    // Allocate SSAO Descriptor Sets
    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = maxFrames * 3;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[1].descriptorCount = maxFrames;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = maxFrames;
    VK_CHECK(vkCreateDescriptorPool(resources.device, &poolInfo, nullptr, &m_SSAODescriptorPool));

    m_SSAODescriptorSets.resize(maxFrames);
    std::vector<VkDescriptorSetLayout> layouts(maxFrames, m_SSAODescriptorSetLayout);
    VkDescriptorSetAllocateInfo allocSetInfo{};
    allocSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocSetInfo.descriptorPool = m_SSAODescriptorPool;
    allocSetInfo.descriptorSetCount = maxFrames;
    allocSetInfo.pSetLayouts = layouts.data();
    VK_CHECK(vkAllocateDescriptorSets(resources.device, &allocSetInfo, m_SSAODescriptorSets.data()));

    // 5. Create m_SSAOPipelineLayout
    std::array<VkDescriptorSetLayout, 2> pipelineSetLayouts = {
        resources.globalSetLayout,
        m_SSAODescriptorSetLayout
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(pipelineSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = pipelineSetLayouts.data();
    VK_CHECK(vkCreatePipelineLayout(resources.device, &pipelineLayoutInfo, nullptr, &m_SSAOPipelineLayout));

    // 6. Create SSAO Pipeline
    VkShaderModule fullscreenVert = resources.loadShaderModule("shaders/fullscreen_vert.spv");
    VkShaderModule ssaoFrag = resources.loadShaderModule("shaders/ssao_frag.spv");

    if (fullscreenVert != VK_NULL_HANDLE && ssaoFrag != VK_NULL_HANDLE) {
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = fullscreenVert;
        stages[0].pName = "main";

        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = ssaoFrag;
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
        blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
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
        pipelineInfo.layout = m_SSAOPipelineLayout;
        pipelineInfo.renderPass = m_SSAORenderPass;

        VK_CHECK(vkCreateGraphicsPipelines(resources.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_SSAOPipeline));

        vkDestroyShaderModule(resources.device, fullscreenVert, nullptr);
        vkDestroyShaderModule(resources.device, ssaoFrag, nullptr);
    }

    // 7. Create m_SSAOBlurDescriptorSetLayout
    VkDescriptorSetLayoutBinding blurBindings[3]{};
    for (uint32_t b = 0; b < 3; ++b) {
        blurBindings[b].binding = b;
        blurBindings[b].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        blurBindings[b].descriptorCount = 1;
        blurBindings[b].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        blurBindings[b].pImmutableSamplers = nullptr;
    }

    VkDescriptorSetLayoutCreateInfo blurLayoutInfo{};
    blurLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    blurLayoutInfo.bindingCount = 3;
    blurLayoutInfo.pBindings = blurBindings;
    VK_CHECK(vkCreateDescriptorSetLayout(resources.device, &blurLayoutInfo, nullptr, &m_SSAOBlurDescriptorSetLayout));

    // Allocate SSAO Blur Descriptor Sets
    VkDescriptorPoolSize blurPoolSize{};
    blurPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    blurPoolSize.descriptorCount = maxFrames * 3;

    VkDescriptorPoolCreateInfo blurPoolInfo{};
    blurPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    blurPoolInfo.poolSizeCount = 1;
    blurPoolInfo.pPoolSizes = &blurPoolSize;
    blurPoolInfo.maxSets = maxFrames;
    VK_CHECK(vkCreateDescriptorPool(resources.device, &blurPoolInfo, nullptr, &m_SSAOBlurDescriptorPool));

    m_SSAOBlurDescriptorSets.resize(maxFrames);
    std::vector<VkDescriptorSetLayout> blurLayouts(maxFrames, m_SSAOBlurDescriptorSetLayout);
    VkDescriptorSetAllocateInfo allocBlurSetInfo{};
    allocBlurSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocBlurSetInfo.descriptorPool = m_SSAOBlurDescriptorPool;
    allocBlurSetInfo.descriptorSetCount = maxFrames;
    allocBlurSetInfo.pSetLayouts = blurLayouts.data();
    VK_CHECK(vkAllocateDescriptorSets(resources.device, &allocBlurSetInfo, m_SSAOBlurDescriptorSets.data()));

    // 8. Create m_SSAOBlurPipelineLayout
    VkPipelineLayoutCreateInfo blurPipelineLayoutInfo{};
    blurPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    blurPipelineLayoutInfo.setLayoutCount = 1;
    blurPipelineLayoutInfo.pSetLayouts = &m_SSAOBlurDescriptorSetLayout;
    VK_CHECK(vkCreatePipelineLayout(resources.device, &blurPipelineLayoutInfo, nullptr, &m_SSAOBlurPipelineLayout));

    // 9. Create SSAO Blur Pipeline
    fullscreenVert = resources.loadShaderModule("shaders/fullscreen_vert.spv");
    VkShaderModule ssaoBlurFrag = resources.loadShaderModule("shaders/ssao_blur_frag.spv");

    if (fullscreenVert != VK_NULL_HANDLE && ssaoBlurFrag != VK_NULL_HANDLE) {
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = fullscreenVert;
        stages[0].pName = "main";

        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = ssaoBlurFrag;
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
        blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
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
        pipelineInfo.layout = m_SSAOBlurPipelineLayout;
        pipelineInfo.renderPass = m_SSAORenderPass;

        VK_CHECK(vkCreateGraphicsPipelines(resources.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_SSAOBlurPipeline));

        vkDestroyShaderModule(resources.device, fullscreenVert, nullptr);
        vkDestroyShaderModule(resources.device, ssaoBlurFrag, nullptr);
    }

    m_SSAOBlurredImGuiTextures.assign(maxFrames, VK_NULL_HANDLE);
}

void Renderer::destroySSAOResources()
{
    if (resources.device == VK_NULL_HANDLE) return;

    for (auto tex : m_SSAOBlurredImGuiTextures) {
        if (tex != VK_NULL_HANDLE) {
            ImGui_ImplVulkan_RemoveTexture(tex);
        }
    }
    m_SSAOBlurredImGuiTextures.clear();

    if (m_SSAOPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(resources.device, m_SSAOPipeline, nullptr);
        m_SSAOPipeline = VK_NULL_HANDLE;
    }
    if (m_SSAOPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(resources.device, m_SSAOPipelineLayout, nullptr);
        m_SSAOPipelineLayout = VK_NULL_HANDLE;
    }
    if (m_SSAODescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(resources.device, m_SSAODescriptorSetLayout, nullptr);
        m_SSAODescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_SSAODescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(resources.device, m_SSAODescriptorPool, nullptr);
        m_SSAODescriptorPool = VK_NULL_HANDLE;
    }

    if (m_SSAOBlurPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(resources.device, m_SSAOBlurPipeline, nullptr);
        m_SSAOBlurPipeline = VK_NULL_HANDLE;
    }
    if (m_SSAOBlurPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(resources.device, m_SSAOBlurPipelineLayout, nullptr);
        m_SSAOBlurPipelineLayout = VK_NULL_HANDLE;
    }
    if (m_SSAOBlurDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(resources.device, m_SSAOBlurDescriptorSetLayout, nullptr);
        m_SSAOBlurDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_SSAOBlurDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(resources.device, m_SSAOBlurDescriptorPool, nullptr);
        m_SSAOBlurDescriptorPool = VK_NULL_HANDLE;
    }

    if (m_SSAONoiseImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(resources.device, m_SSAONoiseImageView, nullptr);
        m_SSAONoiseImageView = VK_NULL_HANDLE;
    }
    if (m_SSAONoiseImage != VK_NULL_HANDLE) {
        vmaDestroyImage(resources.allocator, m_SSAONoiseImage, m_SSAONoiseAllocation);
        m_SSAONoiseImage = VK_NULL_HANDLE;
        m_SSAONoiseAllocation = nullptr;
        ::eng::ResourceTracker::decImage();
    }
    if (m_SSAONoiseSampler != VK_NULL_HANDLE) {
        vkDestroySampler(resources.device, m_SSAONoiseSampler, nullptr);
        m_SSAONoiseSampler = VK_NULL_HANDLE;
    }

    for (size_t i = 0; i < m_SSAOConstantBuffers.size(); ++i) {
        vmaDestroyBuffer(resources.allocator, m_SSAOConstantBuffers[i], m_SSAOConstantAllocations[i]);
        ::eng::ResourceTracker::decBuffer();
    }
    m_SSAOConstantBuffers.clear();
    m_SSAOConstantAllocations.clear();

    if (m_SSAORenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(resources.device, m_SSAORenderPass, nullptr);
        m_SSAORenderPass = VK_NULL_HANDLE;
    }

    // Destroy offscreen targets
    for (auto h : m_SSAOBlurredHandles) m_RenderTargetManager.Destroy(h);
    m_SSAOBlurredHandles.clear();
    m_SSAOBlurredImages.clear();
    m_SSAOBlurredAllocations.clear();
    m_SSAOBlurredImageViews.clear();

    for (auto h : m_SSAOFbHandles) m_FramebufferManager.Destroy(h);
    m_SSAOFbHandles.clear();
    m_SSAOFramebuffers.clear();

    for (auto h : m_SSAOBlurredFbHandles) m_FramebufferManager.Destroy(h);
    m_SSAOBlurredFbHandles.clear();
    m_SSAOBlurredFramebuffers.clear();
}

VkDescriptorSet Renderer::GetSSAOBlurredTexture(uint32_t frameIdx) const {
    if (frameIdx >= m_SSAOBlurredImGuiTextures.size()) return VK_NULL_HANDLE;
    if (m_SSAOBlurredImGuiTextures[frameIdx] == VK_NULL_HANDLE && 
        frameIdx < m_SSAOBlurredImageViews.size() && m_SSAOBlurredImageViews[frameIdx] != VK_NULL_HANDLE && 
        m_GBufferSampler != VK_NULL_HANDLE) {
        m_SSAOBlurredImGuiTextures[frameIdx] = ImGui_ImplVulkan_AddTexture(
            m_GBufferSampler, 
            m_SSAOBlurredImageViews[frameIdx], 
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
    }
    return m_SSAOBlurredImGuiTextures[frameIdx];
}

bool Renderer::CreateGBufferPipeline()
{
    // Destroy existing
    if (m_GBufferPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(resources.device, m_GBufferPipeline, nullptr);
        m_GBufferPipeline = VK_NULL_HANDLE;
    }
    if (m_GBufferPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(resources.device, m_GBufferPipelineLayout, nullptr);
        m_GBufferPipelineLayout = VK_NULL_HANDLE;
    }

    // 1. Create GBuffer pipeline layout
    std::vector<VkDescriptorSetLayout> setLayouts = {
        resources.globalSetLayout,   // Set 0
        resources.materialSetLayout  // Set 1
    };

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(GBufferPushConstants); // instance index

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    layoutInfo.pSetLayouts = setLayouts.data();
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;

    VK_CHECK(vkCreatePipelineLayout(resources.device, &layoutInfo, nullptr, &m_GBufferPipelineLayout));

    // 2. Load Shaders
    VkShaderModule vertModule = resources.loadShaderModule("shaders/gbuffer_vert.spv");
    VkShaderModule fragModule = resources.loadShaderModule("shaders/gbuffer_frag.spv");

    if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE) {
        LOG_ERROR("Failed to load GBuffer shaders!");
        if (vertModule != VK_NULL_HANDLE) vkDestroyShaderModule(resources.device, vertModule, nullptr);
        if (fragModule != VK_NULL_HANDLE) vkDestroyShaderModule(resources.device, fragModule, nullptr);
        return false;
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

    // 3. Vertex input format (PbrVertex)
    VkVertexInputBindingDescription bindingDesc = PbrVertex::GetBindingDescription();
    auto attrDescs = PbrVertex::GetAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDescs.size());
    vertexInputInfo.pVertexAttributeDescriptions = attrDescs.data();

    // 4. Input Assembly
    VkPipelineInputAssemblyStateCreateInfo inputAsm{};
    inputAsm.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAsm.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAsm.primitiveRestartEnable = VK_FALSE;

    // 5. Viewport state (Dynamic viewport/scissor)
    VkViewport viewport{};
    viewport.x = 0.0f; viewport.y = 0.0f;
    viewport.width  = (float)resources.swapChainExtent.width;
    viewport.height = (float)resources.swapChainExtent.height;
    viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0,0}; scissor.extent = resources.swapChainExtent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // 6. Rasterization State
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = RasterConvention::CullMode;
    rasterizer.frontFace = RasterConvention::FrontFace;
    rasterizer.depthBiasEnable = VK_FALSE;

    // 7. Multisample State
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // 8. Depth Stencil State
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // 9. Blend Attachments (6 for GBuffer including ObjectID and Velocity)
    std::array<VkPipelineColorBlendAttachmentState, 6> blendAttachments{};
    for (auto& attachment : blendAttachments) {
        attachment.blendEnable = VK_FALSE;
        attachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;
    }

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = static_cast<uint32_t>(blendAttachments.size());
    colorBlending.pAttachments = blendAttachments.data();

    // 10. Dynamic State
    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicInfo{};
    dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.dynamicStateCount = 2;
    dynamicInfo.pDynamicStates = dynamicStates;

    // 11. Create Graphics Pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAsm;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicInfo;
    pipelineInfo.layout = m_GBufferPipelineLayout;
    pipelineInfo.renderPass = m_GBufferRenderPass;
    pipelineInfo.subpass = 0;

    VkResult result = vkCreateGraphicsPipelines(resources.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_GBufferPipeline.pipeline);
    if (result == VK_SUCCESS) {
        m_GBufferPipeline.name = "GBufferPipeline";
        m_GBufferPipeline.layout = m_GBufferPipelineLayout;
        m_GBufferPipeline.compatibleRenderPass = m_GBufferRenderPass;
        m_GBufferPipeline.colorAttachmentCount = 5;
        m_GBufferPipeline.vertexStride = sizeof(PbrVertex);
    }

    vkDestroyShaderModule(resources.device, vertModule, nullptr);
    vkDestroyShaderModule(resources.device, fragModule, nullptr);

    VK_CHECK(result);

    if (result == VK_SUCCESS) {
        geometryPipeline = m_GBufferPipeline.pipeline;
        return true;
    }
    return false;
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

    if (frameIndex >= m_ObjectIDImages.size() || m_ObjectIDImages[frameIndex] == VK_NULL_HANDLE) {
        return 0;
    }

    VkImage gbufferImage = m_ObjectIDImages[frameIndex];

    // Create staging buffer (4 bytes for uint32_t pixel)
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

    // Transition ObjectID layout to TRANSFER_SRC_OPTIMAL, copy, then transition back to SHADER_READ_ONLY_OPTIMAL
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

    // Entity ID is in a dedicated R32_UINT target
    uint32_t entityID = *reinterpret_cast<uint32_t*>(mappedData);

    vmaUnmapMemory(resources.allocator, stagingAllocation);
    vmaDestroyBuffer(resources.allocator, stagingBuffer, stagingAllocation);

    return entityID;
}

void Renderer::ValidateStartupState()
{
    LOG_INFO("================= Renderer Startup Validation =================");
    ValidateSwapchain();
    ValidateRenderTargets();
    ValidateFramebuffers();
    ValidatePipelines();
    ValidateDescriptorLayouts();
    ValidateRenderGraph();
    LOG_INFO("===============================================================");
}

void Renderer::ValidateSwapchain()
{
    LOG_INFO("----- ValidateSwapchain -----");
    bool valid = true;
    if (resources.swapChain == VK_NULL_HANDLE) {
        LOG_ERROR("ValidateSwapchain: Swapchain handle is NULL!");
        valid = false;
    }
    LOG_INFO("Swapchain handle: " + std::to_string((uint64_t)(uintptr_t)resources.swapChain));
    LOG_INFO("Swapchain extent: " + std::to_string(resources.swapChainExtent.width) + "x" + std::to_string(resources.swapChainExtent.height));
    LOG_INFO("Swapchain image count: " + std::to_string(resources.swapChainImages.size()));
    if (resources.swapChainExtent.width == 0 || resources.swapChainExtent.height == 0) {
        LOG_ERROR("ValidateSwapchain: Swapchain extent has width/height of 0!");
        valid = false;
    }
    if (resources.swapChainImages.empty()) {
        LOG_ERROR("ValidateSwapchain: Swapchain has 0 images!");
        valid = false;
    }
    if (valid) {
        LOG_INFO("ValidateSwapchain: SUCCESS");
    } else {
        LOG_ERROR("ValidateSwapchain: FAILED");
    }
}

void Renderer::ValidateRenderTargets()
{
    LOG_INFO("----- ValidateRenderTargets -----");
    bool valid = true;
    
    LOG_INFO("GBuffer extent: " + std::to_string(m_DepthWidth) + "x" + std::to_string(m_DepthHeight));
    LOG_INFO("Depth extent: " + std::to_string(m_DepthWidth) + "x" + std::to_string(m_DepthHeight));
    LOG_INFO("HDR target extent: " + std::to_string(m_DepthWidth) + "x" + std::to_string(m_DepthHeight));
    
    uint32_t offscreenWidth = m_ViewportRenderer.isOffscreenRenderingEnabled() ? m_ViewportRenderer.getOffscreenWidth() : resources.swapChainExtent.width;
    uint32_t offscreenHeight = m_ViewportRenderer.isOffscreenRenderingEnabled() ? m_ViewportRenderer.getOffscreenHeight() : resources.swapChainExtent.height;
    LOG_INFO("Viewport extent: " + std::to_string(offscreenWidth) + "x" + std::to_string(offscreenHeight));
    LOG_INFO("LDR target extent: " + std::to_string(offscreenWidth) + "x" + std::to_string(offscreenHeight));
    LOG_INFO("ViewportColor target extent: " + std::to_string(offscreenWidth) + "x" + std::to_string(offscreenHeight));

    for (size_t i = 0; i < m_GBufferAImages.size(); ++i) {
        if (m_GBufferAImages[i] == VK_NULL_HANDLE) {
            LOG_ERROR("GBufferA image " + std::to_string(i) + " is NULL!");
            valid = false;
        }
        if (m_GBufferBImages[i] == VK_NULL_HANDLE) {
            LOG_ERROR("GBufferB image " + std::to_string(i) + " is NULL!");
            valid = false;
        }
        if (m_GBufferCImages[i] == VK_NULL_HANDLE) {
            LOG_ERROR("GBufferC image " + std::to_string(i) + " is NULL!");
            valid = false;
        }
        if (m_GBufferDImages[i] == VK_NULL_HANDLE) {
            LOG_ERROR("GBufferD image " + std::to_string(i) + " is NULL!");
            valid = false;
        }
    }
    
    for (size_t i = 0; i < m_DepthImages.size(); ++i) {
        if (m_DepthImages[i] == VK_NULL_HANDLE) {
            LOG_ERROR("Depth image " + std::to_string(i) + " is NULL!");
            valid = false;
        }
    }
    for (size_t i = 0; i < m_HDRColorImages.size(); ++i) {
        if (m_HDRColorImages[i] == VK_NULL_HANDLE) {
            LOG_ERROR("HDR color image " + std::to_string(i) + " is NULL!");
            valid = false;
        }
    }

    if (valid) {
        LOG_INFO("ValidateRenderTargets: SUCCESS");
    } else {
        LOG_ERROR("ValidateRenderTargets: FAILED");
    }
}

void Renderer::ValidateFramebuffers()
{
    LOG_INFO("----- ValidateFramebuffers -----");
    bool valid = true;

    VkFramebuffer postProcessFb = m_ViewportRenderer.isOffscreenRenderingEnabled() ? 
        m_ViewportRenderer.getOffscreenFramebuffer(0) : 
        (resources.swapChainFramebuffers.empty() ? VK_NULL_HANDLE : resources.swapChainFramebuffers[0]);
    
    LOG_INFO("PostProcess framebuffer handle: " + std::to_string((uint64_t)(uintptr_t)postProcessFb));
    LOG_INFO("EditorOverlay framebuffer handle: " + std::to_string((uint64_t)(uintptr_t)postProcessFb));
    LOG_INFO("UIPass swapchain handle: " + std::to_string((uint64_t)(uintptr_t)resources.swapChain));

    for (size_t i = 0; i < m_GBufferFramebuffers.size(); ++i) {
        if (m_GBufferFramebuffers[i] == VK_NULL_HANDLE) {
            LOG_ERROR("GBuffer framebuffer " + std::to_string(i) + " is NULL!");
            valid = false;
        }
    }
    for (size_t i = 0; i < m_HDRColorFramebuffers.size(); ++i) {
        if (m_HDRColorFramebuffers[i] == VK_NULL_HANDLE) {
            LOG_ERROR("HDRColor framebuffer " + std::to_string(i) + " is NULL!");
            valid = false;
        }
    }
    for (size_t i = 0; i < m_DepthFramebuffers.size(); ++i) {
        if (m_DepthFramebuffers[i] == VK_NULL_HANDLE) {
            LOG_ERROR("Depth prepass framebuffer " + std::to_string(i) + " is NULL!");
            valid = false;
        }
    }

    if (valid) {
        LOG_INFO("ValidateFramebuffers: SUCCESS");
    } else {
        LOG_ERROR("ValidateFramebuffers: FAILED");
    }
}

void Renderer::ValidatePipelines()
{
    LOG_INFO("----- ValidatePipelines -----");
    bool valid = true;

    if (shadowPipeline == VK_NULL_HANDLE) {
        LOG_ERROR("ValidatePipelines: shadowPipeline is NULL!");
        valid = false;
    } else {
        LOG_INFO("shadowPipeline handle: " + std::to_string((uint64_t)(uintptr_t)shadowPipeline));
    }

    if (geometryPipeline == VK_NULL_HANDLE) {
        LOG_ERROR("ValidatePipelines: geometryPipeline is NULL!");
        valid = false;
    } else {
        LOG_INFO("geometryPipeline handle: " + std::to_string((uint64_t)(uintptr_t)geometryPipeline));
    }

    if (m_DeferredLightingPipeline == VK_NULL_HANDLE) {
        LOG_ERROR("ValidatePipelines: m_DeferredLightingPipeline is NULL!");
        valid = false;
    } else {
        LOG_INFO("m_DeferredLightingPipeline handle: " + std::to_string((uint64_t)(uintptr_t)m_DeferredLightingPipeline));
    }

    if (m_OffscreenDeferredLightingPipeline == VK_NULL_HANDLE) {
        LOG_ERROR("ValidatePipelines: m_OffscreenDeferredLightingPipeline is NULL!");
        valid = false;
    } else {
        LOG_INFO("m_OffscreenDeferredLightingPipeline handle: " + std::to_string((uint64_t)(uintptr_t)m_OffscreenDeferredLightingPipeline));
    }

    if (m_PostProcessPipeline == VK_NULL_HANDLE) {
        LOG_ERROR("ValidatePipelines: m_PostProcessPipeline is NULL!");
        valid = false;
    } else {
        LOG_INFO("m_PostProcessPipeline handle: " + std::to_string((uint64_t)(uintptr_t)m_PostProcessPipeline));
    }

    if (m_OffscreenPostProcessPipeline == VK_NULL_HANDLE) {
        LOG_ERROR("ValidatePipelines: m_OffscreenPostProcessPipeline is NULL!");
        valid = false;
    } else {
        LOG_INFO("m_OffscreenPostProcessPipeline handle: " + std::to_string((uint64_t)(uintptr_t)m_OffscreenPostProcessPipeline));
    }

    if (valid) {
        LOG_INFO("ValidatePipelines: SUCCESS");
    } else {
        LOG_ERROR("ValidatePipelines: FAILED");
    }
}

void Renderer::ValidateDescriptorLayouts()
{
    LOG_INFO("----- ValidateDescriptorLayouts -----");
    bool valid = true;

    if (m_GBufferDescriptorSetLayout == VK_NULL_HANDLE) {
        LOG_ERROR("m_GBufferDescriptorSetLayout is NULL!");
        valid = false;
    }
    if (m_PostProcessDescriptorSetLayout == VK_NULL_HANDLE) {
        LOG_ERROR("m_PostProcessDescriptorSetLayout is NULL!");
        valid = false;
    }
    if (resources.globalSetLayout == VK_NULL_HANDLE) {
        LOG_ERROR("resources.globalSetLayout is NULL!");
        valid = false;
    }
    if (resources.materialSetLayout == VK_NULL_HANDLE) {
        LOG_ERROR("resources.materialSetLayout is NULL!");
        valid = false;
    }
    if (resources.lightingSetLayout == VK_NULL_HANDLE) {
        LOG_ERROR("resources.lightingSetLayout is NULL!");
        valid = false;
    }

    if (valid) {
        LOG_INFO("ValidateDescriptorLayouts: SUCCESS");
    } else {
        LOG_ERROR("ValidateDescriptorLayouts: FAILED");
    }
}

void Renderer::ValidateRenderGraph()
{
    LOG_INFO("----- ValidateRenderGraph & Render Graph Dump -----");
    renderGraph.PrintDebug();

    LOG_INFO("Render Graph Pass Configurations:");
    
    struct PassDumpInfo {
        std::string name;
        std::string inputs;
        std::string outputs;
        std::string layoutBefore;
        std::string layoutAfter;
        std::string fbStatus;
        std::string pipelineStatus;
    };

    auto getPassInfo = [this](const std::string& name) -> PassDumpInfo {
        PassDumpInfo info;
        info.name = name;
        if (name == "ShadowPass") {
            info.inputs = "None";
            info.outputs = "ShadowMap";
            info.layoutBefore = "UNDEFINED";
            info.layoutAfter = "DEPTH_STENCIL_READ_ONLY_OPTIMAL";
            info.fbStatus = (m_ShadowFramebuffers.empty() || m_ShadowFramebuffers[0] == VK_NULL_HANDLE) ? "Invalid (NULL)" : "Valid";
            info.pipelineStatus = (shadowPipeline == VK_NULL_HANDLE) ? "Invalid (NULL)" : "Valid";
        }
        else if (name == "DepthPass") {
            info.inputs = "None";
            info.outputs = "DepthBuffer";
            info.layoutBefore = "UNDEFINED";
            info.layoutAfter = "DEPTH_STENCIL_ATTACHMENT_OPTIMAL";
            info.fbStatus = (m_DepthFramebuffers.empty() || m_DepthFramebuffers[0] == VK_NULL_HANDLE) ? "Invalid (NULL)" : "Valid";
            info.pipelineStatus = (m_DepthPipeline == VK_NULL_HANDLE) ? "Invalid (NULL)" : "Valid";
        }
        else if (name == "GBufferPass") {
            info.inputs = "None";
            info.outputs = "GBufferA, GBufferB, GBufferC, GBufferD";
            info.layoutBefore = "UNDEFINED";
            info.layoutAfter = "SHADER_READ_ONLY_OPTIMAL";
            info.fbStatus = (m_GBufferFramebuffers.empty() || m_GBufferFramebuffers[0] == VK_NULL_HANDLE) ? "Invalid (NULL)" : "Valid";
            info.pipelineStatus = (geometryPipeline == VK_NULL_HANDLE) ? "Invalid (NULL)" : "Valid";
        }
        else if (name == "LightCullingPass") {
            info.inputs = "None (Compute buffers)";
            info.outputs = "None (Compute buffers)";
            info.layoutBefore = "N/A";
            info.layoutAfter = "N/A";
            info.fbStatus = "N/A (Compute)";
            info.pipelineStatus = (m_LightCullingPipeline == VK_NULL_HANDLE) ? "Invalid (NULL)" : "Valid";
        }
        else if (name == "DeferredLightingPass") {
            info.inputs = "GBufferA, GBufferB, GBufferC, GBufferD, DepthBuffer";
            info.outputs = "HDRColor";
            info.layoutBefore = "UNDEFINED";
            info.layoutAfter = "COLOR_ATTACHMENT_OPTIMAL";
            info.fbStatus = (m_HDRColorFramebuffers.empty() || m_HDRColorFramebuffers[0] == VK_NULL_HANDLE) ? "Invalid (NULL)" : "Valid";
            info.pipelineStatus = (m_ViewportRenderer.isOffscreenRenderingEnabled() ? 
                                   (m_OffscreenDeferredLightingPipeline == VK_NULL_HANDLE ? "Invalid (NULL)" : "Valid") : 
                                   (m_DeferredLightingPipeline == VK_NULL_HANDLE ? "Invalid (NULL)" : "Valid"));
        }
        else if (name == "TransparentPass") {
            info.inputs = "HDRColor, DepthBuffer";
            info.outputs = "HDRColor";
            info.layoutBefore = "COLOR_ATTACHMENT_OPTIMAL";
            info.layoutAfter = "COLOR_ATTACHMENT_OPTIMAL";
            info.fbStatus = (m_TransparentFramebuffers.empty() || m_TransparentFramebuffers[0] == VK_NULL_HANDLE) ? "Invalid (NULL)" : "Valid";
            info.pipelineStatus = (geometryPipeline == VK_NULL_HANDLE) ? "Invalid (NULL)" : "Valid";
        }
        else if (name == "PostProcessPass") {
            info.inputs = "HDRColor";
            info.outputs = "LDRColor";
            info.layoutBefore = "COLOR_ATTACHMENT_OPTIMAL";
            info.layoutAfter = "COLOR_ATTACHMENT_OPTIMAL (or PRESENT_SRC_KHR)";
            VkFramebuffer fb = m_ViewportRenderer.isOffscreenRenderingEnabled() ? 
                m_ViewportRenderer.getOffscreenFramebuffer(0) : 
                (resources.swapChainFramebuffers.empty() ? VK_NULL_HANDLE : resources.swapChainFramebuffers[0]);
            VkPipeline pipe = m_ViewportRenderer.isOffscreenRenderingEnabled() ? 
                m_OffscreenPostProcessPipeline : m_PostProcessPipeline;
            info.fbStatus = (fb == VK_NULL_HANDLE) ? "Invalid (NULL)" : "Valid";
            info.pipelineStatus = (pipe == VK_NULL_HANDLE) ? "Invalid (NULL)" : "Valid";
        }
        else if (name == "EditorOverlayPass") {
            info.inputs = "LDRColor";
            info.outputs = "ViewportColor";
            info.layoutBefore = "COLOR_ATTACHMENT_OPTIMAL";
            info.layoutAfter = "COLOR_ATTACHMENT_OPTIMAL";
            VkFramebuffer fb = m_ViewportRenderer.isOffscreenRenderingEnabled() ? 
                m_ViewportRenderer.getOffscreenFramebuffer(0) : 
                (resources.swapChainFramebuffers.empty() ? VK_NULL_HANDLE : resources.swapChainFramebuffers[0]);
            info.fbStatus = (fb == VK_NULL_HANDLE) ? "Invalid (NULL)" : "Valid";
            info.pipelineStatus = "Valid (Stub Pass)";
        }
        else if (name == "UIPass") {
            info.inputs = "ViewportColor";
            info.outputs = "Swapchain";
            info.layoutBefore = "COLOR_ATTACHMENT_OPTIMAL";
            info.layoutAfter = "PRESENT_SRC_KHR";
            info.fbStatus = resources.swapChainFramebuffers.empty() ? "Invalid (NULL)" : "Valid";
            info.pipelineStatus = "Valid (ImGui pipeline)";
        }
        else if (name == "PresentPass") {
            info.inputs = "Swapchain";
            info.outputs = "Backbuffer";
            info.layoutBefore = "PRESENT_SRC_KHR";
            info.layoutAfter = "PRESENT_SRC_KHR";
            info.fbStatus = "Valid (Swapchain)";
            info.pipelineStatus = "Valid (Present engine)";
        }
        else {
            info.inputs = "Unknown";
            info.outputs = "Unknown";
            info.layoutBefore = "Unknown";
            info.layoutAfter = "Unknown";
            info.fbStatus = "Unknown";
            info.pipelineStatus = "Unknown";
        }
        return info;
    };

    std::vector<std::string> knownPasses = {
        "ShadowPass",
        "DepthPass",
        "GBufferPass",
        "LightCullingPass",
        "DeferredLightingPass",
        "TransparentPass",
        "PostProcessPass",
        "BloomExtractPass",
        "BloomBlurPass",
        "BloomCompositePass",
        "EditorOverlayPass",
        "UIPass",
        "PresentPass"
    };

    for (const auto& passName : knownPasses) {
        PassDumpInfo info = getPassInfo(passName);
        LOG_INFO("Pass Name: " + info.name);
        LOG_INFO("  Inputs:       " + info.inputs);
        LOG_INFO("  Outputs:      " + info.outputs);
        LOG_INFO("  Layout Before: " + info.layoutBefore);
        LOG_INFO("  Layout After:  " + info.layoutAfter);
        LOG_INFO("  Framebuffer:  " + info.fbStatus);
        LOG_INFO("  Pipeline:     " + info.pipelineStatus);
    }
}

VkDescriptorSet Renderer::GetShadowTexture(uint32_t frameIdx) const {
    if (frameIdx >= m_ShadowImGuiTextures.size()) return VK_NULL_HANDLE;
    if (m_ShadowImGuiTextures[frameIdx] == VK_NULL_HANDLE && 
        frameIdx < m_ShadowImageViews.size() && m_ShadowImageViews[frameIdx] != VK_NULL_HANDLE && 
        m_ShadowSampler != VK_NULL_HANDLE) {
        m_ShadowImGuiTextures[frameIdx] = ImGui_ImplVulkan_AddTexture(
            m_ShadowSampler, 
            m_ShadowImageViews[frameIdx], 
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
    }
    return m_ShadowImGuiTextures[frameIdx];
}

void Renderer::SetOffscreenRenderingEnabled(bool enabled) {
    m_ViewportRenderer.setOffscreenRenderingEnabled(enabled);
    if (enabled) {
        recreateOffscreenPostProcessPipeline();
    }
}

void Renderer::CreateOffscreenResources(uint32_t width, uint32_t height) {
    m_ViewportRenderer.createOffscreenResources(width, height);
    if (m_ViewportRenderer.isOffscreenRenderingEnabled()) {
        recreateOffscreenPostProcessPipeline();
    }
}

bool Renderer::CaptureScreenshot(const std::string& filename) {
    if (resources.device == VK_NULL_HANDLE) return false;

    // Wait for GPU to finish work before reading image
    vkDeviceWaitIdle(resources.device);

    uint32_t activeFrameIndex = frameIndex % resources.MAX_FRAMES_IN_FLIGHT;
    
    VkImage srcImage = VK_NULL_HANDLE;
    uint32_t width = m_DepthWidth;
    uint32_t height = m_DepthHeight;
    VkImageLayout oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    if (!m_ViewportColorHandles.empty()) {
        const RenderTarget* rt = m_RenderTargetManager.Get(m_ViewportColorHandles[activeFrameIndex]);
        if (rt) {
            srcImage = rt->image;
        }
    }

    if (srcImage == VK_NULL_HANDLE) {
        if (activeFrameIndex < resources.swapChainImages.size()) {
            srcImage = resources.swapChainImages[activeFrameIndex];
            width = resources.swapChainExtent.width;
            height = resources.swapChainExtent.height;
            oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        }
    }

    if (srcImage == VK_NULL_HANDLE) {
        LOG_ERROR("CaptureScreenshot: Source image is null!");
        return false;
    }

    VkDeviceSize imageSize = width * height * 4;
    VkBuffer stagingBuffer;
    VmaAllocation stagingAlloc;

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = imageSize;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo stagingAllocInfo{};
    stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &stagingAllocInfo, &stagingBuffer, &stagingAlloc, nullptr));

    VkCommandBuffer cmd = resources.beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = srcImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = { width, height, 1 };

    vkCmdCopyImageToBuffer(cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1, &region);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = oldLayout;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    if (oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    } else {
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    }

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    resources.endSingleTimeCommands(cmd);

    void* mappedData;
    VK_CHECK(vmaMapMemory(resources.allocator, stagingAlloc, &mappedData));

    uint8_t* pixels = static_cast<uint8_t*>(mappedData);
    VkFormat format = resources.swapChainImageFormat;
    if (!m_ViewportColorHandles.empty()) {
        const RenderTarget* rt = m_RenderTargetManager.Get(m_ViewportColorHandles[activeFrameIndex]);
        if (rt) {
            format = rt->format;
        }
    }

    if (format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_B8G8R8A8_SRGB) {
        for (uint32_t i = 0; i < width * height; ++i) {
            uint8_t temp = pixels[i * 4];
            pixels[i * 4] = pixels[i * 4 + 2];
            pixels[i * 4 + 2] = temp;
        }
    }

    int success = stbi_write_png(filename.c_str(), width, height, 4, pixels, width * 4);

    vmaUnmapMemory(resources.allocator, stagingAlloc);
    vmaDestroyBuffer(resources.allocator, stagingBuffer, stagingAlloc);

    return success != 0;
}

static float halfToFloat(uint16_t h) {
    uint32_t sign = (h & 0x8000) << 16;
    uint32_t exp  = (h & 0x7C00) >> 10;
    uint32_t mant = (h & 0x03FF);
    if (exp == 0) {
        if (mant == 0) {
            union { uint32_t u; float f; } uval;
            uval.u = sign;
            return uval.f;
        }
        while ((mant & 0x0400) == 0) {
            mant <<= 1;
            exp--;
        }
        exp++;
        mant &= ~0x0400;
    } else if (exp == 31) {
        union { uint32_t u; float f; } uval;
        uval.u = sign | 0x7F800000 | (mant << 13);
        return uval.f;
    }
    exp = exp + (127 - 15);
    union { uint32_t u; float f; } uval;
    uval.u = sign | (exp << 23) | (mant << 13);
    return uval.f;
}

void Renderer::CopyImageToCPU(VkImage image, uint32_t width, uint32_t height, std::vector<float>& outPixels) {
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation stagingAllocation = VK_NULL_HANDLE;
    
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = width * height * 8; // R16G16B16A16 = 8 bytes/pixel
    bufferInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    
    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufferInfo, &allocInfo, &stagingBuffer, &stagingAllocation, nullptr));
    ::eng::ResourceTracker::incBuffer();
    
    VkCommandBuffer cmd = resources.beginSingleTimeCommands();
    
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {width, height, 1};
    
    vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1, &region);
    
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    resources.endSingleTimeCommands(cmd);
    
    void* data = nullptr;
    VK_CHECK(vmaMapMemory(resources.allocator, stagingAllocation, &data));
    
    uint16_t* halfPixels = reinterpret_cast<uint16_t*>(data);
    outPixels.resize(width * height * 4);
    for (uint32_t p = 0; p < width * height * 4; ++p) {
        outPixels[p] = halfToFloat(halfPixels[p]);
    }
    
    vmaUnmapMemory(resources.allocator, stagingAllocation);
    vmaDestroyBuffer(resources.allocator, stagingBuffer, stagingAllocation);
}

void Renderer::RenderSceneOffscreen(
    eng::runtime::IECSWorld& world,
    const glm::mat4& customView,
    const glm::mat4& customProj,
    const glm::vec3& customCamPos
) {
    // Removed duplicate layout tracking initialization m_DepthImageLayout
    CameraComponent fakeCamera{};
    fakeCamera.fov = 90.0f;
    fakeCamera.nearPlane = 0.1f;
    fakeCamera.farPlane = 100.0f;
    
    glm::mat4 cameraWorldMatrix = glm::inverse(customView);
    RenderSceneExtractor::ExtractScene(
        world,
        m_AssetRegistry,
        fakeCamera,
        cameraWorldMatrix,
        activeRenderScene,
        m_UseEditorDefaultLighting
    );
    
    activeRenderScene.camera.viewMatrix = customView;
    activeRenderScene.camera.projectionMatrix = customProj;
    activeRenderScene.camera.position = customCamPos;
    activeRenderScene.camera.fov = 90.0f;
    activeRenderScene.camera.nearPlane = 0.1f;
    activeRenderScene.camera.farPlane = 100.0f;
    
    buildRenderQueue();
    
    float timeSeconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - m_StartTime).count();
    Omnix::Radiance::RadianceFrameUBO radianceUBO{};
    UpdateRadianceFrameUBO(radianceUBO, activeRenderScene.camera, m_DepthWidth, m_DepthHeight, timeSeconds);
    
    std::vector<RenderItem> combinedItems = renderQueue.getItems();
    combinedItems.insert(combinedItems.end(), transparentRenderQueue.getItems().begin(), transparentRenderQueue.getItems().end());
    
    gpuScene.UpdateFrame(
        resources,
        frameIndex,
        activeRenderScene,
        combinedItems,
        m_EcsMaterialCache,
        m_DefaultMaterial,
        radianceUBO,
        m_ShadingMode,
        m_ActiveScene
    );
    
    const auto& frameRes = gpuScene.GetFrameResources(frameIndex);
    activeFrameContext.uboBuffer = frameRes.cameraBuffer;
    activeFrameContext.uboDescriptor = frameRes.descriptorSet;
    activeFrameContext.descriptorPool = frameRes.descriptorPool;
    activeFrameContext.lightingUboBuffer = frameRes.lightBuffer;
    activeFrameContext.lightingDescriptor = frameRes.descriptorSet;
    
    setupRenderGraph();
    renderGraph.Compile(resources);
    
    bool graphExecutionFailed = false;
    renderGraph.ExecuteWithValidation(resources, frameIndex, m_RenderTargetManager, graphExecutionFailed);
}

void Renderer::BakeReflectionProbes(eng::runtime::IECSWorld& world) {
    auto& coordinator = world.getCoordinator();
    auto reflectionProbeType = coordinator.GetComponentType<ReflectionProbeComponent>();
    
    std::vector<Entity> probeEntities;
    for (Entity ent : coordinator.GetActiveEntities()) {
        if (!coordinator.IsEntityAlive(ent)) continue;
        if (coordinator.GetSignature(ent).test(reflectionProbeType)) {
            probeEntities.push_back(ent);
        }
    }
    
    if (probeEntities.empty()) return;
    
    uint32_t origW = m_DepthWidth;
    uint32_t origH = m_DepthHeight;
    bool origOffscreen = m_ViewportRenderer.isOffscreenRenderingEnabled();
    
    vkDeviceWaitIdle(resources.device);
    
    m_ViewportRenderer.setOffscreenRenderingEnabled(true);
    m_ViewportRenderer.destroyOffscreenResources();
    m_ViewportRenderer.createOffscreenResources(256, 256);
    recreateDepthResources(256, 256);
    
    const glm::vec3 targets[6] = {
        glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, -1.0f)
    };
    const glm::vec3 ups[6] = {
        glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f),  glm::vec3(0.0f, 0.0f, -1.0f),
        glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)
    };
    
    for (Entity ent : probeEntities) {
        auto& probe = coordinator.GetComponent<ReflectionProbeComponent>(ent);
        glm::vec3 probePos = glm::vec3(probe.position.x, probe.position.y, probe.position.z);
        if (coordinator.GetSignature(ent).test(coordinator.GetComponentType<TransformComponent>())) {
            const auto& tc = coordinator.GetComponent<TransformComponent>(ent);
            probePos += glm::vec3(tc.position.x, tc.position.y, tc.position.z);
        }
        
        std::vector<std::vector<float>> cubeFaces(6, std::vector<float>(256 * 256 * 4));
        glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
        
        for (int f = 0; f < 6; ++f) {
            glm::mat4 view = glm::lookAt(probePos, probePos + targets[f], ups[f]);
            RenderSceneOffscreen(world, view, proj, probePos);
            CopyImageToCPU(m_HDRColorImages[frameIndex], 256, 256, cubeFaces[f]);
        }
        
        EnvironmentMap convolvedMap{};
        EnvironmentSystem::Get().ProcessRawCubemap(cubeFaces, 256, convolvedMap, resources);
        
        if (probe.capturePath.empty()) {
            probe.capturePath = "Baked_Probe_" + std::to_string(ent);
        }
        
        ReflectionProbeRegistry::Get().Register(probe.capturePath, std::move(convolvedMap.prefilterCube));
    }
    
    vkDeviceWaitIdle(resources.device);
    
    m_ViewportRenderer.setOffscreenRenderingEnabled(origOffscreen);
    m_ViewportRenderer.destroyOffscreenResources();
    if (origOffscreen) {
        m_ViewportRenderer.createOffscreenResources(origW, origH);
    }
    recreateDepthResources(origW, origH);
    updateGBufferDescriptorSets();
}

void Renderer::initSSRPipeline() {
    VkDescriptorSetLayoutBinding bindings[13]{};
    for (uint32_t i = 0; i < 7; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        bindings[i].pImmutableSamplers = nullptr;
    }

    // Binding 7: output image
    bindings[7].binding = 7;
    bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[7].descriptorCount = 1;
    bindings[7].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[7].pImmutableSamplers = nullptr;

    // Binding 8: Lighting SSBO (shader declares it as a storage buffer, not UBO)
    bindings[8].binding = 8;
    bindings[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[8].descriptorCount = 1;
    bindings[8].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[8].pImmutableSamplers = nullptr;

    // Binding 9: localReflectionProbes[4]
    bindings[9].binding = 9;
    bindings[9].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[9].descriptorCount = 4;
    bindings[9].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[9].pImmutableSamplers = nullptr;

    // Binding 10: prefilterMap
    bindings[10].binding = 10;
    bindings[10].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[10].descriptorCount = 1;
    bindings[10].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[10].pImmutableSamplers = nullptr;

    // Binding 11: brdfLUT
    bindings[11].binding = 11;
    bindings[11].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[11].descriptorCount = 1;
    bindings[11].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[11].pImmutableSamplers = nullptr;

    // Binding 12: Camera Frame UBO
    bindings[12].binding = 12;
    bindings[12].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[12].descriptorCount = 1;
    bindings[12].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[12].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 13;
    layoutInfo.pBindings = bindings;
    VK_CHECK(vkCreateDescriptorSetLayout(resources.device, &layoutInfo, nullptr, &m_SSRDescriptorSetLayout));

    // Pipeline Layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_SSRDescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;
    VK_CHECK(vkCreatePipelineLayout(resources.device, &pipelineLayoutInfo, nullptr, &m_SSRPipelineLayout));

    // Load Shader & Create Pipeline
    VkShaderModule ssrComputeShader = resources.loadShaderModule("shaders/ssr.spv");
    if (ssrComputeShader != VK_NULL_HANDLE) {
        VkComputePipelineCreateInfo computePipelineInfo{};
        computePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computePipelineInfo.layout = m_SSRPipelineLayout;
        computePipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computePipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computePipelineInfo.stage.module = ssrComputeShader;
        computePipelineInfo.stage.pName = "main";

        VK_CHECK(vkCreateComputePipelines(resources.device, VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &m_SSRPipeline));
        vkDestroyShaderModule(resources.device, ssrComputeShader, nullptr);
        LOG_INFO("SSR compute pipeline created successfully.");
    } else {
        LOG_ERROR("Failed to load compute shader: shaders/ssr.spv");
    }

    // Allocate Descriptor pool
    uint32_t maxFrames = resources.MAX_FRAMES_IN_FLIGHT;
    VkDescriptorPoolSize poolSizes[4]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = maxFrames * 14;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = maxFrames * 1;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[2].descriptorCount = maxFrames * 2;
    // Binding 8 is a STORAGE_BUFFER (lighting SSBO) — pool must include STORAGE_BUFFER.
    poolSizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[3].descriptorCount = maxFrames * 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 4;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = maxFrames;
    VK_CHECK(vkCreateDescriptorPool(resources.device, &poolInfo, nullptr, &m_SSRDescriptorPool));

    m_SSRDescriptorSets.resize(maxFrames);
    std::vector<VkDescriptorSetLayout> layouts(maxFrames, m_SSRDescriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_SSRDescriptorPool;
    allocInfo.descriptorSetCount = maxFrames;
    allocInfo.pSetLayouts = layouts.data();
    VK_CHECK(vkAllocateDescriptorSets(resources.device, &allocInfo, m_SSRDescriptorSets.data()));
}

void Renderer::destroySSRPipeline() {
    if (resources.device == VK_NULL_HANDLE) return;

    if (m_SSRPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(resources.device, m_SSRPipeline, nullptr);
        m_SSRPipeline = VK_NULL_HANDLE;
    }
    if (m_SSRPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(resources.device, m_SSRPipelineLayout, nullptr);
        m_SSRPipelineLayout = VK_NULL_HANDLE;
    }
    if (m_SSRDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(resources.device, m_SSRDescriptorPool, nullptr);
        m_SSRDescriptorPool = VK_NULL_HANDLE;
    }
    if (m_SSRDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(resources.device, m_SSRDescriptorSetLayout, nullptr);
        m_SSRDescriptorSetLayout = VK_NULL_HANDLE;
    }
}

void Renderer::initExposurePipeline() {
    if (resources.device == VK_NULL_HANDLE) return;

    // Create Descriptor Set Layout
    VkDescriptorSetLayoutBinding bindings[2]{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[0].pImmutableSamplers = nullptr;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings = bindings;
    VK_CHECK(vkCreateDescriptorSetLayout(resources.device, &layoutInfo, nullptr, &m_ExposureDescriptorSetLayout));

    // Push Constants
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(float) * 5;

    // Pipeline Layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_ExposureDescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    VK_CHECK(vkCreatePipelineLayout(resources.device, &pipelineLayoutInfo, nullptr, &m_ExposurePipelineLayout));

    // Load Shader & Create Pipeline
    VkShaderModule exposureComputeShader = resources.loadShaderModule("shaders/exposure.spv");
    if (exposureComputeShader != VK_NULL_HANDLE) {
        VkComputePipelineCreateInfo computePipelineInfo{};
        computePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computePipelineInfo.layout = m_ExposurePipelineLayout;
        computePipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computePipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computePipelineInfo.stage.module = exposureComputeShader;
        computePipelineInfo.stage.pName = "main";

        VK_CHECK(vkCreateComputePipelines(resources.device, VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &m_ExposurePipeline));
        vkDestroyShaderModule(resources.device, exposureComputeShader, nullptr);
        LOG_INFO("Exposure compute pipeline created successfully.");
    } else {
        LOG_ERROR("Failed to load compute shader: shaders/exposure.spv");
    }

    // Create Exposure Buffer (Storage Buffer)
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = sizeof(float) * 2;
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    VK_CHECK(vmaCreateBuffer(resources.allocator, &bufInfo, &allocInfo, &m_ExposureBuffer, &m_ExposureBufferAllocation, nullptr));

    // Write initial exposure values: [1.0f, 1.0f]
    void* data = nullptr;
    VK_CHECK(vmaMapMemory(resources.allocator, m_ExposureBufferAllocation, &data));
    float* fData = static_cast<float*>(data);
    fData[0] = 1.0f;
    fData[1] = 1.0f;
    vmaUnmapMemory(resources.allocator, m_ExposureBufferAllocation);

    // Allocate Descriptor Pool
    uint32_t maxFrames = resources.MAX_FRAMES_IN_FLIGHT;
    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = maxFrames;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = maxFrames;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = maxFrames;
    VK_CHECK(vkCreateDescriptorPool(resources.device, &poolInfo, nullptr, &m_ExposureDescriptorPool));

    m_ExposureDescriptorSets.resize(maxFrames);
    std::vector<VkDescriptorSetLayout> layouts(maxFrames, m_ExposureDescriptorSetLayout);
    VkDescriptorSetAllocateInfo allocSetInfo{};
    allocSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocSetInfo.descriptorPool = m_ExposureDescriptorPool;
    allocSetInfo.descriptorSetCount = maxFrames;
    allocSetInfo.pSetLayouts = layouts.data();
    VK_CHECK(vkAllocateDescriptorSets(resources.device, &allocSetInfo, m_ExposureDescriptorSets.data()));
}

void Renderer::destroyExposurePipeline() {
    if (resources.device == VK_NULL_HANDLE) return;

    if (m_ExposureBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(resources.allocator, m_ExposureBuffer, m_ExposureBufferAllocation);
        m_ExposureBuffer = VK_NULL_HANDLE;
    }
    if (m_ExposurePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(resources.device, m_ExposurePipeline, nullptr);
        m_ExposurePipeline = VK_NULL_HANDLE;
    }
    if (m_ExposurePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(resources.device, m_ExposurePipelineLayout, nullptr);
        m_ExposurePipelineLayout = VK_NULL_HANDLE;
    }
    if (m_ExposureDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(resources.device, m_ExposureDescriptorPool, nullptr);
        m_ExposureDescriptorPool = VK_NULL_HANDLE;
    }
    if (m_ExposureDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(resources.device, m_ExposureDescriptorSetLayout, nullptr);
        m_ExposureDescriptorSetLayout = VK_NULL_HANDLE;
    }
}

void Renderer::updateExposureDescriptorSets() {
    if (resources.device == VK_NULL_HANDLE) return;

    uint32_t maxFrames = resources.MAX_FRAMES_IN_FLIGHT;
    for (uint32_t i = 0; i < maxFrames; ++i) {
        if (m_HDRColorImageViews[i] == VK_NULL_HANDLE || m_ExposureBuffer == VK_NULL_HANDLE) continue;

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_HDRColorImageViews[i];
        imageInfo.sampler = m_GBufferSampler;

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_ExposureBuffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(float) * 2;

        std::array<VkWriteDescriptorSet, 2> writes{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = m_ExposureDescriptorSets[i];
        writes[0].dstBinding = 0;
        writes[0].dstArrayElement = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].descriptorCount = 1;
        writes[0].pImageInfo = &imageInfo;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = m_ExposureDescriptorSets[i];
        writes[1].dstBinding = 1;
        writes[1].dstArrayElement = 0;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[1].descriptorCount = 1;
        writes[1].pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(resources.device, 2, writes.data(), 0, nullptr);
    }
}

void Renderer::initTAAPipeline() {
    if (resources.device == VK_NULL_HANDLE) return;

    // Create Descriptor Set Layout
    VkDescriptorSetLayoutBinding bindings[5]{};
    for (uint32_t i = 0; i < 4; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        bindings[i].pImmutableSamplers = nullptr;
    }
    bindings[4].binding = 4;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[4].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 5;
    layoutInfo.pBindings = bindings;
    VK_CHECK(vkCreateDescriptorSetLayout(resources.device, &layoutInfo, nullptr, &m_TAADescriptorSetLayout));

    // Push Constants (prevViewProjection mat4, blendAlpha float)
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(float) * 20;

    VkDescriptorSetLayout setLayouts[2] = {
        m_TAADescriptorSetLayout,
        gpuScene.GetDescriptorSetLayout()
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 2;
    pipelineLayoutInfo.pSetLayouts = setLayouts;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    VK_CHECK(vkCreatePipelineLayout(resources.device, &pipelineLayoutInfo, nullptr, &m_TAAPipelineLayout));

    // Load Shader Module & Create Compute Pipeline
    VkShaderModule taaComputeShader = resources.loadShaderModule("shaders/taa.spv");
    if (taaComputeShader != VK_NULL_HANDLE) {
        VkComputePipelineCreateInfo computePipelineInfo{};
        computePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computePipelineInfo.layout = m_TAAPipelineLayout;
        computePipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computePipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computePipelineInfo.stage.module = taaComputeShader;
        computePipelineInfo.stage.pName = "main";

        VK_CHECK(vkCreateComputePipelines(resources.device, VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &m_TAAPipeline));
        vkDestroyShaderModule(resources.device, taaComputeShader, nullptr);
        LOG_INFO("TAA compute pipeline created successfully.");
    } else {
        LOG_ERROR("Failed to load compute shader: shaders/taa.spv");
    }

    // Allocate Descriptor Pool
    uint32_t maxFrames = resources.MAX_FRAMES_IN_FLIGHT;
    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = maxFrames * 2 * 4;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = maxFrames * 2 * 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = maxFrames * 2;
    VK_CHECK(vkCreateDescriptorPool(resources.device, &poolInfo, nullptr, &m_TAADescriptorPool));

    m_TAADescriptorSets.resize(maxFrames * 2);
    std::vector<VkDescriptorSetLayout> layouts(maxFrames * 2, m_TAADescriptorSetLayout);
    VkDescriptorSetAllocateInfo allocSetInfo{};
    allocSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocSetInfo.descriptorPool = m_TAADescriptorPool;
    allocSetInfo.descriptorSetCount = maxFrames * 2;
    allocSetInfo.pSetLayouts = layouts.data();
    VK_CHECK(vkAllocateDescriptorSets(resources.device, &allocSetInfo, m_TAADescriptorSets.data()));
}

void Renderer::destroyTAAPipeline() {
    if (resources.device == VK_NULL_HANDLE) return;

    recreateTAAResources(0, 0);

    if (m_TAAPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(resources.device, m_TAAPipeline, nullptr);
        m_TAAPipeline = VK_NULL_HANDLE;
    }
    if (m_TAAPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(resources.device, m_TAAPipelineLayout, nullptr);
        m_TAAPipelineLayout = VK_NULL_HANDLE;
    }
    if (m_TAADescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(resources.device, m_TAADescriptorPool, nullptr);
        m_TAADescriptorPool = VK_NULL_HANDLE;
    }
    if (m_TAADescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(resources.device, m_TAADescriptorSetLayout, nullptr);
        m_TAADescriptorSetLayout = VK_NULL_HANDLE;
    }
}

void Renderer::recreateTAAResources(uint32_t width, uint32_t height) {
    if (resources.device == VK_NULL_HANDLE) return;

    for (uint32_t h = 0; h < 2; ++h) {
        if (m_TAAHistoryImages[h] != VK_NULL_HANDLE) {
            vkDestroyImageView(resources.device, m_TAAHistoryImageViews[h], nullptr);
            vmaDestroyImage(resources.allocator, m_TAAHistoryImages[h], m_TAAHistoryAllocations[h]);
            m_TAAHistoryImages[h] = VK_NULL_HANDLE;
            m_TAAHistoryImageViews[h] = VK_NULL_HANDLE;
            m_TAAHistoryAllocations[h] = nullptr;
        }
    }

    // Reset init flag: new images start in UNDEFINED layout.
    m_TAAInitialized = false;
    m_TAAHistoryIndex = 0;

    if (width == 0 || height == 0) return;

    VkFormat hdrFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

    for (uint32_t h = 0; h < 2; ++h) {
        VkImageCreateInfo imgInfo{};
        imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.imageType = VK_IMAGE_TYPE_2D;
        imgInfo.format = hdrFormat;
        imgInfo.extent.width = width;
        imgInfo.extent.height = height;
        imgInfo.extent.depth = 1;
        imgInfo.mipLevels = 1;
        imgInfo.arrayLayers = 1;
        imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VK_CHECK(vmaCreateImage(resources.allocator, &imgInfo, &allocInfo, &m_TAAHistoryImages[h], &m_TAAHistoryAllocations[h], nullptr));

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_TAAHistoryImages[h];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = hdrFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(resources.device, &viewInfo, nullptr, &m_TAAHistoryImageViews[h]));
    }
}

void Renderer::updateTAADescriptorSets() {
    if (resources.device == VK_NULL_HANDLE) return;

    uint32_t maxFrames = resources.MAX_FRAMES_IN_FLIGHT;
    for (uint32_t i = 0; i < maxFrames; ++i) {
        for (uint32_t h = 0; h < 2; ++h) {
            uint32_t readIndex = h;
            uint32_t writeIndex = 1 - h;

            if (m_HDRColorImageViews[i] == VK_NULL_HANDLE ||
                m_TAAHistoryImageViews[readIndex] == VK_NULL_HANDLE ||
                m_TAAHistoryImageViews[writeIndex] == VK_NULL_HANDLE ||
                m_GBufferVelocityImageViews[i] == VK_NULL_HANDLE ||
                m_DepthImageViews[i] == VK_NULL_HANDLE) {
                continue;
            }

            VkDescriptorImageInfo currentInfo{};
            currentInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            currentInfo.imageView = m_HDRColorImageViews[i];
            currentInfo.sampler = m_GBufferSampler;

            VkDescriptorImageInfo historyInfo{};
            historyInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            historyInfo.imageView = m_TAAHistoryImageViews[readIndex];
            historyInfo.sampler = m_GBufferSampler;

            VkDescriptorImageInfo velocityInfo{};
            velocityInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            velocityInfo.imageView = m_GBufferVelocityImageViews[i];
            velocityInfo.sampler = m_GBufferSampler;

            VkDescriptorImageInfo depthInfo{};
            depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            depthInfo.imageView = m_DepthImageViews[i];
            depthInfo.sampler = m_GBufferSampler;

            VkDescriptorImageInfo outputInfo{};
            outputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            outputInfo.imageView = m_TAAHistoryImageViews[writeIndex];

            std::array<VkWriteDescriptorSet, 5> writes{};
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = m_TAADescriptorSets[i * 2 + h];
            writes[0].dstBinding = 0;
            writes[0].dstArrayElement = 0;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[0].descriptorCount = 1;
            writes[0].pImageInfo = &currentInfo;

            writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet = m_TAADescriptorSets[i * 2 + h];
            writes[1].dstBinding = 1;
            writes[1].dstArrayElement = 0;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[1].descriptorCount = 1;
            writes[1].pImageInfo = &historyInfo;

            writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[2].dstSet = m_TAADescriptorSets[i * 2 + h];
            writes[2].dstBinding = 2;
            writes[2].dstArrayElement = 0;
            writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[2].descriptorCount = 1;
            writes[2].pImageInfo = &velocityInfo;

            writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[3].dstSet = m_TAADescriptorSets[i * 2 + h];
            writes[3].dstBinding = 3;
            writes[3].dstArrayElement = 0;
            writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[3].descriptorCount = 1;
            writes[3].pImageInfo = &depthInfo;

            writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[4].dstSet = m_TAADescriptorSets[i * 2 + h];
            writes[4].dstBinding = 4;
            writes[4].dstArrayElement = 0;
            writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            writes[4].descriptorCount = 1;
            writes[4].pImageInfo = &outputInfo;

            vkUpdateDescriptorSets(resources.device, 5, writes.data(), 0, nullptr);
        }
    }
}

} // namespace eng::renderer
