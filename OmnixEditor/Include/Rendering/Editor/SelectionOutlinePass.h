#pragma once
#include <vulkan/vulkan.h>
#include "RenderingEngine/Core/Engine/EngineResources.h"

namespace eng::renderer {

    class SelectionOutlinePass {
    public:
        SelectionOutlinePass() = default;
        ~SelectionOutlinePass() = default;

        void Initialize(EngineResources& resources, VkRenderPass renderPass, VkDescriptorSetLayout gbufferLayout);
        void Shutdown(EngineResources& resources);

        void Execute(
            VkCommandBuffer cmd,
            EngineResources& resources,
            uint32_t frameIndex,
            VkRenderPass renderPass,
            VkFramebuffer framebuffer,
            VkExtent2D extent,
            VkDescriptorSet gbufferDescriptorSet, // Set containing GBufferC at binding 2 (or custom)
            uint32_t selectedEntityID
        );

    private:
        void createPipeline(EngineResources& resources, VkRenderPass renderPass);

        VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_Pipeline = VK_NULL_HANDLE;
        VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> m_DescriptorSets;
    };

} // namespace eng::renderer
