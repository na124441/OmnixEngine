#pragma once
#include <vulkan/vulkan.h>
#include "RenderingEngine/Core/Engine/EngineResources.h"
#include "Rendering/Core/RenderTypes.h"
#include "Rendering/Editor/SelectionOutlinePass.h"

namespace eng::renderer {

    class EditorOverlayPass {
    public:
        EditorOverlayPass() = default;
        ~EditorOverlayPass() = default;

        void Initialize(EngineResources& resources);
        void Shutdown(EngineResources& resources);

        void Execute(
            VkCommandBuffer cmd,
            EngineResources& resources,
            uint32_t frameIndex,
            VkRenderPass renderPass,
            VkFramebuffer framebuffer,
            VkExtent2D extent,
            VkDescriptorSet gpuSceneDescriptorSet,
            VkDescriptorSet gbufferDescriptorSet,
            VkPipeline gridPipeline,
            VkPipelineLayout gridPipelineLayout,
            SelectionOutlinePass& selectionOutlinePass,
            uint32_t selectedEntityID,
            const ViewportOverlaySettings& settings
        );
    };

} // namespace eng::renderer
