#pragma once
#include <vulkan/vulkan.h>
#include "Core/Engine/VmaUsage.h"
#include "Rendering/Geometry/GeometryHandle.h"
#include "Rendering/Geometry/GeometryTypes.h"
#include "Rendering/Geometry/Arena/GeometryAllocator.h"
#include "Rendering/Geometry/Arena/GeometryUploader.h"
#include "Rendering/Geometry/Arena/GeometryArenaStats.h"
#include <vector>
#include <mutex>

namespace eng::renderer {

struct EngineResources;

struct StagingBatchRelease {
    uint64_t stagingOffset;
    uint32_t frameIndex;
};

struct DeferredFreeItem {
    enum class Type {
        Buffer,
        Allocation
    } type;
    
    // For Type::Buffer
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    
    // For Type::Allocation
    uint64_t vertexOffset = 0;
    uint64_t vertexSize = 0;
    uint64_t indexOffset = 0;
    uint64_t indexSize = 0;
    
    uint32_t frameIndex = 0;
};

class GeometryArena {
public:
    GeometryArena();
    ~GeometryArena();

    void Initialize(VkDevice device, VmaAllocator allocator, uint64_t initialVertexCap = 64 * 1024 * 1024, uint64_t initialIndexCap = 32 * 1024 * 1024);
    void Shutdown();

    // Begin a rendering frame, processes deferred frees for this frame slot
    void BeginFrame(uint32_t currentFrameIndex);

    // Allocate space in the arena, uploading vertex and index data
    bool Allocate(
        EngineResources& eng,
        const void* vertices,
        size_t vertexCount,
        size_t vertexStride,
        const uint32_t* indices,
        size_t indexCount,
        GeometryHandle& outHandle
    );

    // Free an allocation from the arena
    void Free(GeometryHandle handle);

    // Accessors
    VkBuffer GetVertexBuffer() const { return m_VertexBuffer; }
    VkBuffer GetIndexBuffer() const { return m_IndexBuffer; }

    bool IsValid(GeometryHandle handle) const;
    const GeometryAllocation* GetAllocation(GeometryHandle handle) const;

    ArenaStats GetStats() const;
    uint32_t GetTotalIndexCount() const { return static_cast<uint32_t>(m_IndexCapacity / sizeof(uint32_t)); }

    static bool IsInitialized() { return s_Instance != nullptr; }
    static bool IsEnabled() { return s_Enabled; }
    static void SetEnabled(bool enable) { s_Enabled = enable; }
    static GeometryArena* GetInstance() { return s_Instance; }

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    VmaAllocator m_Allocator = VK_NULL_HANDLE;

    VkBuffer m_VertexBuffer = VK_NULL_HANDLE;
    VmaAllocation m_VertexAlloc = VK_NULL_HANDLE;
    uint64_t m_VertexCapacity = 0;

    VkBuffer m_IndexBuffer = VK_NULL_HANDLE;
    VmaAllocation m_IndexAlloc = VK_NULL_HANDLE;
    uint64_t m_IndexCapacity = 0;

    GeometryAllocator m_VertexAllocator;
    GeometryAllocator m_IndexAllocator;
    GeometryUploader m_Uploader;

    std::vector<GeometryAllocation> m_AllocationTable;
    std::vector<uint32_t> m_FreeSlots;

    std::vector<StagingBatchRelease> m_StagingReleases;
    std::vector<DeferredFreeItem> m_DeferredFrees;

    uint32_t m_GrowthCount = 0;
    mutable std::mutex m_ArenaMutex;

    static GeometryArena* s_Instance;
    static bool s_Enabled;

    bool AllocateGPUBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& outBuffer, VmaAllocation& outAlloc);
    void GrowVertexBuffer(EngineResources& eng, uint64_t requiredSize);
    void GrowIndexBuffer(EngineResources& eng, uint64_t requiredSize);
};

} // namespace eng::renderer
