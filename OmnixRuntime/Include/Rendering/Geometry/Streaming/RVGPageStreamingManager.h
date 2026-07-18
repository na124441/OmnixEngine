#pragma once
#include <vector>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <unordered_map>
#include <atomic>
#include <memory>
#include "Rendering/Geometry/Streaming/GeometryPageTypes.h"
#include "Rendering/Geometry/Assets/RVGAsset.h"
#include "Core/Vulkan/VkUtils.h"
#include "Core/Engine/VmaUsage.h"

namespace eng::renderer {

struct EngineResources;

struct PhysicalPageSlot {
    uint32_t slotIndex = 0xFFFFFFFF;
    uint32_t lastUsedFrame = 0;
    uint32_t generation = 0;
    bool isAllocated = false;
    bool isLocked = false; // Locked while upload is in flight
    uint32_t virtualAssetID = 0xFFFFFFFF;
    uint32_t virtualPageIndex = 0xFFFFFFFF;
};

struct StreamingSchedulerStats {
    std::atomic<uint32_t> totalRequests{0};
    std::atomic<uint32_t> completedRequests{0};
    std::atomic<uint32_t> failedRequests{0};
    std::atomic<uint32_t> activeUploads{0};
    std::atomic<uint32_t> cacheHits{0};
    std::atomic<uint32_t> cacheMisses{0};
    std::atomic<uint32_t> evictedPages{0};
};

class RVGPageStreamingManager {
public:
    static RVGPageStreamingManager& Get() {
        static RVGPageStreamingManager instance;
        return instance;
    }

    void Initialize(EngineResources& resources, uint32_t maxPhysicalPages = 256);
    void Shutdown(EngineResources& resources);
    void UploadRootPage(EngineResources& resources, uint32_t slotIndex, const std::vector<uint8_t>& pageData);

    // Register a loaded asset's virtual pages
    uint32_t RegisterAssetPages(uint32_t assetID, const RVGAsset* asset);

    // Update table and process requests (called every frame)
    void Update(EngineResources& resources, uint32_t frameIndex, uint32_t currentFrameCount);

    // Read back requests from GPU buffer and queue streaming jobs
    void ProcessGPURequests(EngineResources& resources, uint32_t frameIndex, uint32_t currentFrameCount);

    // Getters for Vulkan buffers
    VkBuffer GetVirtualPageTableBuffer(uint32_t frameIndex) const {
        if (frameIndex >= m_VirtualPageTableBuffers.size()) return VK_NULL_HANDLE;
        return m_VirtualPageTableBuffers[frameIndex];
    }
    VkBuffer GetStreamingRequestBuffer(uint32_t frameIndex) const {
        if (frameIndex >= m_StreamingRequestBuffers.size()) return VK_NULL_HANDLE;
        return m_StreamingRequestBuffers[frameIndex];
    }
    VkBuffer GetPhysicalPagePoolBuffer() const { return m_PhysicalPagePoolBuffer; }

    uint32_t GetMaxPhysicalPages() const { return m_MaxPhysicalPages; }
    uint32_t GetRootPagesCount() const { return m_NextRootSlotIndex; }
    uint32_t GetPageSize() const { return m_PageSize; }

    const StreamingSchedulerStats& GetStats() const { return m_Stats; }
    void ResetStats() {
        m_Stats.totalRequests = 0;
        m_Stats.completedRequests = 0;
        m_Stats.failedRequests = 0;
        m_Stats.activeUploads = 0;
        m_Stats.cacheHits = 0;
        m_Stats.cacheMisses = 0;
        m_Stats.evictedPages = 0;
    }

    // Direct interface to check residency state
    GeometryPageState GetPageState(uint32_t assetID, uint32_t pageIndex) const;
    void RequestPageDirectly(uint32_t assetID, uint32_t pageID, float priority = 1.0f);

private:
    RVGPageStreamingManager() = default;
    ~RVGPageStreamingManager() = default;

    void updateGPUPageTable(EngineResources& resources, uint32_t frameIndex);
    
    // Background worker thread for IO & decompression
    void workerThreadLoop();

    struct StreamingJob {
        uint32_t assetID = 0xFFFFFFFF;
        uint32_t pageID = 0xFFFFFFFF;
        float priority = 0.0f;
        uint32_t requestAge = 0;
    };

    struct CompareJob {
        bool operator()(const StreamingJob& a, const StreamingJob& b) const {
            // Sort by priority desc, then age desc
            if (a.priority == b.priority) {
                return a.requestAge < b.requestAge;
            }
            return a.priority < b.priority;
        }
    };

    // Allocates a physical page slot using LRU eviction if necessary
    uint32_t allocatePhysicalPageSlot(uint32_t assetID, uint32_t pageID, uint32_t frameCount);
    void freePhysicalPageSlot(uint32_t slotIndex);

    // Buffers and allocations
    std::vector<VkBuffer> m_VirtualPageTableBuffers;
    std::vector<VmaAllocation> m_VirtualPageTableAllocations;

    std::vector<VkBuffer> m_StreamingRequestBuffers;
    std::vector<VmaAllocation> m_StreamingRequestAllocations;

    VkBuffer m_PhysicalPagePoolBuffer = VK_NULL_HANDLE;
    VmaAllocation m_PhysicalPagePoolAllocation = VK_NULL_HANDLE;

    uint32_t m_MaxPhysicalPages = 256;
    uint32_t m_PageSize = 196608; // 192 KB default (size of sphere.rvg pages)

    // Layout partitions
    uint32_t m_NextRootSlotIndex = 0;
    
    // Virtual to Physical table
    // Key: globalPageIndex = assetPageTableOffset + pageIndex
    std::vector<GPUPageMapping> m_CPUPageTable;
    std::vector<PageResidencyState> m_PageResidencyStates;
    std::unordered_map<uint64_t, uint32_t> m_GlobalPageIndices; // hash(assetID, pageIndex) -> globalPageIndex

    // Physical slots
    std::vector<PhysicalPageSlot> m_PhysicalSlots;

    // Async worker variables
    std::thread m_WorkerThread;
    std::mutex m_QueueMutex;
    std::condition_variable m_QueueCond;
    std::priority_queue<StreamingJob, std::vector<StreamingJob>, CompareJob> m_JobQueue;
    std::atomic<bool> m_Shutdown{false};

    // Staging and transfer variables
    struct PendingUpload {
        uint32_t assetID;
        uint32_t pageID;
        uint32_t slotIndex;
        VkBuffer stagingBuffer;
        VmaAllocation stagingAlloc;
        VkFence transferFence;
        VkCommandBuffer cmdBuffer;
    };
    std::vector<PendingUpload> m_PendingUploads;
    std::mutex m_UploadsMutex;

    StreamingSchedulerStats m_Stats;
};

} // namespace eng::renderer
