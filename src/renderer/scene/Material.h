#pragma once
#include <vulkan/vulkan.h>
#include "engine/Log.h"

/// Minimal material – only the pipeline and its layout.
/// In later phases we will add descriptor sets, textures, push‑constants, etc.
class Material {
public:
    Material() = default;
    Material(VkPipeline p, VkPipelineLayout l)
        : pipeline(p), layout(l) {}

    // Non‑copyable (pipeline handles are owned elsewhere)
    Material(const Material&) = delete;
    Material& operator=(const Material&) = delete;

    // Move‑able – useful when storing in containers
    Material(Material&& o) noexcept
        : pipeline(o.pipeline), layout(o.layout)
    {
        o.pipeline = VK_NULL_HANDLE;
        o.layout   = VK_NULL_HANDLE;
    }
    Material& operator=(Material&& o) noexcept
    {
        if (this != &o) {
            pipeline = o.pipeline; o.pipeline = VK_NULL_HANDLE;
            layout   = o.layout;   o.layout   = VK_NULL_HANDLE;
        }
        return *this;
    }

    // Bind the material for a draw call.
    void bind(VkCommandBuffer cmd) const
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        // No descriptor sets yet – will be added later.
    }

    VkPipeline       getPipeline() const { return pipeline; }
    VkPipelineLayout getLayout()   const { return layout;   }

private:
    VkPipeline       pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout   = VK_NULL_HANDLE;
};
