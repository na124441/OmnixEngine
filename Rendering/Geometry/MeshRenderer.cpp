#include "Core/pch.h"
#include "MeshRenderer.h"
#include "RenderingEngine/Renderer/scene/RenderQueue.h"
#include "RenderingEngine/Core/Engine/EngineResources.h"
#include "Rendering/Geometry/Arena/GeometryArena.h"
#include "Rendering/Core/Renderer.h"
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
    VkPipeline overridePipeline,
    const GPUScene* gpuScene
)
{
    const auto& items = renderQueue.getItems();
    if (overridePipeline != VK_NULL_HANDLE) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, overridePipeline);
    }
    
    VkBuffer boundVB = VK_NULL_HANDLE;
    VkBuffer boundIB = VK_NULL_HANDLE;

    for (uint32_t i = 0; i < items.size(); ++i)
    {
        const RenderItem& item = items[i];
        if (!item.mesh) {
            continue;
        }

        if (gpuScene && !ValidateRenderItem(item, i, *gpuScene)) {
            auto currentDiag = Renderer::GetCurrentDiagnostics();
            if (currentDiag) {
                currentDiag->rejectedItems++;
                currentDiag->validationErrors++;
            }
            continue;
        }

        auto currentDiag = Renderer::GetCurrentDiagnostics();
        if (currentDiag) {
            currentDiag->drawCalls++;
            currentDiag->triangles += item.mesh->indexCount / 3;
        }

        bool useArena = GeometryArena::IsInitialized() && GeometryArena::IsEnabled() && item.mesh->handle.IsValid();
        if (useArena) {
            auto arenaPtr = GeometryArena::GetInstance();
            const GeometryAllocation* alloc = arenaPtr->GetAllocation(item.mesh->handle);
            if (!alloc) {
                ::Logger::Log(::LogLevel::Error, "MeshRenderer::DrawQueue - Invalid allocation handle for mesh");
                continue;
            }
            if (alloc->firstIndex + alloc->indexCount > arenaPtr->GetTotalIndexCount()) {
                ::Logger::Log(::LogLevel::Error, "MeshRenderer::DrawQueue - Allocation firstIndex + indexCount out of range of total arena capacity");
                continue;
            }

            if (pipelineLayout != VK_NULL_HANDLE) {
                GBufferPushConstants push{};
                push.instanceIndex = i;
                vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GBufferPushConstants), &push);
            }
            if (overridePipeline != VK_NULL_HANDLE) {
                if (item.material) {
                    item.material->bindDescriptorSet(cmd, pipelineLayout);
                }
            } else if (item.material) {
                item.material->bind(cmd, pipelineLayout);
            }

            // Bind vertex and index buffers (avoid redundant binds)
            if (item.mesh->vertexBuffer != boundVB) {
                VkDeviceSize offsets[] = { 0 };
                vkCmdBindVertexBuffers(cmd, 0, 1, &item.mesh->vertexBuffer, offsets);
                boundVB = item.mesh->vertexBuffer;
            }
            if (item.mesh->indexBuffer != boundIB) {
                vkCmdBindIndexBuffer(cmd, item.mesh->indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                boundIB = item.mesh->indexBuffer;
            }

            vkCmdDrawIndexed(
                cmd,
                alloc->indexCount,
                1,
                alloc->firstIndex,
                alloc->vertexOffset,
                i
            );
        } else {
            // Conventional (non-arena) path
            if (item.mesh->indexBuffer == VK_NULL_HANDLE ||
                item.mesh->vertexBuffer == VK_NULL_HANDLE ||
                item.mesh->indexCount == 0)
            {
                continue;
            }

            if (pipelineLayout != VK_NULL_HANDLE) {
                GBufferPushConstants push{};
                push.instanceIndex = i;
                vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GBufferPushConstants), &push);
            }
            if (overridePipeline != VK_NULL_HANDLE) {
                if (item.material) {
                    item.material->bindDescriptorSet(cmd, pipelineLayout);
                }
            } else if (item.material) {
                item.material->bind(cmd, pipelineLayout);
            }

            // Bind vertex and index buffers (avoid redundant binds)
            if (item.mesh->vertexBuffer != boundVB) {
                VkDeviceSize offsets[] = { 0 };
                vkCmdBindVertexBuffers(cmd, 0, 1, &item.mesh->vertexBuffer, offsets);
                boundVB = item.mesh->vertexBuffer;
            }
            if (item.mesh->indexBuffer != boundIB) {
                vkCmdBindIndexBuffer(cmd, item.mesh->indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                boundIB = item.mesh->indexBuffer;
            }

            vkCmdDrawIndexed(
                cmd,
                item.mesh->indexCount,
                1,
                0,
                0,
                i
            );
        }
    }
}

} // namespace eng::renderer
