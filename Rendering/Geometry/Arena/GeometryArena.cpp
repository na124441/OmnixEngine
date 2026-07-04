#include "GeometryArena.h"
#include "Core/Engine/EngineResources.h"
#include "Core/Engine/VmaHelpers.h"
#include "Core/Vulkan/VkUtils.h"
#include "Core/Engine/Log.h"
#include <cstring>
#include <algorithm>

namespace eng::renderer {

GeometryArena* GeometryArena::s_Instance = nullptr;
bool GeometryArena::s_Enabled = false;

GeometryArena::GeometryArena() {
    s_Instance = this;
}

GeometryArena::~GeometryArena() {
    Shutdown();
    if (s_Instance == this) {
        s_Instance = nullptr;
    }
}

void GeometryArena::Initialize(VkDevice device, VmaAllocator allocator, uint64_t initialVertexCap, uint64_t initialIndexCap) {
    m_Device = device;
    m_Allocator = allocator;
    m_VertexCapacity = initialVertexCap;
    m_IndexCapacity = initialIndexCap;
    m_GrowthCount = 0;

    m_VertexAllocator.Initialize(initialVertexCap);
    m_IndexAllocator.Initialize(initialIndexCap);
    m_Uploader.Initialize(device, allocator);

    AllocateGPUBuffer(m_VertexCapacity, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, m_VertexBuffer, m_VertexAlloc);
    AllocateGPUBuffer(m_IndexCapacity, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, m_IndexBuffer, m_IndexAlloc);

    m_AllocationTable.clear();
    m_FreeSlots.clear();
    m_DeferredFrees.clear();
    m_StagingReleases.clear();

    s_Enabled = true;
    LOG_INFO("[GeometryArena] Initialized: Vertex capacity = " + std::to_string(initialVertexCap) + " bytes, Index capacity = " + std::to_string(initialIndexCap) + " bytes.");
}

void GeometryArena::Shutdown() {
    std::lock_guard<std::mutex> lock(m_ArenaMutex);
    
    m_Uploader.Shutdown();

    // Process all deferred frees immediately
    for (const auto& item : m_DeferredFrees) {
        if (item.type == DeferredFreeItem::Type::Buffer) {
            if (item.buffer != VK_NULL_HANDLE) {
                destroyBufferVMA(m_Allocator, item.buffer, item.allocation);
            }
        }
    }
    m_DeferredFrees.clear();
    m_StagingReleases.clear();

    if (m_VertexBuffer != VK_NULL_HANDLE) {
        destroyBufferVMA(m_Allocator, m_VertexBuffer, m_VertexAlloc);
        m_VertexBuffer = VK_NULL_HANDLE;
        m_VertexAlloc = VK_NULL_HANDLE;
    }

    if (m_IndexBuffer != VK_NULL_HANDLE) {
        destroyBufferVMA(m_Allocator, m_IndexBuffer, m_IndexAlloc);
        m_IndexBuffer = VK_NULL_HANDLE;
        m_IndexAlloc = VK_NULL_HANDLE;
    }

    m_AllocationTable.clear();
    m_FreeSlots.clear();
    s_Enabled = false;
    LOG_INFO("[GeometryArena] Shutdown complete.");
}

void GeometryArena::BeginFrame(uint32_t currentFrameIndex) {
    std::lock_guard<std::mutex> lock(m_ArenaMutex);

    // Release staging ring regions
    auto stagingIt = m_StagingReleases.begin();
    while (stagingIt != m_StagingReleases.end()) {
        if (stagingIt->frameIndex == currentFrameIndex) {
            m_Uploader.ReleaseSpace(stagingIt->stagingOffset);
            stagingIt = m_StagingReleases.erase(stagingIt);
        } else {
            ++stagingIt;
        }
    }

    // Process deferred buffer and allocation frees
    auto freeIt = m_DeferredFrees.begin();
    while (freeIt != m_DeferredFrees.end()) {
        if (freeIt->frameIndex == currentFrameIndex) {
            if (freeIt->type == DeferredFreeItem::Type::Buffer) {
                destroyBufferVMA(m_Allocator, freeIt->buffer, freeIt->allocation);
            } else if (freeIt->type == DeferredFreeItem::Type::Allocation) {
                m_VertexAllocator.Free(freeIt->vertexOffset, freeIt->vertexSize);
                m_IndexAllocator.Free(freeIt->indexOffset, freeIt->indexSize);
            }
            freeIt = m_DeferredFrees.erase(freeIt);
        } else {
            ++freeIt;
        }
    }
}

bool GeometryArena::Allocate(
    EngineResources& eng,
    const void* vertices,
    size_t vertexCount,
    size_t vertexStride,
    const uint32_t* indices,
    size_t indexCount,
    GeometryHandle& outHandle
) {
    uint64_t vSize = vertexCount * vertexStride;
    uint64_t iSize = indexCount * sizeof(uint32_t);

    uint64_t vOffset = 0;
    uint64_t iOffset = 0;

    std::unique_lock<std::mutex> lock(m_ArenaMutex);

    // Allocate Vertex Space (Grow if necessary)
    while (!m_VertexAllocator.Allocate(vSize, vertexStride, vOffset)) {
        GrowVertexBuffer(eng, vSize);
    }

    // Allocate Index Space (Grow if necessary)
    while (!m_IndexAllocator.Allocate(iSize, sizeof(uint32_t), iOffset)) {
        GrowIndexBuffer(eng, iSize);
    }

    // Reserve Staging Space
    uint64_t stagingOffset = 0;
    // Align to 16 bytes for safe copying
    uint64_t totalSize = vSize + iSize;
    if (!m_Uploader.Reserve(totalSize, 16, stagingOffset)) {
        // Staging buffer is full. For simple sync, wait idle and reclaim
        lock.unlock();
        vkDeviceWaitIdle(m_Device);
        lock.lock();
        m_Uploader.ReleaseSpace(m_Uploader.GetCapacity()); // Reset head
        if (!m_Uploader.Reserve(totalSize, 16, stagingOffset)) {
            LOG_ERROR("[GeometryArena] Failed to allocate staging uploader memory even after device wait.");
            m_VertexAllocator.Free(vOffset, vSize);
            m_IndexAllocator.Free(iOffset, iSize);
            return false;
        }
    }

    // Copy to Staging
    void* stagePtr = m_Uploader.GetMappedData(stagingOffset);
    std::memcpy(stagePtr, vertices, static_cast<size_t>(vSize));
    std::memcpy(static_cast<char*>(stagePtr) + vSize, indices, static_cast<size_t>(iSize));

    // Upload using single-time command buffer
    // Unlock to allow other commands to record while we submit synchronously
    lock.unlock();
    
    VkCommandBuffer copyCmd = eng.beginSingleTimeCommands();

    VkBufferCopy vCopy{};
    vCopy.srcOffset = stagingOffset;
    vCopy.dstOffset = vOffset;
    vCopy.size      = vSize;
    vkCmdCopyBuffer(copyCmd, m_Uploader.GetStagingBuffer(), m_VertexBuffer, 1, &vCopy);

    VkBufferCopy iCopy{};
    iCopy.srcOffset = stagingOffset + vSize;
    iCopy.dstOffset = iOffset;
    iCopy.size      = iSize;
    vkCmdCopyBuffer(copyCmd, m_Uploader.GetStagingBuffer(), m_IndexBuffer, 1, &iCopy);

    eng.endSingleTimeCommands(copyCmd); // Blocks until finished

    lock.lock();

    m_Uploader.AddUploadedBytes(totalSize);

    // Queue staging buffer region release
    // In our synchronous path we could free immediately, but queueing matches frame timeline rules.
    uint32_t activeFrameIndex = 0; // Will be mapped to current swapchain or frame index in Renderer.cpp
    m_StagingReleases.push_back({stagingOffset + totalSize, activeFrameIndex});

    // Populate Allocation Record
    GeometryAllocation alloc{};
    alloc.vertexByteOffset = vOffset;
    alloc.vertexByteSize = vSize;
    alloc.indexByteOffset = iOffset;
    alloc.indexByteSize = iSize;
    alloc.firstIndex = static_cast<uint32_t>(iOffset / sizeof(uint32_t));
    alloc.vertexOffset = static_cast<int32_t>(vOffset / vertexStride);
    alloc.vertexCount = static_cast<uint32_t>(vertexCount);
    alloc.indexCount = static_cast<uint32_t>(indexCount);
    alloc.generation = 0;
    alloc.flags = 1; // 1 = Active

    uint32_t slot = 0;
    if (!m_FreeSlots.empty()) {
        slot = m_FreeSlots.back();
        m_FreeSlots.pop_back();
        alloc.generation = m_AllocationTable[slot].generation; // keep current generation count
        m_AllocationTable[slot] = alloc;
    } else {
        slot = static_cast<uint32_t>(m_AllocationTable.size());
        m_AllocationTable.push_back(alloc);
    }

    outHandle = GeometryHandle(slot, alloc.generation);
    return true;
}

void GeometryArena::Free(GeometryHandle handle) {
    if (!handle.IsValid()) return;

    std::lock_guard<std::mutex> lock(m_ArenaMutex);
    if (handle.index >= m_AllocationTable.size()) return;

    auto& alloc = m_AllocationTable[handle.index];
    if (alloc.flags == 0 || alloc.generation != handle.generation) {
        return; // Already freed or generation mismatch
    }

    // Mark inactive & increment generation for stale handle checks
    alloc.flags = 0;
    alloc.generation++;

    // Defer freeing GPU memory to prevent corruption of in-flight frames
    DeferredFreeItem item{};
    item.type = DeferredFreeItem::Type::Allocation;
    item.vertexOffset = alloc.vertexByteOffset;
    item.vertexSize = alloc.vertexByteSize;
    item.indexOffset = alloc.indexByteOffset;
    item.indexSize = alloc.indexByteSize;
    item.frameIndex = 0; // Reclaim after current frame finishes (safe next round)
    m_DeferredFrees.push_back(item);

    m_FreeSlots.push_back(handle.index);
}

bool GeometryArena::IsValid(GeometryHandle handle) const {
    std::lock_guard<std::mutex> lock(m_ArenaMutex);
    if (!handle.IsValid() || handle.index >= m_AllocationTable.size()) return false;
    const auto& alloc = m_AllocationTable[handle.index];
    return alloc.flags != 0 && alloc.generation == handle.generation;
}

const GeometryAllocation* GeometryArena::GetAllocation(GeometryHandle handle) const {
    std::lock_guard<std::mutex> lock(m_ArenaMutex);
    if (!handle.IsValid() || handle.index >= m_AllocationTable.size()) return nullptr;
    const auto& alloc = m_AllocationTable[handle.index];
    if (alloc.flags == 0 || alloc.generation != handle.generation) return nullptr;
    return &alloc;
}

ArenaStats GeometryArena::GetStats() const {
    std::lock_guard<std::mutex> lock(m_ArenaMutex);
    ArenaStats stats{};
    stats.totalVertexMemory = m_VertexCapacity;
    stats.totalIndexMemory = m_IndexCapacity;
    stats.usedVertexMemory = m_VertexAllocator.GetUsedSize();
    stats.usedIndexMemory = m_IndexAllocator.GetUsedSize();
    stats.freeVertexMemory = m_VertexAllocator.GetFreeSize();
    stats.freeIndexMemory = m_IndexAllocator.GetFreeSize();
    stats.vertexFragmentation = m_VertexAllocator.GetFragmentation();
    stats.indexFragmentation = m_IndexAllocator.GetFragmentation();
    stats.allocationCount = m_VertexAllocator.GetAllocationCount();
    stats.uploadBytesPerFrame = m_Uploader.GetBytesUploaded();
    stats.growthCount = m_GrowthCount;
    return stats;
}

bool GeometryArena::AllocateGPUBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& outBuffer, VmaAllocation& outAlloc) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkResult res = createBufferVMA(m_Allocator, &bufferInfo, &allocInfo, &outBuffer, &outAlloc);
    return res == VK_SUCCESS;
}

void GeometryArena::GrowVertexBuffer(EngineResources& eng, uint64_t requiredSize) {
    uint64_t newCapacity = m_VertexCapacity * 2;
    while (newCapacity < m_VertexCapacity + requiredSize) {
        newCapacity *= 2;
    }

    LOG_INFO("[GeometryArena] Growing vertex buffer from " + std::to_string(m_VertexCapacity) + " to " + std::to_string(newCapacity) + " bytes.");

    VkBuffer newBuffer = VK_NULL_HANDLE;
    VmaAllocation newAlloc = VK_NULL_HANDLE;
    AllocateGPUBuffer(newCapacity, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, newBuffer, newAlloc);

    // Copy old contents
    if (m_VertexBuffer != VK_NULL_HANDLE) {
        // Unlock while submitting copy
        m_ArenaMutex.unlock();
        VkCommandBuffer copyCmd = eng.beginSingleTimeCommands();
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size      = m_VertexCapacity;
        vkCmdCopyBuffer(copyCmd, m_VertexBuffer, newBuffer, 1, &copyRegion);
        eng.endSingleTimeCommands(copyCmd);
        m_ArenaMutex.lock();

        // Queue old buffer for deferred destruction
        DeferredFreeItem freeItem{};
        freeItem.type = DeferredFreeItem::Type::Buffer;
        freeItem.buffer = m_VertexBuffer;
        freeItem.allocation = m_VertexAlloc;
        freeItem.frameIndex = 0; // Wait next round
        m_DeferredFrees.push_back(freeItem);
    }

    m_VertexBuffer = newBuffer;
    m_VertexAlloc = newAlloc;
    m_VertexCapacity = newCapacity;
    m_VertexAllocator.Grow(newCapacity);
    m_GrowthCount++;
}

void GeometryArena::GrowIndexBuffer(EngineResources& eng, uint64_t requiredSize) {
    uint64_t newCapacity = m_IndexCapacity * 2;
    while (newCapacity < m_IndexCapacity + requiredSize) {
        newCapacity *= 2;
    }

    LOG_INFO("[GeometryArena] Growing index buffer from " + std::to_string(m_IndexCapacity) + " to " + std::to_string(newCapacity) + " bytes.");

    VkBuffer newBuffer = VK_NULL_HANDLE;
    VmaAllocation newAlloc = VK_NULL_HANDLE;
    AllocateGPUBuffer(newCapacity, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, newBuffer, newAlloc);

    // Copy old contents
    if (m_IndexBuffer != VK_NULL_HANDLE) {
        m_ArenaMutex.unlock();
        VkCommandBuffer copyCmd = eng.beginSingleTimeCommands();
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size      = m_IndexCapacity;
        vkCmdCopyBuffer(copyCmd, m_IndexBuffer, newBuffer, 1, &copyRegion);
        eng.endSingleTimeCommands(copyCmd);
        m_ArenaMutex.lock();

        DeferredFreeItem freeItem{};
        freeItem.type = DeferredFreeItem::Type::Buffer;
        freeItem.buffer = m_IndexBuffer;
        freeItem.allocation = m_IndexAlloc;
        freeItem.frameIndex = 0;
        m_DeferredFrees.push_back(freeItem);
    }

    m_IndexBuffer = newBuffer;
    m_IndexAlloc = newAlloc;
    m_IndexCapacity = newCapacity;
    m_IndexAllocator.Grow(newCapacity);
    m_GrowthCount++;
}

} // namespace eng::renderer
