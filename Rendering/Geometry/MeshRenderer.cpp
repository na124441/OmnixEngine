#include "Core/pch.h"
#include "MeshRenderer.h"
#include "RenderingEngine/Renderer/scene/RenderQueue.h"
#include "RenderingEngine/Core/Engine/EngineResources.h"
#include <glm/gtc/type_ptr.hpp>

namespace eng::renderer {

void MeshRenderer::DrawQueue(
    VkCommandBuffer cmd,
    VkPipelineLayout pipelineLayout,
    const RenderQueue& renderQueue
)
{
    DrawQueue(cmd, pipelineLayout, renderQueue, VK_NULL_HANDLE);
}

void MeshRenderer::DrawQueue(
    VkCommandBuffer cmd,
    VkPipelineLayout pipelineLayout,
    const RenderQueue& renderQueue,
    VkPipeline overridePipeline
)
{
    const auto& items = renderQueue.getItems();
    if (overridePipeline != VK_NULL_HANDLE) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, overridePipeline);
    }
    for (uint32_t i = 0; i < items.size(); ++i)
    {
        const RenderItem& item = items[i];
        if (pipelineLayout != VK_NULL_HANDLE) {
            vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t), &i);
        }
        if (overridePipeline == VK_NULL_HANDLE) {
            item.material->bind(cmd, pipelineLayout);
        }
        item.mesh->bind(cmd);
        vkCmdDrawIndexed(cmd, item.mesh->getIndexCount(), 1, 0, 0, 0);
    }
}

} // namespace eng::renderer
