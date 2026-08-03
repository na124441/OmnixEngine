#include "Rendering/Geometry/Arena/GeometryUploader.h"
#include "Core/Vulkan/VkUtils.h"
#include "Core/Engine/VmaHelpers.h"
#include "Core/Engine/Log.h"
#include <cstring>

namespace eng::renderer {

void GeometryUploader::Initialize(VkDevice device, VmaAllocator allocator, uint64_t capacity) {
    Shutdown();

    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Device = device;
    m_Allocator = allocator;
    m_Capacity = capacity;
    m_Head = 0;
    m_Tail = 0;
    m_BytesUploaded = 0;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = capacity;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkResult res = createBufferVMA(allocator, &bufferInfo, &allocInfo, &m_StagingBuffer, &m_StagingAlloc);
    if (res != VK_SUCCESS) {
        LOG_ERROR("GeometryUploader: Failed to create staging buffer.");
        return;
    }

    VmaAllocationInfo vmaInfo{};
    vmaGetAllocationInfo(allocator, m_StagingAlloc, &vmaInfo);
    m_MappedData = vmaInfo.pMappedData;
}

void GeometryUploader::Shutdown() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_StagingBuffer != VK_NULL_HANDLE) {
        destroyBufferVMA(m_Allocator, m_StagingBuffer, m_StagingAlloc);
        m_StagingBuffer = VK_NULL_HANDLE;
        m_StagingAlloc = VK_NULL_HANDLE;
    }
    m_MappedData = nullptr;
    m_Device = VK_NULL_HANDLE;
    m_Allocator = VK_NULL_HANDLE;
}

bool GeometryUploader::Reserve(uint64_t size, uint64_t alignment, uint64_t& outOffset) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (size > m_Capacity) {
        LOG_ERROR("GeometryUploader: Requested size exceeds uploader capacity.");
        return false;
    }

    uint64_t H = m_Head;
    uint64_t T = m_Tail;

    if (T >= H) {
        // Space from T to end
        uint64_t alignedOffset = T;
        if (alignment > 0 && T % alignment != 0) {
            alignedOffset = T + (alignment - (T % alignment));
        }

        if (alignedOffset + size <= m_Capacity) {
            outOffset = alignedOffset;
            m_Tail = alignedOffset + size;
            return true;
        }

        // Try wrapping around to the beginning
        alignedOffset = 0;
        if (size < H) { // Leave at least 1 byte separator to distinguish full vs empty
            outOffset = alignedOffset;
            m_Tail = size;
            return true;
        }
    } else {
        // Space from T to H
        uint64_t alignedOffset = T;
        if (alignment > 0 && T % alignment != 0) {
            alignedOffset = T + (alignment - (T % alignment));
        }

        if (alignedOffset + size < H) {
            outOffset = alignedOffset;
            m_Tail = alignedOffset + size;
            return true;
        }
    }

    return false;
}

void GeometryUploader::ReleaseSpace(uint64_t offset) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Head = offset % m_Capacity;
}

void* GeometryUploader::GetMappedData(uint64_t offset) const {
    if (!m_MappedData) return nullptr;
    return static_cast<char*>(m_MappedData) + offset;
}

} // namespace eng::renderer
