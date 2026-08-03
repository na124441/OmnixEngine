#pragma once
#include <cstdint>

#include <vulkan/vulkan.h>

#include <string>
#include <glm/glm.hpp>

namespace eng::renderer {

    namespace GPUSceneBindings {
        constexpr uint32_t Camera = 0;
        constexpr uint32_t Instances = 1;
        constexpr uint32_t Materials = 2;
        constexpr uint32_t Lights = 3;
    }

    struct GBufferPushConstants {
        uint32_t instanceIndex;
    };

    struct RasterConvention {
        static constexpr VkFrontFace FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        static constexpr VkCullModeFlags CullMode = VK_CULL_MODE_BACK_BIT;
    };

    struct GraphicsPipelineInfo {
        std::string name;
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkRenderPass compatibleRenderPass = VK_NULL_HANDLE;
        uint32_t colorAttachmentCount = 0;
        uint32_t vertexStride = 0;

        operator VkPipeline() const { return pipeline; }
        GraphicsPipelineInfo& operator=(VkPipeline p) {
            pipeline = p;
            return *this;
        }
    };

    struct RenderItem;
    class GPUScene;

    bool IsFinite(const glm::mat4& matrix);
    bool ValidateRenderItem(const RenderItem& item, uint32_t instanceIndex, const GPUScene& gpuScene);

} // namespace eng::renderer