#pragma once
#include <vulkan/vulkan.h>

namespace eng::renderer {

    class RenderQueue;
    struct EngineResources;

    class MeshRenderer {
    public:
        static void DrawQueue(
            VkCommandBuffer cmd,
            VkPipelineLayout pipelineLayout,
            const RenderQueue& renderQueue
        );
        static void DrawQueue(
            VkCommandBuffer cmd,
            VkPipelineLayout pipelineLayout,
            const RenderQueue& renderQueue,
            VkPipeline overridePipeline,
            const class GPUScene* gpuScene = nullptr
        );
    };

} // namespace eng::renderer
