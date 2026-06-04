#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <string>

#include "Core/Vulkan/VkUtils.h"     // Priority 3: VK_CHECK
#include "Core/types/Vertex.h"      // Priority 2: Unified Vertex
#include "Core/Engine/Timer.h"
#include "Core/Engine/Log.h"
#include "Core/Engine/ResourceTracker.h"
#include "Core/Engine/DebugLabels.h"
#include "Vulkan/VulkanDevice.h"

namespace eng::renderer {

    struct UniformBufferObject {
        float view[16];
        float proj[16];
    };

    class PyramidRenderer {
    public:
        PyramidRenderer();
        ~PyramidRenderer();

        void Initialize(VkInstance instance, eng::vulkan::VulkanDevice* device, VkRenderPass renderPass, VkExtent2D extent);
        void Shutdown();

        void Update(float deltaTime, uint32_t currentImage);
        void RecordCommands(VkCommandBuffer commandBuffer, uint32_t currentImage);
        void mainLoop(::GLFWwindow* window);

        VkPipeline GetPipeline() const { return m_GraphicsPipeline; }
        VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }
        VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_DescriptorSetLayout; }
        const std::vector<VkDescriptorSet>& GetDescriptorSets() const { return m_DescriptorSets; }

        uint32_t frameIndex = 0;
        static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;
        ::eng::FrameTimer timer;
        VkInstance instance;

    private:
        void CreateDescriptorSetLayout();
        void CreateGraphicsPipeline(VkRenderPass renderPass);
        void CreateVertexBuffer();
        void CreateIndexBuffer();
        void CreateUniformBuffers();
        void CreateDescriptorPool();
        void CreateDescriptorSets();
        void CreateCommandPools();
        void allocateCommandBuffers();
        
        void recordMainPass(VkCommandBuffer cmd, uint32_t currentImage);
        
        // Priority 4: Declare CreateShaderModule
        VkShaderModule CreateShaderModule(const std::vector<char>& code);

        uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
        void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

        eng::vulkan::VulkanDevice* m_DevicePtr = nullptr;
        VkDevice m_Device = VK_NULL_HANDLE; // Priority 5: Add m_Device
        VkExtent2D m_Extent;

        VkDescriptorSetLayout m_DescriptorSetLayout;
        VkPipelineLayout m_PipelineLayout;
        VkPipeline m_GraphicsPipeline;

        VkBuffer m_VertexBuffer;
        VkDeviceMemory m_VertexBufferMemory;
        VkBuffer m_IndexBuffer;
        VkDeviceMemory m_IndexBufferMemory;

        std::vector<VkBuffer> m_UniformBuffers;
        std::vector<VkDeviceMemory> m_UniformBuffersMemory;
        std::vector<void*> m_UniformBuffersMapped;

        VkDescriptorPool m_DescriptorPool;
        std::vector<VkDescriptorSet> m_DescriptorSets;

        std::vector<VkCommandPool> commandPools;
        std::vector<VkCommandBuffer> commandBuffers;

        float m_Rotation = 0.0f;
    };

} // namespace eng::renderer
