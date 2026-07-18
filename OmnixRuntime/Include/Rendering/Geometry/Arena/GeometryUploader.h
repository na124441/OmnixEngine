#pragma once
#include <vulkan/vulkan.h>
#include "Core/Engine/VmaUsage.h"
#include <cstdint>
#include <mutex>

namespace eng::renderer {

class GeometryUploader {
public:
    GeometryUploader() = default;
    ~GeometryUploader() { Shutdown(); }

    void Initialize(VkDevice device, VmaAllocator allocator, uint64_t capacity = 16 * 1024 * 1024);
    void Shutdown();

    // Reserve a contiguous space in the staging ring buffer
    bool Reserve(uint64_t size, uint64_t alignment, uint64_t& outOffset);
    
    // Release space up to a certain offset once GPU has completed reading it
    void ReleaseSpace(uint64_t offset);

    void* GetMappedData(uint64_t offset) const;
    VkBuffer GetStagingBuffer() const { return m_StagingBuffer; }
    uint64_t GetCapacity() const { return m_Capacity; }

    // Statistics
    uint64_t GetBytesUploaded() const { return m_BytesUploaded; }
    void ResetBandwidthStats() { m_BytesUploaded = 0; }
    void AddUploadedBytes(uint64_t bytes) { m_BytesUploaded += bytes; }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    VmaAllocator m_Allocator = VK_NULL_HANDLE;
    VkBuffer m_StagingBuffer = VK_NULL_HANDLE;
    VmaAllocation m_StagingAlloc = VK_NULL_HANDLE;
    void* m_MappedData = nullptr;

    uint64_t m_Capacity = 0;
    uint64_t m_Head = 0; // GPU read cursor
    uint64_t m_Tail = 0; // CPU write cursor
    uint64_t m_BytesUploaded = 0;

    mutable std::mutex m_Mutex;
};

} // namespace eng::renderer
