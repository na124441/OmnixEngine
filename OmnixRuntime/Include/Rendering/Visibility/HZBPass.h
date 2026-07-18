#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "RenderingEngine/Core/Engine/EngineResources.h"

namespace eng::renderer {

    struct HZBFrameResources {
        VkImage hzbImage = VK_NULL_HANDLE;
        VmaAllocation hzbAllocation = VK_NULL_HANDLE;
        VkImageView hzbSRV = VK_NULL_HANDLE; // Overall view for sampling all mips
        std::vector<VkImageView> mipViews;   // Single-mip views
        
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t mipLevels = 0;

        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkDescriptorSet copyDescriptorSet = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> downsampleDescriptorSets;
    };

    class HZBPass {
    public:
        HZBPass() = default;
        ~HZBPass() = default;

        void Initialize(EngineResources& resources);
        void Shutdown(EngineResources& resources);

        void Execute(
            VkCommandBuffer cmd,
            EngineResources& resources,
            uint32_t frameIndex,
            VkImageView depthImageView,
            VkImage depthImage,
            uint32_t width,
            uint32_t height
        );

        VkImageView GetHZBSRV(uint32_t frameIndex) const { return m_Frames[frameIndex].hzbSRV; }
        uint32_t GetMipCount(uint32_t frameIndex) const { return m_Frames[frameIndex].mipLevels; }
        uint32_t GetWidth(uint32_t frameIndex) const { return m_Frames[frameIndex].width; }
        uint32_t GetHeight(uint32_t frameIndex) const { return m_Frames[frameIndex].height; }
        VkSampler GetSampler() const { return m_HZBSampler; }
        void RecreateResources(EngineResources& resources, uint32_t width, uint32_t height);

    private:
        void createPipelines(EngineResources& resources);
        void recreateFrameResources(EngineResources& resources, HZBFrameResources& frameRes, uint32_t width, uint32_t height);
        void destroyFrameResources(EngineResources& resources, HZBFrameResources& frameRes);

        VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        
        VkPipeline m_CopyPipeline = VK_NULL_HANDLE;
        VkPipeline m_DownsamplePipeline = VK_NULL_HANDLE;
        VkSampler m_HZBSampler = VK_NULL_HANDLE;

        std::vector<HZBFrameResources> m_Frames;
    };

} // namespace eng::renderer
