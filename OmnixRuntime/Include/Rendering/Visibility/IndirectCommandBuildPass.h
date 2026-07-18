#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "RenderingEngine/Core/Engine/EngineResources.h"
#include "Rendering/GPUScene/GPUScene.h"
#include "Rendering/Visibility/FrustumCullPass.h"

namespace eng::renderer {

    struct IndirectCommandFrameResources {
        VkBuffer indirectCommandBuffer = VK_NULL_HANDLE;
        VmaAllocation indirectCommandAlloc = VK_NULL_HANDLE;
        VkDeviceSize indirectCommandBufferSize = 0;
        uint32_t indirectCommandCapacity = 0;

        VkBuffer indirectCountBuffer = VK_NULL_HANDLE;
        VmaAllocation indirectCountAlloc = VK_NULL_HANDLE;

        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSetFinal = VK_NULL_HANDLE;
    };

    class IndirectCommandBuildPass {
    public:
        IndirectCommandBuildPass() = default;
        ~IndirectCommandBuildPass() = default;

        void Initialize(EngineResources& resources);
        void Shutdown(EngineResources& resources);

        void Execute(
            VkCommandBuffer cmd,
            EngineResources& resources,
            uint32_t frameIndex,
            GPUScene& gpuScene,
            VkBuffer visibleInstanceBuffer,
            VkDeviceSize visibleInstanceBufferSize,
            VkBuffer visibleCountBuffer,
            uint32_t maxInstanceCount,
            bool isFinal = false
        );

        uint32_t ReadBackDrawCount(EngineResources& resources, uint32_t frameIndex);

        VkBuffer GetIndirectCommandBuffer(uint32_t frameIndex) const { return m_Frames[frameIndex].indirectCommandBuffer; }
        VkBuffer GetIndirectCountBuffer(uint32_t frameIndex) const { return m_Frames[frameIndex].indirectCountBuffer; }

    private:
        void createPipeline(EngineResources& resources);
        void createFrameResources(EngineResources& resources, IndirectCommandFrameResources& frameRes);
        void destroyFrameResources(EngineResources& resources, IndirectCommandFrameResources& frameRes);
        
        void resizeIndirectCommandBufferIfNeeded(
            EngineResources& resources,
            IndirectCommandFrameResources& frameRes,
            uint32_t neededCapacity
        );

        void writeDescriptorSet(
            EngineResources& resources,
            IndirectCommandFrameResources& frameRes,
            VkDescriptorSet descriptorSet,
            VkBuffer visibleInstanceBuffer,
            VkDeviceSize visibleInstanceBufferSize,
            VkBuffer visibleCountBuffer,
            VkBuffer instanceBuffer,
            VkDeviceSize instanceBufferSize,
            VkBuffer meshDrawDataBuffer,
            VkDeviceSize meshDrawDataBufferSize
        );

        VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_Pipeline = VK_NULL_HANDLE;

        std::vector<IndirectCommandFrameResources> m_Frames;
    };

} // namespace eng::renderer
