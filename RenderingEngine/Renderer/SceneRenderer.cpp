#include "Core/pch.h"
#include "SceneRenderer.h"
#include "Pass.h"
#include "LightingUBO.h"
#include "Core/Vulkan/VkUtils.h"
#include "Core/Engine/EngineResources.h"
#include "Core/Engine/VmaHelpers.h"
#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/imgui/backends/imgui_impl_vulkan.h"
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Fix collisions between Core Logger and Rendering Logger
#define OMNIX_DONT_DEFINE_GLOBAL_LOG_MACROS
#include "Core/World.h"
#include "ECS/ECSComponents.h"
#include "ECS/LightCollectionSystem.h"
#include "Runtime/Public/AssetRegistry.h"
#include "Runtime/Public/OmnixMaterialFormat.h"
#include "Runtime/Public/World/ZoneEntityComponent.h"

namespace eng::renderer {

static std::vector<char> ReadFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        ::Logger::Log(::LogLevel::Error, "Failed to open shader file: " + filename);
        return {};
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

VkShaderModule CreateShaderModule(VkDevice device, const std::vector<char>& code) {
    if (code.empty()) return VK_NULL_HANDLE;
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return shaderModule;
}

void SceneRenderer::init()
{
    // 1️⃣ Geometry pipeline is already created by the old engine (PyramidRenderer)
    geometryPipeline = resources.graphicsPipeline;

    // 2️⃣ Create pipelines for the other passes (stubs)
    initPipelines();

    // 3️⃣ Build the render‑graph (passes are recorded later each frame)
    setupRenderGraph();

    // 4️⃣ Create default fallback assets for ECS rendering
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
    bool ok = m_DefaultMaterial->create("shaders/pbr_vert.spv", 
                                        "shaders/pbr_frag.spv", 
                                        "textures/brick_albedo.png", 
                                        "textures/brick_normal.png", 
                                        resources);
    if (!ok) {
        ::Logger::Log(::LogLevel::Error, "Failed to create default material. Falling back to geometry pipeline.");
        m_DefaultMaterial->setFallbackPipeline(resources.graphicsPipeline);
    }

    ::Logger::Log(::LogLevel::Info, "SceneRenderer fully initialized - " + std::to_string(PASS_COUNT) + " passes ready.");
}

void SceneRenderer::initPipelines()
{
    auto createPipeline = [&](const char* vertPath,
                              const char* fragPath,
                              VkRenderPass rp,
                              VkPipelineLayout layout,
                              VkPipeline* outPipeline)
    {
        // 1️⃣ Load shaders – guard against missing files
        VkShaderModule vertModule = resources.loadShaderModule(vertPath);
        VkShaderModule fragModule = resources.loadShaderModule(fragPath);

        if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE) {
            ::Logger::Log(::LogLevel::Error, std::string("Stub pipeline creation failed (shader missing) – falling back to geometry pipeline: ")
                       + vertPath + " / " + fragPath);
            
            // Fallback: Use the already working geometry pipeline
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

    // Stub pipelines
    createPipeline("shadow_vert.spv", "shadow_frag.spv", resources.renderPass, resources.pipelineLayout, &shadowPipeline);
    createPipeline("fullscreen_vert.spv", "lighting_frag.spv", resources.renderPass, resources.pipelineLayout, &lightingPipeline);
    createPipeline("fullscreen_vert.spv", "postprocess_frag.spv", resources.renderPass, resources.pipelineLayout, &postProcessPipeline);
}

void SceneRenderer::setupRenderGraph()
{
    renderGraph.clear();

    // 0️⃣ Shadow Pass (stub)
    renderGraph.addPass(Pass{
        "ShadowPass",
        [this]() {
            VkCommandBuffer cmd = resources.commandBuffers[frameIndex][static_cast<size_t>(PassID::Shadow)];
            VkCommandBufferBeginInfo begin{};
            begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            VK_CHECK(vkBeginCommandBuffer(cmd, &begin));

            ::eng::DebugLabel::Begin(cmd, "Shadow Pass");
            ::Logger::Log(::LogLevel::Debug, "ShadowPass: stub - no drawing performed.");
            ::eng::DebugLabel::End(cmd);
            VK_CHECK(vkEndCommandBuffer(cmd));
        }
    });

    // 1️⃣ Geometry Pass (real)
    renderGraph.addPass(Pass{
        "GeometryPass",
        [this]() {
            VkCommandBuffer cmd = resources.commandBuffers[frameIndex][static_cast<size_t>(PassID::Geometry)];
            VkCommandBufferBeginInfo begin{};
            begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            VK_CHECK(vkBeginCommandBuffer(cmd, &begin));

            ::eng::DebugLabel::Begin(cmd, "Geometry Pass");

            VkRenderPassBeginInfo rpInfo{};
            rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            if (m_OffscreenRenderingEnabled) {
                rpInfo.renderPass = m_OffscreenRenderPass;
                rpInfo.framebuffer = m_OffscreenFramebuffers[frameIndex];
                rpInfo.renderArea.extent = { m_OffscreenWidth, m_OffscreenHeight };
            } else {
                rpInfo.renderPass = resources.renderPass;
                rpInfo.framebuffer = resources.swapChainFramebuffers[currentSwapchainImageIndex];
                rpInfo.renderArea.extent = resources.swapChainExtent;
            }

            VkClearValue clearVals[1];
            clearVals[0].color = {{0.035f, 0.040f, 0.050f, 1.0f}};
            rpInfo.clearValueCount = 1;
            rpInfo.pClearValues = clearVals;

            vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

            // ---- Set dynamic viewport and scissor (required by the pipeline) ----
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = m_OffscreenRenderingEnabled ? static_cast<float>(m_OffscreenWidth) : static_cast<float>(resources.swapChainExtent.width);
            viewport.height = m_OffscreenRenderingEnabled ? static_cast<float>(m_OffscreenHeight) : static_cast<float>(resources.swapChainExtent.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(cmd, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = m_OffscreenRenderingEnabled ? VkExtent2D{m_OffscreenWidth, m_OffscreenHeight} : resources.swapChainExtent;
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            // Bind global camera UBO
            const FrameData& fd = resources.perFrameData[frameIndex];
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.pipelineLayout, 0, 1, &fd.uboDescriptor, 0, nullptr);

            // Bind lighting set (set = 2)
            const auto& lightingData = resources.perFrameLightingData[frameIndex];
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.pipelineLayout, 2, 1, &lightingData.descriptor, 0, nullptr);

            for (const RenderItem& item : renderQueue.getItems())
            {
                if (resources.pipelineLayout != VK_NULL_HANDLE) {
                    vkCmdPushConstants(cmd, resources.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), glm::value_ptr(item.transform));
                }
                item.material->bind(cmd, resources.pipelineLayout);
                item.mesh->bind(cmd);
                vkCmdDrawIndexed(cmd, item.mesh->getIndexCount(), 1, 0, 0, 0);
            }


            vkCmdEndRenderPass(cmd);
            ::eng::DebugLabel::End(cmd);
            VK_CHECK(vkEndCommandBuffer(cmd));
        }
    });

    // 2️⃣ Lighting Pass (stub)
    renderGraph.addPass(Pass{
        "LightingPass",
        [this]() {
            VkCommandBuffer cmd = resources.commandBuffers[frameIndex][static_cast<size_t>(PassID::Lighting)];
            VkCommandBufferBeginInfo begin{};
            begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            VK_CHECK(vkBeginCommandBuffer(cmd, &begin));
            ::eng::DebugLabel::Begin(cmd, "Lighting Pass");
            ::eng::DebugLabel::End(cmd);
            VK_CHECK(vkEndCommandBuffer(cmd));
        }
    });

    // 3️⃣ Post-process Pass (stub)
    renderGraph.addPass(Pass{
        "PostProcessPass",
        [this]() {
            VkCommandBuffer cmd = resources.commandBuffers[frameIndex][static_cast<size_t>(PassID::PostProcess)];
            VkCommandBufferBeginInfo begin{};
            begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            VK_CHECK(vkBeginCommandBuffer(cmd, &begin));
            ::eng::DebugLabel::Begin(cmd, "Post-process Pass");
            ::eng::DebugLabel::End(cmd);
            VK_CHECK(vkEndCommandBuffer(cmd));
        }
    });

    // 4️⃣ UI Pass (empty stub)
    renderGraph.addPass(Pass{
        "UIPass",
        [this]() {
            VkCommandBuffer cmd = resources.commandBuffers[frameIndex][static_cast<size_t>(PassID::UI)];
            VkCommandBufferBeginInfo begin{};
            begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            VK_CHECK(vkBeginCommandBuffer(cmd, &begin));

            ::eng::DebugLabel::Begin(cmd, "UI Pass");

            // Transition the swapchain image layout from UNDEFINED to PRESENT_SRC_KHR if offscreen rendering is enabled,
            // as GeometryPass will not have written to/transitioned the swapchain image, but UIRenderPass expects PRESENT_SRC_KHR.
            if (m_OffscreenRenderingEnabled && currentSwapchainImageIndex < resources.swapChainImages.size()) {
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
                barrier.dstAccessMask = 0;

                vkCmdPipelineBarrier(cmd,
                                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                     0,
                                     0, nullptr,
                                     0, nullptr,
                                     1, &barrier);
            }

            if (resources.uiCallback) {
                resources.uiCallback(cmd, currentSwapchainImageIndex);
            }
            ::eng::DebugLabel::End(cmd);
            VK_CHECK(vkEndCommandBuffer(cmd));
        }
    });
}

void SceneRenderer::drawFrame()
{
    // --------------------------------------------------------------
    // 0️⃣ Wait for the *previous* frame to finish (fence)
    vkWaitForFences(resources.device, 1,
                    &resources.inFlightFences.at(frameIndex),
                    VK_TRUE, UINT64_MAX);

    // --------------------------------------------------------------
    // 1️⃣ Acquire next swap‑chain image → signals imageAvailable[frameIndex]
    VkResult result = vkAcquireNextImageKHR(resources.device,
                                            resources.swapChain,
                                            UINT64_MAX,
                                            resources.imageAvailableSemaphores.at(frameIndex),
                                            VK_NULL_HANDLE,
                                            &currentSwapchainImageIndex);
    
    bool swapchainNeedsRecreation = false;
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        if (recreateSwapChainCallback) {
            recreateSwapChainCallback();
        } else {
            onWindowResized();
        }
        return;
    } else if (result == VK_SUBOPTIMAL_KHR) {
        swapchainNeedsRecreation = true;
    } else if (result != VK_SUCCESS) {
        VK_CHECK(result);
    }

    // --------------------------------------------------------------
    // 2️⃣ Reset the command pool (cheapest way to free all per‑pass buffers)
    vkResetCommandPool(resources.device,
                       resources.commandPools.at(frameIndex),
                       VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);

    // --------------------------------------------------------------
    // 3️⃣ Build render‑queue and update camera UBO (Phase 5 code)
    buildRenderQueue();
    updateGlobalUBO();
    updateLightingUBO();

    // --------------------------------------------------------------
    // 4️⃣ Record *all* passes (the graph just calls the lambdas)
    renderGraph.execute();

    // --------------------------------------------------------------
    // 5️⃣ Gather the per‑pass command buffers
    const auto& perPassCmds = resources.commandBuffers.at(frameIndex);
    std::vector<VkCommandBuffer> submitCmds;
    for (auto cmd : perPassCmds) {
        if (cmd != VK_NULL_HANDLE) submitCmds.push_back(cmd);
    }

    // --------------------------------------------------------------
    // 6️⃣ Submit *one* VkSubmitInfo that submits the whole batch
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

    // Reset fence *before* submitting
    vkResetFences(resources.device, 1, &resources.inFlightFences.at(frameIndex));
    VK_CHECK(vkQueueSubmit(resources.graphicsQueue,
                            1,
                            &submitInfo,
                            resources.inFlightFences.at(frameIndex)));

    // --------------------------------------------------------------
    // 7️⃣ Present – wait on the renderFinished semaphore
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &signalSem;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &resources.swapChain;
    presentInfo.pImageIndices      = &currentSwapchainImageIndex;

    result = vkQueuePresentKHR(resources.presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || swapchainNeedsRecreation) {
        if (recreateSwapChainCallback) {
            recreateSwapChainCallback();
        } else {
            onWindowResized();
        }
    } else {
        VK_CHECK(result);
    }

    // --------------------------------------------------------------
    // 8️⃣ Advance frame index (wrap)
    frameIndex = (frameIndex + 1) % resources.MAX_FRAMES_IN_FLIGHT;

    timer.tick();
}

void SceneRenderer::cleanup()
{
    DestroyOffscreenResources();

    if (shadowPipeline) vkDestroyPipeline(resources.device, shadowPipeline, nullptr);
    if (lightingPipeline) vkDestroyPipeline(resources.device, lightingPipeline, nullptr);
    if (postProcessPipeline) vkDestroyPipeline(resources.device, postProcessPipeline, nullptr);
    
    scene.clearObjects();
}

void SceneRenderer::onWindowResized()
{
    ::Logger::Log(::LogLevel::Info, "Swapchain out of date or window resized – recreating");
    resources.recreateSwapChain();
    setupRenderGraph();
}

void SceneRenderer::buildRenderQueue()
{
    renderQueue.clear();
    m_StaticRenderCount = 0;
    m_EcsRenderCount = 0;
    m_TotalRenderCount = 0;
    
    // 1. Static loaded scene models
    for (const RenderObject& ro : scene.getObjects()) {
        RenderItem item{};
        item.mesh = ro.mesh;
        item.material = ro.material;
        item.transform = ro.transform;
        renderQueue.push_back(item);
        m_StaticRenderCount++;
    }

    // 2. Dynamic ECS entities
    if (m_World) {
        auto& coordinator = m_World->getCoordinator();
        const auto& entities = coordinator.GetActiveEntities();
        for (Entity entity : entities) {
            if (entity == 0 || !coordinator.IsEntityAlive(entity)) {
                continue;
            }

            Signature sig = coordinator.GetSignature(entity);
            auto transformType = coordinator.GetComponentType<TransformComponent>();
            auto meshRendererType = coordinator.GetComponentType<MeshRendererComponent>();
            
            if (sig.test(transformType) && sig.test(meshRendererType)) {
                // Check if ZoneEntityComponent is attached and simulating is false (hide renderables from inactive zones)
                auto zoneEntityCompType = coordinator.GetComponentType<eng::runtime::ZoneEntityComponent>();
                if (sig.test(zoneEntityCompType)) {
                    const auto& zec = coordinator.GetComponent<eng::runtime::ZoneEntityComponent>(entity);
                    if (!zec.simulating) {
                        continue;
                    }
                }

                auto& transform = coordinator.GetComponent<TransformComponent>(entity);
                auto& meshRenderer = coordinator.GetComponent<MeshRendererComponent>(entity);

                if (meshRenderer.visible) {
                    if (transform.dirty) {
                        transform.worldMatrix = Matrix4x4::TRS(transform.position, transform.rotation, transform.scale);
                        transform.dirty = false;
                    }

                    if (m_DefaultMesh == nullptr || m_DefaultMaterial == nullptr) {
                        continue;
                    }

                    RenderItem item{};
                    item.mesh = m_DefaultMesh;
                    item.material = m_DefaultMaterial;

                    // 1. Resolve Custom Mesh Asset if RenderableMeshComponent is present
                    auto renderableMeshType = coordinator.GetComponentType<RenderableMeshComponent>();
                    if (sig.test(renderableMeshType)) {
                        const auto& rm = coordinator.GetComponent<RenderableMeshComponent>(entity);
                        if (rm.meshAssetHandle.IsValid()) {
                            uint64_t handleVal = rm.meshAssetHandle.value;
                            auto meshIt = m_EcsMeshCache.find(handleVal);
                            if (meshIt != m_EcsMeshCache.end()) {
                                item.mesh = meshIt->second;
                            } else if (m_EcsWarningHandles.find(handleVal) == m_EcsWarningHandles.end()) {
                                if (m_AssetRegistry) {
                                    const auto* meta = m_AssetRegistry->GetMetadata(rm.meshAssetHandle);
                                    if (meta && meta->type == AssetType::Mesh) {
                                        Mesh* loadedMesh = scene.createMeshFromOBJ(meta->sourcePath, resources);
                                        if (loadedMesh) {
                                            m_EcsMeshCache[handleVal] = loadedMesh;
                                            item.mesh = loadedMesh;
                                            m_EcsAssignedMeshCount++;
                                        } else {
                                            m_EcsWarningHandles.insert(handleVal);
                                            ::Logger::Log(::LogLevel::Warn, "[SceneRenderer] Failed to load mesh asset from path: " + meta->sourcePath + " (falling back to default)");
                                            m_EcsFallbackMeshCount++;
                                        }
                                    } else {
                                        m_EcsWarningHandles.insert(handleVal);
                                        ::Logger::Log(::LogLevel::Warn, "[SceneRenderer] Mesh asset handle " + std::to_string(handleVal) + " not found or wrong type in registry (falling back to default)");
                                        m_EcsFallbackMeshCount++;
                                    }
                                }
                            }
                        }
                    }

                    // 2. Resolve Custom Material Asset if MaterialComponent is present
                    auto materialType = coordinator.GetComponentType<MaterialComponent>();
                    if (sig.test(materialType)) {
                        const auto& mc = coordinator.GetComponent<MaterialComponent>(entity);
                        if (mc.materialAssetHandle.IsValid()) {
                            uint64_t handleVal = mc.materialAssetHandle.value;
                            auto matIt = m_EcsMaterialCache.find(handleVal);
                            if (matIt != m_EcsMaterialCache.end()) {
                                item.material = matIt->second;
                            } else if (m_EcsWarningHandles.find(handleVal) == m_EcsWarningHandles.end()) {
                                if (m_AssetRegistry) {
                                    const auto* meta = m_AssetRegistry->GetMetadata(mc.materialAssetHandle);
                                    if (meta && meta->type == AssetType::Material) {
                                        // Deserialize the .omnixmat file to read the texture paths set by the user
                                        OmnixMaterial omnixMat;
                                        std::string albedoPath;
                                        std::string normalPath;

                                        if (DeserializeMaterial(omnixMat, meta->sourcePath)) {
                                            albedoPath = omnixMat.albedoTexturePath;
                                            normalPath = omnixMat.normalTexturePath;
                                        }

                                        // Legacy fallback: if the material has no texture paths set,
                                        // use the old name-based heuristic so existing scenes still work
                                        if (albedoPath.empty()) {
                                            if (meta->sourcePath.find("wood") != std::string::npos) {
                                                albedoPath = "textures/wood_albedo.png";
                                            } else {
                                                albedoPath = "textures/brick_albedo.png";
                                                normalPath = "textures/brick_normal.png";
                                            }
                                        }

                                        Material* loadedMat = scene.createMaterial();
                                        bool ok = loadedMat->create("shaders/pbr_vert.spv", 
                                                                    "shaders/pbr_frag.spv", 
                                                                    albedoPath, 
                                                                    normalPath, 
                                                                    resources);
                                        if (ok) {
                                            m_EcsMaterialCache[handleVal] = loadedMat;
                                            item.material = loadedMat;
                                        } else {
                                            scene.destroyMaterial(loadedMat);
                                            m_EcsWarningHandles.insert(handleVal);
                                            ::Logger::Log(::LogLevel::Warn, "[SceneRenderer] Failed to load/create material asset from path: " + meta->sourcePath + " (falling back to default)");
                                        }
                                    } else {
                                        m_EcsWarningHandles.insert(handleVal);
                                        ::Logger::Log(::LogLevel::Warn, "[SceneRenderer] Material asset handle " + std::to_string(handleVal) + " not found or wrong type in registry (falling back to default)");
                                    }
                                }
                            }
                        }
                    }

                    // Copy Column-major Matrix4x4 to glm::mat4
                    glm::mat4 m(1.0f);
                    std::memcpy(glm::value_ptr(m), transform.worldMatrix.m, sizeof(float) * 16);
                    item.transform = m;

                    renderQueue.push_back(item);
                    m_EcsRenderCount++;
                }
            }
        }
    }

    m_TotalRenderCount = m_StaticRenderCount + m_EcsRenderCount;
}

void SceneRenderer::loadModel(const std::string& path)
{
    if (path.find(".gltf") != std::string::npos || path.find(".glb") != std::string::npos) {
        auto model = std::make_unique<GltfModel>();
        if (model->load(path, resources, scene)) {
            gltfModels.push_back(std::move(model));
        } else {
            ::Logger::Log(::LogLevel::Error, "Failed to load GLTF model: " + path);
        }
        return;
    }

    Mesh* m = scene.createMeshFromOBJ(path, resources);
    if (m) {
        glm::vec3 size = m->maxBounds - m->minBounds;
        float maxDim = std::max({size.x, size.y, size.z});
        float scale = (maxDim > 0.001f) ? (10.0f / maxDim) : 1.0f;
        glm::vec3 center = (m->minBounds + m->maxBounds) * 0.5f;
        
        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::scale(transform, glm::vec3(scale));
        transform = glm::translate(transform, -center);

        Material* mat = scene.createMaterial();
        bool ok = mat->create("shaders/pbr_vert.spv", 
                              "shaders/pbr_frag.spv", 
                              "textures/brick_albedo.png", 
                              "textures/brick_normal.png", 
                              resources);
        if (!ok) {
            ::Logger::Log(::LogLevel::Error, "Failed to create material for loaded model. Falling back to geometry pipeline.");
            mat->setFallbackPipeline(resources.graphicsPipeline);
        }
        scene.addObject(m, mat, transform);
    }
}

void SceneRenderer::updateGlobalUBO()
{
    float aspect = m_OffscreenRenderingEnabled
        ? static_cast<float>(m_OffscreenWidth) / (float)m_OffscreenHeight
        : static_cast<float>(resources.swapChainExtent.width) / (float)resources.swapChainExtent.height;
    GlobalUBO ubo{};
    ubo.view = camera.getViewMatrix();
    ubo.proj = camera.getProjMatrix(aspect);
    ubo.proj[1][1] *= -1.0f;
    ubo.cameraPos = glm::vec4(camera.position, 0.0f);

    FrameData& fd = resources.getCurrentFrameData(frameIndex);
    void* dst = nullptr;
    vmaMapMemory(resources.allocator, fd.uboAlloc, &dst);
    std::memcpy(dst, &ubo, sizeof(ubo));
    vmaUnmapMemory(resources.allocator, fd.uboAlloc);
}

void SceneRenderer::updateLightingUBO()
{
    LightData uboData{};
    
    // Editor Preview Lighting (Sunny preview fallback settings)
    glm::vec3 activeLightDir = glm::vec3(-0.35f, -0.85f, -0.35f);
    glm::vec3 activeLightCol = glm::vec3(1.0f, 0.96f, 0.86f);
    float activeLightIntensity = 3.5f;
    glm::vec3 activeAmbientCol = glm::vec3(0.45f, 0.50f, 0.58f);
    float activeAmbientIntensity = 0.55f;

    uboData.ambientColorIntensity = glm::vec4(activeAmbientCol, activeAmbientIntensity);
    uboData.directionalDirectionIntensity = glm::vec4(glm::normalize(activeLightDir), activeLightIntensity);
    uboData.directionalColor = glm::vec4(activeLightCol, 1.0f);
    uboData.pointLightCount = 0;
    uboData.shadingMode = m_ShadingMode;

    bool sceneHasLights = false;
    if (m_World) {
        auto& coordinator = m_World->getCoordinator();
        for (Entity ent : coordinator.GetActiveEntities()) {
            if (!coordinator.IsEntityAlive(ent)) continue;
            auto sig = coordinator.GetSignature(ent);
            if (sig.test(coordinator.GetComponentType<DirectionalLightComponent>()) ||
                sig.test(coordinator.GetComponentType<PointLightComponent>()) ||
                sig.test(coordinator.GetComponentType<AmbientLightComponent>()) ||
                sig.test(coordinator.GetComponentType<SpotLightComponent>())) {
                sceneHasLights = true;
                break;
            }
        }
    }

    if (!m_UseEditorDefaultLighting && m_World && sceneHasLights) {
        auto& coordinator = m_World->getCoordinator();
        auto lightCollectionSys = coordinator.GetSystem<eng::runtime::LightCollectionSystem>();
        if (lightCollectionSys) {
            auto lightData = lightCollectionSys->CollectLights(coordinator);
            
            // Map directional light
            uboData.directionalDirectionIntensity = glm::vec4(lightData.directionalLight.direction, lightData.directionalLight.intensity);
            uboData.directionalColor = glm::vec4(lightData.directionalLight.color, 1.0f);
            
            // Map ambient light
            uboData.ambientColorIntensity = glm::vec4(lightData.ambientLight.color, lightData.ambientLight.intensity);
            
            // Map point lights
            uboData.pointLightCount = static_cast<uint32_t>(lightData.pointLights.size());
            for (uint32_t i = 0; i < uboData.pointLightCount && i < 16; ++i) {
                const auto& pt = lightData.pointLights[i];
                uboData.pointPositionsRadius[i] = glm::vec4(pt.position, pt.radius);
                uboData.pointColorsIntensity[i] = glm::vec4(pt.color, pt.intensity);
            }
        }
    }

    m_LastLightData = uboData;
    m_LastFallbackActive = m_UseEditorDefaultLighting || !m_World || !sceneHasLights;
    if (!m_LastFallbackActive && m_World) {
        auto& coordinator = m_World->getCoordinator();
        if (!coordinator.GetSystem<eng::runtime::LightCollectionSystem>()) {
            m_LastFallbackActive = true;
        }
    }

    const auto& lData = resources.perFrameLightingData[frameIndex];
    void* dst = nullptr;
    VK_CHECK(vmaMapMemory(resources.allocator, lData.uboAlloc, &dst));
    std::memcpy(dst, &uboData, sizeof(uboData));
    vmaUnmapMemory(resources.allocator, lData.uboAlloc);
}

void SceneRenderer::SetOffscreenRenderingEnabled(bool enabled) {
    if (m_OffscreenRenderingEnabled != enabled) {
        m_OffscreenRenderingEnabled = enabled;
        if (m_OffscreenRenderingEnabled) {
            CreateOffscreenResources(m_OffscreenWidth, m_OffscreenHeight);
        } else {
            DestroyOffscreenResources();
        }
        setupRenderGraph();
    }
}

void SceneRenderer::CreateOffscreenResources(uint32_t width, uint32_t height) {
    width = std::max(1u, width);
    height = std::max(1u, height);

    if (m_OffscreenWidth == width && m_OffscreenHeight == height && m_OffscreenRenderPass != VK_NULL_HANDLE) {
        return;
    }

    DestroyOffscreenResources();

    m_OffscreenWidth = width;
    m_OffscreenHeight = height;

    uint32_t maxFrames = resources.MAX_FRAMES_IN_FLIGHT;
    m_OffscreenImages.resize(maxFrames, VK_NULL_HANDLE);
    m_OffscreenAllocations.resize(maxFrames, VK_NULL_HANDLE);
    m_OffscreenImageViews.resize(maxFrames, VK_NULL_HANDLE);
    m_OffscreenFramebuffers.resize(maxFrames, VK_NULL_HANDLE);
    m_OffscreenImGuiTextures.resize(maxFrames, VK_NULL_HANDLE);

    // 1. Create Offscreen Render Pass
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = resources.swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkSubpassDependency dependency2{};
    dependency2.srcSubpass = 0;
    dependency2.dstSubpass = VK_SUBPASS_EXTERNAL;
    dependency2.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency2.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependency2.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    std::array<VkSubpassDependency, 2> dependencies = { dependency, dependency2 };

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    VK_CHECK(vkCreateRenderPass(resources.device, &renderPassInfo, nullptr, &m_OffscreenRenderPass));

    // 2. Create Sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    VK_CHECK(vkCreateSampler(resources.device, &samplerInfo, nullptr, &m_OffscreenSampler));

    // 3. Create color images, image views, framebuffers, and ImGui textures
    for (uint32_t i = 0; i < maxFrames; ++i) {
        VkImageCreateInfo imgInfo{};
        imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.imageType = VK_IMAGE_TYPE_2D;
        imgInfo.format = resources.swapChainImageFormat;
        imgInfo.extent.width = width;
        imgInfo.extent.height = height;
        imgInfo.extent.depth = 1;
        imgInfo.mipLevels = 1;
        imgInfo.arrayLayers = 1;
        imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo imgAllocInfo{};
        imgAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VK_CHECK(vmaCreateImage(resources.allocator, &imgInfo, &imgAllocInfo,
                                 &m_OffscreenImages[i], &m_OffscreenAllocations[i], nullptr));
        ::eng::ResourceTracker::incImage();

        // Transition layout UNDEFINED -> SHADER_READ_ONLY_OPTIMAL so render pass initialLayout is valid
        VkCommandBuffer cmd = resources.beginSingleTimeCommands();
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_OffscreenImages[i];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);
        resources.endSingleTimeCommands(cmd);

        // Create ImageView
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_OffscreenImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = resources.swapChainImageFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(resources.device, &viewInfo, nullptr, &m_OffscreenImageViews[i]));

        // Create Framebuffer
        VkImageView attachments[] = { m_OffscreenImageViews[i] };
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_OffscreenRenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = width;
        framebufferInfo.height = height;
        framebufferInfo.layers = 1;

        VK_CHECK(vkCreateFramebuffer(resources.device, &framebufferInfo, nullptr, &m_OffscreenFramebuffers[i]));

        // Register texture with ImGui
        m_OffscreenImGuiTextures[i] = ImGui_ImplVulkan_AddTexture(m_OffscreenSampler, m_OffscreenImageViews[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
}

void SceneRenderer::DestroyOffscreenResources() {
    for (size_t i = 0; i < m_OffscreenImGuiTextures.size(); ++i) {
        if (m_OffscreenImGuiTextures[i] != VK_NULL_HANDLE) {
            ImGui_ImplVulkan_RemoveTexture(m_OffscreenImGuiTextures[i]);
            m_OffscreenImGuiTextures[i] = VK_NULL_HANDLE;
        }
        if (m_OffscreenFramebuffers[i] != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(resources.device, m_OffscreenFramebuffers[i], nullptr);
            m_OffscreenFramebuffers[i] = VK_NULL_HANDLE;
        }
        if (m_OffscreenImageViews[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(resources.device, m_OffscreenImageViews[i], nullptr);
            m_OffscreenImageViews[i] = VK_NULL_HANDLE;
        }
        if (m_OffscreenImages[i] != VK_NULL_HANDLE) {
            vmaDestroyImage(resources.allocator, m_OffscreenImages[i], m_OffscreenAllocations[i]);
            m_OffscreenImages[i] = VK_NULL_HANDLE;
            m_OffscreenAllocations[i] = VK_NULL_HANDLE;
            ::eng::ResourceTracker::decImage();
        }
    }

    if (m_OffscreenSampler != VK_NULL_HANDLE) {
        vkDestroySampler(resources.device, m_OffscreenSampler, nullptr);
        m_OffscreenSampler = VK_NULL_HANDLE;
    }

    if (m_OffscreenRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(resources.device, m_OffscreenRenderPass, nullptr);
        m_OffscreenRenderPass = VK_NULL_HANDLE;
    }

    m_OffscreenImages.clear();
    m_OffscreenAllocations.clear();
    m_OffscreenImageViews.clear();
    m_OffscreenFramebuffers.clear();
    m_OffscreenImGuiTextures.clear();
}

VkDescriptorSet SceneRenderer::GetOffscreenTexture(uint32_t frameIndex) const {
    if (!m_OffscreenRenderingEnabled || frameIndex >= m_OffscreenImGuiTextures.size()) {
        return VK_NULL_HANDLE;
    }
    return m_OffscreenImGuiTextures[frameIndex];
}

} // namespace eng::renderer
