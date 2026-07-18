#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "RenderingEngine/Core/Engine/EngineResources.h"

namespace eng::renderer {

    class GPUScene;

    struct RVGClusterCullFrameResources {
        VkBuffer indirectCommandBuffer = VK_NULL_HANDLE;
        VmaAllocation indirectCommandAlloc = VK_NULL_HANDLE;
        VkDeviceSize indirectCommandBufferSize = 0;
        uint32_t indirectCommandCapacity = 0;

        VkBuffer indirectCountBuffer = VK_NULL_HANDLE;
        VmaAllocation indirectCountAlloc = VK_NULL_HANDLE;

        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    };

    class RVGClusterCullPass {
    public:
        RVGClusterCullPass() = default;
        ~RVGClusterCullPass() = default;

        void Initialize(EngineResources& resources);
        void Shutdown(EngineResources& resources);

        void Execute(
            VkCommandBuffer cmd,
            EngineResources& resources,
            uint32_t frameIndex,
            GPUScene& gpuScene,
            VkImageView hzbSRV,
            VkSampler hzbSampler,
            uint32_t instanceCount,
            bool frustumOnlyMode,
            float lodBias,
            float targetPixelError,
            uint32_t maxTraversalDepth,
            uint32_t debugMode,
            uint32_t forceRoot,
            uint32_t forceFullDetail
        );

        VkBuffer GetIndirectCommandBuffer(uint32_t frameIndex) const { return m_Frames[frameIndex].indirectCommandBuffer; }
        VkBuffer GetIndirectCountBuffer(uint32_t frameIndex) const { return m_Frames[frameIndex].indirectCountBuffer; }

        uint32_t ReadBackDrawCount(EngineResources& resources, uint32_t frameIndex);

    private:
        void createPipeline(EngineResources& resources);
        void createFrameResources(EngineResources& resources, RVGClusterCullFrameResources& frameRes);
        void destroyFrameResources(EngineResources& resources, RVGClusterCullFrameResources& frameRes);
        
        void resizeIndirectCommandBufferIfNeeded(
            EngineResources& resources,
            RVGClusterCullFrameResources& frameRes,
            uint32_t neededCapacity
        );

        void writeDescriptorSet(
            EngineResources& resources,
            RVGClusterCullFrameResources& frameRes,
            uint32_t frameIndex,
            VkBuffer instanceBuffer,
            VkDeviceSize instanceBufferSize,
            VkBuffer rvgAssetBuffer,
            VkBuffer rvgClusterBuffer,
            VkBuffer rvgNodeBuffer,
            VkBuffer cameraBuffer,
            VkBuffer frustumBuffer,
            VkImageView hzbSRV,
            VkSampler hzbSampler
        );

        VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_Pipeline = VK_NULL_HANDLE;

        std::vector<RVGClusterCullFrameResources> m_Frames;
    };

} // namespace eng::renderer
