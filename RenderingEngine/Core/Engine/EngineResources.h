#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <string>
#include <functional>
#include "VmaUsage.h"
#include "renderer/Pass.h"

namespace eng::renderer {

class GeometryArena;
struct RenderDebugConfig;

struct TransferContext {
    VkBuffer          stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation     stagingAlloc;
    VkDeviceSize      size = 0;               // current buffer size
};

struct FrameData {
    VkBuffer          uboBuffer    = VK_NULL_HANDLE;
    VmaAllocation     uboAlloc     = VK_NULL_HANDLE;
    VkDescriptorSet   uboDescriptor = VK_NULL_HANDLE; // Renamed to clarify

    // Temporary descriptor pool (for per‑frame allocations)
    VkDescriptorPool  descriptorPool = VK_NULL_HANDLE;
};

struct EngineResources {
    const RenderDebugConfig* debugConfig = nullptr;
    GeometryArena*      geometryArena   = nullptr;
    // Core Vulkan objects – owned elsewhere
    VkInstance          instance        = VK_NULL_HANDLE;
    VkDevice            device          = VK_NULL_HANDLE;
    VkPhysicalDevice    physicalDevice  = VK_NULL_HANDLE;
    VkQueue             graphicsQueue   = VK_NULL_HANDLE;
    VkQueue             presentQueue    = VK_NULL_HANDLE;
    uint32_t            graphicsQueueFamily = UINT32_MAX;

    // Swapchain handles
    VkSwapchainKHR      swapChain          = VK_NULL_HANDLE;
    VkFormat            swapChainImageFormat;
    VkExtent2D          swapChainExtent;
    std::vector<VkImage>       swapChainImages;
    std::vector<VkImageView>   swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    // Render pass & pipeline (baseline)
    VkRenderPass        renderPass       = VK_NULL_HANDLE;
    VkRenderPass        transparentRenderPass = VK_NULL_HANDLE;
    VkRenderPass        gbufferRenderPass = VK_NULL_HANDLE;
    VkPipeline          graphicsPipeline = VK_NULL_HANDLE;
    VkPipelineLayout    pipelineLayout   = VK_NULL_HANDLE;

    // Sync objects
    std::vector<VkSemaphore>   imageAvailableSemaphores;
    std::vector<VkSemaphore>   renderFinishedSemaphores;
    std::vector<VkFence>       inFlightFences;

    // Command pools & per-frame command buffers (one per pass)
    std::vector<VkCommandPool> commandPools; // one pool per frame
    std::vector<std::array<VkCommandBuffer, PASS_COUNT>> commandBuffers; // [frame][pass]

    // Descriptor sets from the main pipeline (legacy)
    std::vector<VkDescriptorSet> descriptorSets;

    // VMA Allocator
    VmaAllocator allocator = VK_NULL_HANDLE;

    // Staging buffer context
    TransferContext transfer;

    // UI Overlay callback for editor rendering
    std::function<void(VkCommandBuffer, uint32_t)> uiCallback;

    // Phase 5: Per-frame UBO resources
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;
    VkDescriptorSetLayout    globalSetLayout = VK_NULL_HANDLE;
    std::vector<FrameData>   perFrameData;

    FrameData& getCurrentFrameData(uint32_t frameIdx) { return perFrameData[frameIdx]; }

    // --- material‑specific resources ------------------------------------
    VkDescriptorSetLayout materialSetLayout = VK_NULL_HANDLE;   // layout for per‑material resources
    VkDescriptorPool      materialDescriptorPool = VK_NULL_HANDLE; // pool for all material sets

    // --- lighting‑specific resources ------------------------------------
    VkDescriptorSetLayout lightingSetLayout = VK_NULL_HANDLE;   // layout for lighting resources
    VkDescriptorPool      lightingDescriptorPool = VK_NULL_HANDLE;
    
    struct LightingFrameData {
        VkBuffer            uboBuffer   = VK_NULL_HANDLE;
        VmaAllocation       uboAlloc    = VK_NULL_HANDLE;
        VkDescriptorSet     descriptor   = VK_NULL_HANDLE;
    };
    std::vector<LightingFrameData> perFrameLightingData;

    void createMaterialDescriptorResources();
    void destroyMaterialDescriptorResources();
    
    void createLightingDescriptorResources();
    void destroyLightingDescriptorResources();
    
    void createPipelineLayout();

    void createCommandPools();
    void createCommandBuffers();                // allocates PASS_COUNT buffers per frame
    void createSyncObjects();                   // New dedicated sync object creation
    void destroySyncObjects();                  // Cleanup
    void destroyCommandPools();                // frees command pools (and buffers)
    void createPerFrameResources();           // creates UBO + descriptor pool per frame
    void destroyPerFrameResources();

    // Helpers
    VkCommandBuffer beginSingleTimeCommands() const;
    void           endSingleTimeCommands(VkCommandBuffer cmd) const;
    void           recreateSwapChain();
    
    // Shader loading helper
    VkShaderModule loadShaderModule(const std::string& filename) const;

    // VMA Staging buffer helpers
    void ensureStagingBuffer(VkDeviceSize neededSize);
    void copyStagingToDevice(VkCommandBuffer cmd, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize copySize);

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) const;

    // Singleton access for cleanup
    static EngineResources& get() {
        static EngineResources res;
        return res;
    }
};

} // namespace eng::renderer
