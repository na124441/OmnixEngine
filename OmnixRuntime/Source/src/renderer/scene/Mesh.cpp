#include "src/renderer/scene/Mesh.h"
#include "engine/EngineResources.h"
#include "engine/VkResultCheck.h"
#include <cstring>

template<typename VertexT, typename IndexT>
bool Mesh::init(const VertexT* vertices, size_t vertexCount,
                const IndexT* indices,   size_t indexCount,
                const EngineResources& eng)
{
    // -------- Vertex Buffer (GPU‑local) --------
    VkDeviceSize vSize = sizeof(VertexT) * vertexCount;
    eng.createBuffer(vSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexMemory);

    // -------- Index Buffer (GPU‑local) --------
    VkDeviceSize iSize = sizeof(IndexT) * indexCount;
    eng.createBuffer(iSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexMemory);

    // -------- Staging buffer (host visible) --------
    VkDeviceSize totalSize = vSize + iSize;
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    eng.createBuffer(totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingMemory);

    // Map once and copy both vertex & index data
    void* data;
    VK_CHECK(vkMapMemory(eng.device, stagingMemory, 0, totalSize, 0, &data));
    std::memcpy(data, vertices, static_cast<size_t>(vSize));
    std::memcpy(static_cast<char*>(data) + vSize, indices, static_cast<size_t>(iSize));
    vkUnmapMemory(eng.device, stagingMemory);

    // -------- Copy staging → GPU buffers --------
    VkCommandBuffer copyCmd = eng.beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size      = vSize;
    vkCmdCopyBuffer(copyCmd, stagingBuffer, vertexBuffer, 1, &copyRegion);

    copyRegion.srcOffset = vSize;
    copyRegion.dstOffset = 0;
    copyRegion.size      = iSize;
    vkCmdCopyBuffer(copyCmd, stagingBuffer, indexBuffer, 1, &copyRegion);

    eng.endSingleTimeCommands(copyCmd);

    // Cleanup staging
    vkDestroyBuffer(eng.device, stagingBuffer, nullptr);
    vkFreeMemory(eng.device, stagingMemory, nullptr);
    ResourceTracker::decBuffer();

    vertexSize = vSize;
    indexSize  = iSize;
    this->indexCount = static_cast<uint32_t>(indexCount);
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
    if (eng.device == VK_NULL_HANDLE) return;

    if (vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(eng.device, vertexBuffer, nullptr);
        vkFreeMemory(eng.device, vertexMemory, nullptr);
        ResourceTracker::decBuffer();
        vertexBuffer = VK_NULL_HANDLE;
        vertexMemory = VK_NULL_HANDLE;
    }
    if (indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(eng.device, indexBuffer, nullptr);
        vkFreeMemory(eng.device, indexMemory, nullptr);
        ResourceTracker::decBuffer();
        indexBuffer = VK_NULL_HANDLE;
        indexMemory = VK_NULL_HANDLE;
    }
    indexCount = 0;
    vertexSize = indexSize = 0;
}

void Mesh::moveFrom(Mesh&& rhs)
{
    vertexBuffer = rhs.vertexBuffer;
    vertexMemory  = rhs.vertexMemory;
    vertexSize   = rhs.vertexSize;

    indexBuffer  = rhs.indexBuffer;
    indexMemory   = rhs.indexMemory;
    indexSize    = rhs.indexSize;

    indexCount   = rhs.indexCount;

    rhs.vertexBuffer = VK_NULL_HANDLE;
    rhs.indexBuffer   = VK_NULL_HANDLE;
    rhs.vertexMemory   = VK_NULL_HANDLE;
    rhs.indexMemory    = VK_NULL_HANDLE;
    rhs.indexCount   = 0;
}

// Explicit template instantiation
template bool Mesh::init(const Vertex*, size_t, const uint32_t*, size_t, const EngineResources&);
template bool Mesh::init(const glm::vec3*, size_t, const uint32_t*, size_t, const EngineResources&);
