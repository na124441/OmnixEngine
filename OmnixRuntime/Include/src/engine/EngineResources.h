#pragma once
#include <vulkan/vulkan.h>
#include <vector>

struct EngineResources {
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
    VkPipeline          graphicsPipeline = VK_NULL_HANDLE;
    VkPipelineLayout    pipelineLayout   = VK_NULL_HANDLE;

    // Sync objects
    std::vector<VkSemaphore>   imageAvailableSemaphores;
    std::vector<VkSemaphore>   renderFinishedSemaphores;
    std::vector<VkFence>       inFlightFences;

    // Command pools
    std::vector<VkCommandPool> commandPools;

    // Helpers
    VkCommandBuffer beginSingleTimeCommands() const;
    void           endSingleTimeCommands(VkCommandBuffer cmd) const;
    void           recreateSwapChain() const;

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) const;

    // Singleton access for cleanup
    static EngineResources& get() {
        static EngineResources res;
        return res;
    }
};
