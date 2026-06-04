#include "SceneRenderer.h"
#include "engine/EngineResources.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

void SceneRenderer::init()
{
    device        = resources.device;
    graphicsQueue = resources.graphicsQueue;
    presentQueue  = resources.presentQueue;
    swapExtent    = resources.swapChainExtent;
    swapImageViews= resources.swapChainImageViews;
    renderPass    = resources.renderPass;
    graphicsPipeline = resources.graphicsPipeline; 
    pipelineLayout   = resources.pipelineLayout;

    commandBuffers.resize(resources.commandPools.size());
    for (size_t i = 0; i < resources.commandPools.size(); ++i) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = resources.commandPools[i];
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo,
                                          &commandBuffers[i]));
    }

    buildPyramidMesh();
    LOG_INFO("SceneRenderer initialised – ready to draw the scene");
}

void SceneRenderer::cleanup()
{
    scene.clearObjects();
    LOG_INFO("SceneRenderer cleaned up.");
}

void SceneRenderer::buildPyramidMesh()
{
    const std::vector<Vertex> vertices = {
        {{ 0.0f, -0.5f,  0.0f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}}
    };
    const std::vector<uint32_t> indices = {
        0,1,2, 0,3,1, 0,2,4, 0,4,3,
        1,3,4, 1,4,2
    };

    Mesh* pyramidMesh = scene.createMesh();
    bool ok = pyramidMesh->init(
        vertices.data(), vertices.size(),
        indices.data(),  indices.size(),
        resources);
    if (!ok) LOG_FATAL("Failed to create pyramid mesh");

    Material* mat = scene.createMaterial(graphicsPipeline, pipelineLayout);
    scene.addObject(pyramidMesh, mat, glm::mat4(1.0f));

    LOG_INFO("Pyramid mesh & material added to Scene.");
}

void SceneRenderer::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIdx)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    DebugLabel::Begin(cmd, ("Frame " + std::to_string(frameIndex)).c_str());

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass  = renderPass;
    rpInfo.framebuffer = resources.swapChainFramebuffers[imageIdx];
    rpInfo.renderArea.offset = {0,0};
    rpInfo.renderArea.extent = swapExtent;

    VkClearValue clearColors[1];
    clearColors[0].color = {{0.1f, 0.1f, 0.1f, 1.0f}};
    rpInfo.clearValueCount = 1;
    rpInfo.pClearValues = clearColors;

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    for (const RenderObject& obj : scene.getObjects())
    {
        obj.material->bind(cmd);
        obj.mesh->bind(cmd);
        vkCmdDrawIndexed(cmd, obj.mesh->getIndexCount(), 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(cmd);
    DebugLabel::End(cmd);
    VK_CHECK(vkEndCommandBuffer(cmd));
}

void SceneRenderer::drawFrame()
{
    vkWaitForFences(device, 1, &resources.inFlightFences[frameIndex], VK_TRUE, UINT64_MAX);

    uint32_t imageIdx;
    VkResult result = vkAcquireNextImageKHR(device, resources.swapChain, UINT64_MAX, resources.imageAvailableSemaphores[frameIndex], VK_NULL_HANDLE, &imageIdx);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        onWindowResized();
        return;
    }
    VK_CHECK(result);

    vkResetCommandPool(device, resources.commandPools[frameIndex], VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);

    recordCommandBuffer(commandBuffers[frameIndex], imageIdx);

    VkSemaphore waitSem = resources.imageAvailableSemaphores[frameIndex];
    VkSemaphore signalSem = resources.renderFinishedSemaphores[frameIndex];

    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores    = &waitSem;
    submitInfo.pWaitDstStageMask  = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &commandBuffers[frameIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &signalSem;

    vkResetFences(device, 1, &resources.inFlightFences[frameIndex]);
    VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, resources.inFlightFences[frameIndex]));

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &signalSem;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains    = &resources.swapChain;
    presentInfo.pImageIndices  = &imageIdx;

    result = vkQueuePresentKHR(presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        onWindowResized();
    } else {
        VK_CHECK(result);
    }

    frameIndex = (frameIndex + 1) % resources.commandPools.size();
    timer.tick();
}

void SceneRenderer::onWindowResized()
{
    LOG_INFO("Window resize detected – recreating swapchain");
    resources.recreateSwapChain();
    swapExtent = resources.swapChainExtent;
}
