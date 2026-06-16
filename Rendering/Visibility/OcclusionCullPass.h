#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "RenderingEngine/Core/Engine/EngineResources.h"

namespace eng::renderer {

    class GPUScene;

    struct OcclusionCullFrameResources {
        VkBuffer finalVisibleInstanceBuffer = VK_NULL_HANDLE;
        VmaAllocation finalVisibleInstanceAlloc = VK_NULL_HANDLE;
        VkDeviceSize finalVisibleInstanceBufferSize = 0;
        uint32_t finalVisibleInstanceCapacity = 0;

        VkBuffer finalVisibleCountBuffer = VK_NULL_HANDLE;
        VmaAllocation finalVisibleCountAlloc = VK_NULL_HANDLE;

        VkBuffer occlusionCulledCountBuffer = VK_NULL_HANDLE;
        VmaAllocation occlusionCulledCountAlloc = VK_NULL_HANDLE;

        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VkDescriptorSet gbufferDescriptorSet = VK_NULL_HANDLE;
    };

    class OcclusionCullPass {
    public:
        OcclusionCullPass() = default;
        ~OcclusionCullPass() = default;

        void Initialize(EngineResources& resources, VkDescriptorSetLayout gbufferLayout);
        void Shutdown(EngineResources& resources);

        void Execute(
            VkCommandBuffer cmd,
            EngineResources& resources,
            uint32_t frameIndex,
            GPUScene& gpuScene,
            VkBuffer frustumVisibleBuffer,
            VkDeviceSize frustumVisibleBufferSize,
            VkBuffer frustumVisibleCountBuffer,
            VkImageView hzbSRV,
            VkSampler hzbSampler,
            uint32_t instanceCount,
            bool frustumOnlyMode
        );

        uint32_t ReadBackFinalCount(EngineResources& resources, uint32_t frameIndex);
        uint32_t ReadBackCulledCount(EngineResources& resources, uint32_t frameIndex);
        std::vector<uint32_t> ReadBackFinalVisibleInstances(EngineResources& resources, uint32_t frameIndex, uint32_t count);

        VkBuffer GetFinalVisibleInstanceBuffer(uint32_t frameIndex) const { return m_Frames[frameIndex].finalVisibleInstanceBuffer; }
        VkBuffer GetFinalVisibleCountBuffer(uint32_t frameIndex) const { return m_Frames[frameIndex].finalVisibleCountBuffer; }
        VkDescriptorSet GetGbufferDescriptorSet(uint32_t frameIndex) const { return m_Frames[frameIndex].gbufferDescriptorSet; }

    private:
        void createPipeline(EngineResources& resources);
        void createFrameResources(EngineResources& resources, OcclusionCullFrameResources& frameRes, VkDescriptorSetLayout gbufferLayout);
        void destroyFrameResources(EngineResources& resources, OcclusionCullFrameResources& frameRes);
        
        void resizeFinalVisibleInstanceBufferIfNeeded(
            EngineResources& resources,
            OcclusionCullFrameResources& frameRes,
            uint32_t neededCapacity
        );

        void writeDescriptorSet(
            EngineResources& resources,
            OcclusionCullFrameResources& frameRes,
            VkBuffer instanceBuffer,
            VkDeviceSize instanceBufferSize,
            VkBuffer frustumBuffer,
            VkBuffer frustumVisibleBuffer,
            VkDeviceSize frustumVisibleBufferSize,
            VkBuffer frustumVisibleCountBuffer,
            VkBuffer cameraBuffer,
            VkImageView hzbSRV,
            VkSampler hzbSampler
        );

        VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_GbufferDescriptorSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_Pipeline = VK_NULL_HANDLE;

        std::vector<OcclusionCullFrameResources> m_Frames;
    };

} // namespace eng::renderer
