#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "RenderingEngine/Core/Engine/EngineResources.h"

namespace eng::renderer {

    class GPUScene;

    struct FrustumCullFrameResources {
        VkBuffer visibleInstanceBuffer = VK_NULL_HANDLE;
        VmaAllocation visibleInstanceAlloc = VK_NULL_HANDLE;
        VkDeviceSize visibleInstanceBufferSize = 0;
        uint32_t visibleInstanceCapacity = 0;

        VkBuffer visibleCountBuffer = VK_NULL_HANDLE;
        VmaAllocation visibleCountAlloc = VK_NULL_HANDLE;

        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    };

    class FrustumCullPass {
    public:
        FrustumCullPass() = default;
        ~FrustumCullPass() = default;

        void Initialize(EngineResources& resources);
        void Shutdown(EngineResources& resources);

        void Execute(
            VkCommandBuffer cmd,
            EngineResources& resources,
            uint32_t frameIndex,
            GPUScene& gpuScene,
            uint32_t instanceCount
        );

        uint32_t ReadBackVisibleCount(EngineResources& resources, uint32_t frameIndex);
        std::vector<uint32_t> ReadBackVisibleInstances(EngineResources& resources, uint32_t frameIndex, uint32_t count);
        const FrustumCullFrameResources& GetFrameResources(uint32_t frameIndex) const { return m_Frames[frameIndex]; }
        VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_DescriptorSetLayout; }


    private:
        void createPipeline(EngineResources& resources);
        void createFrameResources(EngineResources& resources, FrustumCullFrameResources& frameRes);
        void destroyFrameResources(EngineResources& resources, FrustumCullFrameResources& frameRes);
        
        void resizeVisibleInstanceBufferIfNeeded(
            EngineResources& resources,
            FrustumCullFrameResources& frameRes,
            uint32_t neededCapacity
        );

        void writeDescriptorSet(
            EngineResources& resources,
            FrustumCullFrameResources& frameRes,
            VkBuffer instanceBuffer,
            VkDeviceSize instanceBufferSize,
            VkBuffer frustumBuffer
        );

        VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_Pipeline = VK_NULL_HANDLE;

        std::vector<FrustumCullFrameResources> m_Frames;
    };

} // namespace eng::renderer
