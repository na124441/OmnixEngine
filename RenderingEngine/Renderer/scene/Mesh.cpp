#include "Core/pch.h"
#include "Mesh.h"
#include "Core/Engine/EngineResources.h"
#include "Core/Engine/VmaHelpers.h"
#include "Core/Vulkan/VkUtils.h"
#include <cstring>

namespace eng::renderer {

template<typename VertexT, typename IndexT>
bool Mesh::init(const VertexT* vertices, size_t vertexCount,
                const IndexT* indices,   size_t indexCount,
                EngineResources& eng)
{
    // -----------------------------------------------------------------
    // 1️⃣ Determine sizes
    VkDeviceSize vSize = sizeof(VertexT) * vertexCount;
    VkDeviceSize iSize = sizeof(IndexT)   * indexCount;
    VkDeviceSize total = vSize + iSize;

    // -----------------------------------------------------------------
    // 2️⃣ Ensure we have a staging buffer large enough
    eng.ensureStagingBuffer(total);   // may realloc internally

    // -----------------------------------------------------------------
    // 3️⃣ Map the staging buffer and copy vertex + index data
    void* dst = nullptr;
    VK_CHECK(vmaMapMemory(eng.allocator,
                          eng.transfer.stagingAlloc,
                          &dst));
    std::memcpy(dst, vertices, static_cast<size_t>(vSize));
    std::memcpy(static_cast<char*>(dst) + vSize,
                indices, static_cast<size_t>(iSize));
    vmaUnmapMemory(eng.allocator, eng.transfer.stagingAlloc);

    // -----------------------------------------------------------------
    // 4️⃣ Create the *device‑local* vertex buffer (GPU only)
    VkBufferCreateInfo vInfo{};
    vInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vInfo.size  = vSize;
    vInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo vAllocInfo{};
    vAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkResult res = createBufferVMA(eng.allocator,
                                   &vInfo,
                                   &vAllocInfo,
                                   &vertexBuffer,
                                   &vertexAlloc);
    VK_CHECK(res);
    vertexSize = vSize;

    // -----------------------------------------------------------------
    // 5️⃣ Create the *device‑local* index buffer
    VkBufferCreateInfo iInfo{};
    iInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    iInfo.size  = iSize;
    iInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    iInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo iAllocInfo{};
    iAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    res = createBufferVMA(eng.allocator,
                          &iInfo,
                          &iAllocInfo,
                          &indexBuffer,
                          &indexAlloc);
    VK_CHECK(res);
    indexSize = iSize;
    this->indexCount = static_cast<uint32_t>(indexCount);

    // -----------------------------------------------------------------
    // 6️⃣ Copy data from staging → GPU buffers using a single‑time command buffer
    VkCommandBuffer copyCmd = eng.beginSingleTimeCommands();

    // Vertex copy
    VkBufferCopy vCopy{};
    vCopy.srcOffset = 0;
    vCopy.dstOffset = 0;
    vCopy.size      = vSize;
    vkCmdCopyBuffer(copyCmd, eng.transfer.stagingBuffer,
                    vertexBuffer, 1, &vCopy);

    // Index copy (starts after vertex region in staging)
    VkBufferCopy iCopy{};
    iCopy.srcOffset = vSize;
    iCopy.dstOffset = 0;
    iCopy.size      = iSize;
    vkCmdCopyBuffer(copyCmd, eng.transfer.stagingBuffer,
                    indexBuffer, 1, &iCopy);

    eng.endSingleTimeCommands(copyCmd);   // blocks until copy finishes

    LOG_INFO(("Mesh created – vertices: " + std::to_string(vertexCount) +
             ", indices: " + std::to_string(indexCount)).c_str());
    return true;
}

void Mesh::bind(VkCommandBuffer cmd) const
{
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer (cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
}

void Mesh::destroy()
{
    EngineResources& eng = EngineResources::get();
    if (eng.device == VK_NULL_HANDLE || eng.allocator == VK_NULL_HANDLE) return;

    if (vertexBuffer != VK_NULL_HANDLE) {
        destroyBufferVMA(eng.allocator, vertexBuffer, vertexAlloc);
        vertexBuffer = VK_NULL_HANDLE;
        vertexAlloc = VK_NULL_HANDLE;
    }
    if (indexBuffer != VK_NULL_HANDLE) {
        destroyBufferVMA(eng.allocator, indexBuffer, indexAlloc);
        indexBuffer = VK_NULL_HANDLE;
        indexAlloc = VK_NULL_HANDLE;
    }
    indexCount = 0;
    vertexSize = indexSize = 0;
}

void Mesh::moveFrom(Mesh&& rhs)
{
    vertexBuffer = rhs.vertexBuffer;
    vertexAlloc  = rhs.vertexAlloc;
    vertexSize   = rhs.vertexSize;

    indexBuffer  = rhs.indexBuffer;
    indexAlloc   = rhs.indexAlloc;
    indexSize    = rhs.indexSize;

    indexCount   = rhs.indexCount;

    rhs.vertexBuffer = VK_NULL_HANDLE;
    rhs.indexBuffer   = VK_NULL_HANDLE;
    rhs.vertexAlloc   = VK_NULL_HANDLE;
    rhs.indexAlloc    = VK_NULL_HANDLE;
    rhs.indexCount   = 0;
}

// Explicit template instantiation
template bool Mesh::init(const Vertex*, size_t, const uint32_t*, size_t, EngineResources&);
template bool Mesh::init(const PbrVertex*, size_t, const uint32_t*, size_t, EngineResources&);
template bool Mesh::init(const glm::vec3*, size_t, const uint32_t*, size_t, EngineResources&);

} // namespace eng::renderer
