#pragma once
#include "engine/Log.h"
#include "engine/Timer.h"
#include "engine/DebugLabels.h"
#include "engine/VkResultCheck.h"   // VK_CHECK macro
#include "engine/EngineResources.h" // we’ll expose a thin struct with device, allocator, command pool, etc.
#include "scene/Scene.h"

class SceneRenderer {
public:
    explicit SceneRenderer(const EngineResources& eng)
        : resources(eng) {}

    // -----------------------------------------------------------------
    // High‑level API used by the application
    void init();      // creates a dummy scene (pyramid) – called after Vulkan init
    void cleanup();   // destroys scene, meshes, etc.
    void drawFrame(); // same entry‑point signature as PyramidRenderer::drawFrame()
    void onWindowResized(); // forwards to swapchain recreation if needed

    // -----------------------------------------------------------------
    // Access to the scene for user code (e.g. load assets later)
    Scene& getScene() { return scene; }

private:
    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex);
    void buildPyramidMesh(); // convenience – creates the same geometry the legacy renderer uses

    const EngineResources& resources; // non‑owning reference to shared Vulkan objects

    // -----------------------------------------------------------------
    // Cached handles we borrow from the existing engine (no ownership)
    VkDevice            device          = VK_NULL_HANDLE;
    VkQueue             graphicsQueue   = VK_NULL_HANDLE;
    VkQueue             presentQueue    = VK_NULL_HANDLE;
    VkExtent2D          swapExtent      = {};
    std::vector<VkImageView> swapImageViews;
    VkRenderPass       renderPass      = VK_NULL_HANDLE;
    VkPipeline          graphicsPipeline = VK_NULL_HANDLE; // re‑use the one PyramidRenderer built
    VkPipelineLayout   pipelineLayout   = VK_NULL_HANDLE;

    // Command buffers – one per frame (reuse command pools from EngineResources)
    std::vector<VkCommandBuffer> commandBuffers;

    // -----------------------------------------------------------------
    Scene scene;          // owns meshes, materials, render objects
    uint32_t frameIndex = 0;
    FrameTimer timer;
};
