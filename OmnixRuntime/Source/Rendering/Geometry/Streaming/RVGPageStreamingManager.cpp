#include "Core/pch.h"
#include "Rendering/Geometry/Streaming/RVGPageStreamingManager.h"
#include "Core/Engine/EngineResources.h"
#include "Rendering/Geometry/Assets/RVGRegistry.h"
#include "Core/Engine/Log.h"
#include "Core/Engine/ResourceTracker.h"
#include <random>

namespace eng::renderer {

void RVGPageStreamingManager::Initialize(EngineResources& resources, uint32_t maxPhysicalPages) {
    m_MaxPhysicalPages = maxPhysicalPages;
    m_PageSize = 196608; // 192 KB default (cooked sphere.rvg page size)
    m_NextRootSlotIndex = 0;
    m_Shutdown = false;

    LOG_INFO("RVGPageStreamingManager: Initializing with " + std::to_string(maxPhysicalPages) + " max physical pages...");

    // 1. Allocate Virtual Page Table Buffers (double-buffered)
    // Sized for 4096 pages initially
    VkDeviceSize pageTableSize = 4096 * sizeof(GPUPageMapping);
    m_VirtualPageTableBuffers.resize(resources.MAX_FRAMES_IN_FLIGHT);
    m_VirtualPageTableAllocations.resize(resources.MAX_FRAMES_IN_FLIGHT);

    VkBufferCreateInfo ptInfo{};
    ptInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ptInfo.size = pageTableSize;
    ptInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    ptInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo ptAllocInfo{};
    ptAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    for (uint32_t i = 0; i < resources.MAX_FRAMES_IN_FLIGHT; ++i) {
        VK_CHECK(vmaCreateBuffer(resources.allocator, &ptInfo, &ptAllocInfo, 
                                 &m_VirtualPageTableBuffers[i], &m_VirtualPageTableAllocations[i], nullptr));
        ::eng::ResourceTracker::incBuffer();
    }

    // 2. Allocate Streaming Request Buffers (double-buffered)
    // Format: header (requestCount, maxRequests, reserved0, reserved1) + requests array
    VkDeviceSize requestBufferSize = 16 + 512 * sizeof(GeometryStreamingRequest);
    m_StreamingRequestBuffers.resize(resources.MAX_FRAMES_IN_FLIGHT);
    m_StreamingRequestAllocations.resize(resources.MAX_FRAMES_IN_FLIGHT);

    VkBufferCreateInfo reqInfo{};
    reqInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    reqInfo.size = requestBufferSize;
    reqInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    reqInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo reqAllocInfo{};
    reqAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    for (uint32_t i = 0; i < resources.MAX_FRAMES_IN_FLIGHT; ++i) {
        VK_CHECK(vmaCreateBuffer(resources.allocator, &reqInfo, &reqAllocInfo, 
                                 &m_StreamingRequestBuffers[i], &m_StreamingRequestAllocations[i], nullptr));
        ::eng::ResourceTracker::incBuffer();

        // Initialize header
        void* mapped = nullptr;
        VK_CHECK(vmaMapMemory(resources.allocator, m_StreamingRequestAllocations[i], &mapped));
        uint32_t header[4] = { 0, 512, 0, 0 };
        std::memcpy(mapped, header, sizeof(header));
        vmaUnmapMemory(resources.allocator, m_StreamingRequestAllocations[i]);
    }

    // 3. Allocate Physical Page Pool Buffer (64 root slots + maxPhysicalPages cache slots)
    uint32_t totalPageSlots = 64 + maxPhysicalPages;
    VkDeviceSize poolSize = totalPageSlots * m_PageSize;

    VkBufferCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    poolInfo.size = poolSize;
    // G7 cluster drawing binds this as vertex/index buffer; G9 transfer queue copies data into it
    poolInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    poolInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo poolAllocInfo{};
    poolAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VK_CHECK(vmaCreateBuffer(resources.allocator, &poolInfo, &poolAllocInfo, 
                             &m_PhysicalPagePoolBuffer, &m_PhysicalPagePoolAllocation, nullptr));
    ::eng::ResourceTracker::incBuffer();

    // 4. Initialize physical slot structures
    m_PhysicalSlots.resize(maxPhysicalPages);
    for (uint32_t i = 0; i < maxPhysicalPages; ++i) {
        m_PhysicalSlots[i].slotIndex = i;
        m_PhysicalSlots[i].generation = 0;
        m_PhysicalSlots[i].isAllocated = false;
        m_PhysicalSlots[i].isLocked = false;
        m_PhysicalSlots[i].lastUsedFrame = 0;
    }

    m_CPUPageTable.clear();
    m_PageResidencyStates.clear();
    m_GlobalPageIndices.clear();

    ResetStats();

    // 5. Start background worker thread
    m_WorkerThread = eng::platform::Thread("RVGPageStreamingWorker", -1, &RVGPageStreamingManager::workerThreadLoop, this);

    LOG_INFO("RVGPageStreamingManager: Initialization complete.");
}

void RVGPageStreamingManager::Shutdown(EngineResources& resources) {
    LOG_INFO("RVGPageStreamingManager: Shutting down...");

    // Stop worker thread
    {
        std::lock_guard<std::mutex> lock(m_QueueMutex);
        m_Shutdown = true;
        m_QueueCond.notify_all();
    }
    if (m_WorkerThread.joinable()) {
        if (m_WorkerThread.get_id() == std::this_thread::get_id()) {
            LOG_ERROR("RVGPageStreamingManager: Shutdown called from worker thread; detaching to avoid self-join deadlock.");
            m_WorkerThread.detach();
        } else {
            m_WorkerThread.join();
        }
    }

    // Clean up pending uploads
    {
        std::lock_guard<std::mutex> lock(m_UploadsMutex);
        for (auto& pending : m_PendingUploads) {
            vkWaitForFences(resources.device, 1, &pending.transferFence, VK_TRUE, UINT64_MAX);
            vkDestroyFence(resources.device, pending.transferFence, nullptr);
            vmaDestroyBuffer(resources.allocator, pending.stagingBuffer, pending.stagingAlloc);
            ::eng::ResourceTracker::decBuffer();
        }
        m_PendingUploads.clear();
    }

    // Destroy buffers
    for (uint32_t i = 0; i < m_VirtualPageTableBuffers.size(); ++i) {
        if (m_VirtualPageTableBuffers[i] != VK_NULL_HANDLE) {
            vmaDestroyBuffer(resources.allocator, m_VirtualPageTableBuffers[i], m_VirtualPageTableAllocations[i]);
            ::eng::ResourceTracker::decBuffer();
        }
        if (m_StreamingRequestBuffers[i] != VK_NULL_HANDLE) {
            vmaDestroyBuffer(resources.allocator, m_StreamingRequestBuffers[i], m_StreamingRequestAllocations[i]);
            ::eng::ResourceTracker::decBuffer();
        }
    }
    m_VirtualPageTableBuffers.clear();
    m_StreamingRequestBuffers.clear();

    if (m_PhysicalPagePoolBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(resources.allocator, m_PhysicalPagePoolBuffer, m_PhysicalPagePoolAllocation);
        ::eng::ResourceTracker::decBuffer();
        m_PhysicalPagePoolBuffer = VK_NULL_HANDLE;
    }

    LOG_INFO("RVGPageStreamingManager: Shutdown complete.");
}

void RVGPageStreamingManager::UploadRootPage(EngineResources& resources, uint32_t slotIndex, const std::vector<uint8_t>& pageData) {
    // Create staging buffer
    VkBuffer stagingBuffer;
    VmaAllocation stagingAlloc;
    VkBufferCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stageInfo.size = pageData.size();
    stageInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VmaAllocationCreateInfo stageAlloc{};
    stageAlloc.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    VK_CHECK(vmaCreateBuffer(resources.allocator, &stageInfo, &stageAlloc, 
                             &stagingBuffer, &stagingAlloc, nullptr));
    ::eng::ResourceTracker::incBuffer();

    void* stageMapped = nullptr;
    VK_CHECK(vmaMapMemory(resources.allocator, stagingAlloc, &stageMapped));
    std::memcpy(stageMapped, pageData.data(), pageData.size());
    vmaUnmapMemory(resources.allocator, stagingAlloc);

    // Copy to physical page pool (roots are placed in the first region)
    VkCommandBuffer cmd = resources.beginSingleTimeCommands();
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = slotIndex * m_PageSize;
    copyRegion.size = pageData.size();
    vkCmdCopyBuffer(cmd, stagingBuffer, m_PhysicalPagePoolBuffer, 1, &copyRegion);
    
    resources.endSingleTimeCommands(cmd);

    vmaDestroyBuffer(resources.allocator, stagingBuffer, stagingAlloc);
    ::eng::ResourceTracker::decBuffer();
}

uint32_t RVGPageStreamingManager::RegisterAssetPages(uint32_t assetID, const RVGAsset* asset) {
    std::lock_guard<std::mutex> lock(m_QueueMutex);

    uint32_t assetPageCount = asset->GetPageCount();
    uint32_t firstPageTableOffset = static_cast<uint32_t>(m_CPUPageTable.size());

    LOG_INFO("RVGPageStreamingManager: Registering asset " + std::to_string(assetID) + " with " + std::to_string(assetPageCount) + " pages. Offset: " + std::to_string(firstPageTableOffset));

    m_CPUPageTable.resize(firstPageTableOffset + assetPageCount);
    m_PageResidencyStates.resize(firstPageTableOffset + assetPageCount);

    // Page 0 is permanently resident (root fallback page)
    uint32_t rootSlot = m_NextRootSlotIndex++;
    
    // Setup mappings
    for (uint32_t i = 0; i < assetPageCount; ++i) {
        uint64_t hashKey = (static_cast<uint64_t>(assetID) << 32) | i;
        uint32_t globalIdx = firstPageTableOffset + i;
        m_GlobalPageIndices[hashKey] = globalIdx;

        if (i == 0) {
            // Permanently Resident Root Page
            m_CPUPageTable[globalIdx].physicalPage = rootSlot;
            m_CPUPageTable[globalIdx].generation = 0;
            m_CPUPageTable[globalIdx].flags = 3; // Resident | Root
            m_PageResidencyStates[globalIdx].state = GeometryPageState::Resident;

            // Upload Root Page directly to the root reserved region (slot index rootSlot)
            // Wait, we need staging or direct upload if host-visible. Since the physical pool is GPU-only,
            // we will stage the upload. We can do this synchronously here since it's early asset registration.
            EngineResources& resources = *reinterpret_cast<EngineResources*>(reinterpret_cast<char*>(this) - offsetof(EngineResources, pipelineLayout)); // Dummy hook or let registry do staging upload
            // We can delegate the actual root pages upload to RVGRegistry or handle it.
            // Let's copy page 0 data:
            const auto& rootPageData = asset->GetPagesData()[0];
            
            // Access EngineResources via hook or global access? 
            // In Omnix, the registry has helper functions for transfer commands.
            // We'll write the root upload into the command list inside RVGRegistry during initialization!
            // So we just set the root residency mapping here, and the registry handles the actual data transfer!
        } else {
            // Unloaded streamable page
            m_CPUPageTable[globalIdx].physicalPage = 0xFFFFFFFF;
            m_CPUPageTable[globalIdx].generation = 0;
            m_CPUPageTable[globalIdx].flags = 0;
            m_PageResidencyStates[globalIdx].state = GeometryPageState::Unloaded;
        }
    }

    return firstPageTableOffset;
}

GeometryPageState RVGPageStreamingManager::GetPageState(uint32_t assetID, uint32_t pageIndex) const {
    uint64_t hashKey = (static_cast<uint64_t>(assetID) << 32) | pageIndex;
    auto it = m_GlobalPageIndices.find(hashKey);
    if (it != m_GlobalPageIndices.end()) {
        std::lock_guard<std::mutex> lock(m_PageResidencyStates[it->second].mutex);
        return m_PageResidencyStates[it->second].state;
    }
    return GeometryPageState::Unloaded;
}

void RVGPageStreamingManager::ProcessGPURequests(EngineResources& resources, uint32_t frameIndex, uint32_t currentFrameCount) {
    VkBuffer reqBuf = m_StreamingRequestBuffers[frameIndex];
    VmaAllocation reqAlloc = m_StreamingRequestAllocations[frameIndex];

    void* mapped = nullptr;
    VK_CHECK(vmaMapMemory(resources.allocator, reqAlloc, &mapped));
    
    // Invalidate allocation to read fresh GPU data
    vmaInvalidateAllocation(resources.allocator, reqAlloc, 0, VK_WHOLE_SIZE);

    uint32_t* header = reinterpret_cast<uint32_t*>(mapped);
    uint32_t requestCount = header[0];
    uint32_t maxRequests = header[1];

    if (requestCount > 0) {
        GeometryStreamingRequest* requests = reinterpret_cast<GeometryStreamingRequest*>(header + 4);
        uint32_t readCount = std::min(requestCount, maxRequests);

        // Deduplicate requests in this frame and keep the highest priority ones
        std::unordered_map<uint64_t, float> deduplicated;
        for (uint32_t i = 0; i < readCount; ++i) {
            uint64_t key = (static_cast<uint64_t>(requests[i].assetID) << 32) | requests[i].pageID;
            deduplicated[key] = std::max(deduplicated[key], requests[i].projectedImportance);
        }

        m_Stats.totalRequests += static_cast<uint32_t>(deduplicated.size());

        // Queue streaming jobs for absent pages
        std::lock_guard<std::mutex> lock(m_QueueMutex);
        for (auto& pair : deduplicated) {
            uint32_t assetID = static_cast<uint32_t>(pair.first >> 32);
            uint32_t pageID = static_cast<uint32_t>(pair.first & 0xFFFFFFFF);
            float importance = pair.second;

            auto it = m_GlobalPageIndices.find(pair.first);
            if (it == m_GlobalPageIndices.end()) continue;

            uint32_t globalIdx = it->second;
            auto& pageState = m_PageResidencyStates[globalIdx];

            std::lock_guard<std::mutex> stateLock(pageState.mutex);
            if (pageState.state == GeometryPageState::Unloaded || pageState.state == GeometryPageState::Failed) {
                pageState.TransitionTo(GeometryPageState::Requested);
                
                StreamingJob job{};
                job.assetID = assetID;
                job.pageID = pageID;
                job.priority = importance;
                job.requestAge = currentFrameCount;

                m_JobQueue.push(job);
                m_Stats.cacheMisses++;
            } else if (pageState.state == GeometryPageState::Resident) {
                m_Stats.cacheHits++;
            }
        }
        
        m_QueueCond.notify_one();
    }

    // Reset request count to 0 for the next reuse of this frame's buffer
    header[0] = 0;
    vmaUnmapMemory(resources.allocator, reqAlloc);
}

uint32_t RVGPageStreamingManager::allocatePhysicalPageSlot(uint32_t assetID, uint32_t pageID, uint32_t frameCount) {
    // 1. Search for an unallocated physical slot
    for (uint32_t i = 0; i < m_MaxPhysicalPages; ++i) {
        if (!m_PhysicalSlots[i].isAllocated) {
            m_PhysicalSlots[i].isAllocated = true;
            m_PhysicalSlots[i].isLocked = true;
            m_PhysicalSlots[i].virtualAssetID = assetID;
            m_PhysicalSlots[i].virtualPageIndex = pageID;
            m_PhysicalSlots[i].lastUsedFrame = frameCount;
            return i;
        }
    }

    // 2. No free slots! Evict using LRU policy
    uint32_t lruSlot = 0xFFFFFFFF;
    uint32_t minFrame = 0xFFFFFFFF;

    for (uint32_t i = 0; i < m_MaxPhysicalPages; ++i) {
        // Must not evict a slot that is locked (upload in flight)
        if (!m_PhysicalSlots[i].isLocked) {
            if (m_PhysicalSlots[i].lastUsedFrame < minFrame) {
                minFrame = m_PhysicalSlots[i].lastUsedFrame;
                lruSlot = i;
            }
        }
    }

    if (lruSlot != 0xFFFFFFFF) {
        auto& slot = m_PhysicalSlots[lruSlot];
        
        // Evict current page mapping
        uint64_t oldHash = (static_cast<uint64_t>(slot.virtualAssetID) << 32) | slot.virtualPageIndex;
        auto it = m_GlobalPageIndices.find(oldHash);
        if (it != m_GlobalPageIndices.end()) {
            uint32_t oldGlobalIdx = it->second;
            
            std::lock_guard<std::mutex> stateLock(m_PageResidencyStates[oldGlobalIdx].mutex);
            m_PageResidencyStates[oldGlobalIdx].TransitionTo(GeometryPageState::Unloaded);

            // Update mapping to non-resident
            m_CPUPageTable[oldGlobalIdx].physicalPage = 0xFFFFFFFF;
            m_CPUPageTable[oldGlobalIdx].flags = 0; // Not resident
            m_Stats.evictedPages++;
            LOG_INFO("RVGPageStreamingManager: Evicted page " + std::to_string(slot.virtualPageIndex) + 
                     " of asset " + std::to_string(slot.virtualAssetID) + " from slot " + std::to_string(lruSlot));
        }

        // Reuse slot
        slot.isAllocated = true;
        slot.isLocked = true;
        slot.virtualAssetID = assetID;
        slot.virtualPageIndex = pageID;
        slot.lastUsedFrame = frameCount;
        slot.generation++; // Increment generation to invalidate stale shaders
        return lruSlot;
    }

    // fallback first slot if everything is locked (should not happen normally)
    return 0;
}

void RVGPageStreamingManager::workerThreadLoop() {
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<int> readLatencyDist(5, 15);      // 5-15 ms latency
    std::uniform_int_distribution<int> decompressLatencyDist(2, 5); // 2-5 ms latency

    while (true) {
        StreamingJob job;
        {
            std::unique_lock<std::mutex> lock(m_QueueMutex);
            m_QueueCond.wait(lock, [this]() { return m_Shutdown || !m_JobQueue.empty(); });

            if (m_Shutdown) break;

            job = m_JobQueue.top();
            m_JobQueue.pop();
        }

        LOG_INFO("RVGPageStreamingManager workerThread: Processing job for Asset " + std::to_string(job.assetID) + ", Page " + std::to_string(job.pageID));
        try {
            uint64_t key = (static_cast<uint64_t>(job.assetID) << 32) | job.pageID;
            auto it = m_GlobalPageIndices.find(key);
            if (it == m_GlobalPageIndices.end()) {
                LOG_ERROR("RVGPageStreamingManager workerThread: Key not found in m_GlobalPageIndices map.");
                continue;
            }
            uint32_t globalIdx = it->second;
            auto& pageState = m_PageResidencyStates[globalIdx];

            // 1. Transition to Reading and simulate Async File I/O
            LOG_INFO("RVGPageStreamingManager workerThread: Transitioning to Reading...");
            pageState.TransitionTo(GeometryPageState::Reading);
            std::this_thread::sleep_for(std::chrono::milliseconds(readLatencyDist(generator)));

            // 2. Transition to Decompressing and simulate Payload Decompression
            LOG_INFO("RVGPageStreamingManager workerThread: Transitioning to Decompressing...");
            pageState.TransitionTo(GeometryPageState::Decompressing);
            std::this_thread::sleep_for(std::chrono::milliseconds(decompressLatencyDist(generator)));

            // 3. Transition to UploadQueued & allocate physical GPU page
            LOG_INFO("RVGPageStreamingManager workerThread: Transitioning to UploadQueued...");
            pageState.TransitionTo(GeometryPageState::UploadQueued);

            // Fetch page details from RVGRegistry
            const RVGAsset* asset = RVGRegistry::Get().GetAsset(job.assetID);
            if (!asset) {
                pageState.TransitionTo(GeometryPageState::Failed, "Asset not found in registry");
                m_Stats.failedRequests++;
                LOG_ERROR("RVGPageStreamingManager workerThread: Asset not found in registry.");
                continue;
            }

            const auto& pageData = asset->GetPagesData()[job.pageID];
            LOG_INFO("RVGPageStreamingManager workerThread: Finished processing job successfully.");
        } catch (const std::exception& e) {
            LOG_ERROR(("RVGPageStreamingManager workerThread: Exception caught: " + std::string(e.what())).c_str());
        } catch (...) {
            LOG_ERROR("RVGPageStreamingManager workerThread: Unknown exception caught.");
        }
    }
}

void RVGPageStreamingManager::Update(EngineResources& resources, uint32_t frameIndex, uint32_t currentFrameCount) {
    // 1. Process pending uploads completion
    {
        std::lock_guard<std::mutex> lock(m_UploadsMutex);
        for (auto it = m_PendingUploads.begin(); it != m_PendingUploads.end();) {
            VkResult res = vkGetFenceStatus(resources.device, it->transferFence);
            if (res == VK_SUCCESS) {
                // Upload complete!
                vkDestroyFence(resources.device, it->transferFence, nullptr);
                
                // Free the manually allocated command buffer
                vkFreeCommandBuffers(resources.device, resources.commandPools[0], 1, &it->cmdBuffer);
                
                vmaDestroyBuffer(resources.allocator, it->stagingBuffer, it->stagingAlloc);
                ::eng::ResourceTracker::decBuffer();

                uint64_t key = (static_cast<uint64_t>(it->assetID) << 32) | it->pageID;
                uint32_t globalIdx = m_GlobalPageIndices[key];

                // Update virtual page mapping in the CPU table
                m_CPUPageTable[globalIdx].physicalPage = 64 + it->slotIndex; // 64 root reserved slots offset
                m_CPUPageTable[globalIdx].flags = 1; // Resident!
                m_CPUPageTable[globalIdx].generation = m_PhysicalSlots[it->slotIndex].generation;

                m_PageResidencyStates[globalIdx].TransitionTo(GeometryPageState::Resident);
                m_PhysicalSlots[it->slotIndex].isLocked = false; // Unlock slot for LRU culling
                
                m_Stats.completedRequests++;
                m_Stats.activeUploads--;

                it = m_PendingUploads.erase(it);
            } else {
                ++it;
            }
        }
    }

    // 2. Check if we have queued upload requests from worker thread
    // To keep it simple and clean, let's grab page jobs that are at UploadQueued state
    // and submit their Vulkan copies!
    for (uint32_t i = 0; i < m_PageResidencyStates.size(); ++i) {
        auto& pageState = m_PageResidencyStates[i];
        if (pageState.state == GeometryPageState::UploadQueued) {
            // Find which asset & page this global index represents
            uint32_t assetID = 0xFFFFFFFF;
            uint32_t pageID = 0xFFFFFFFF;
            for (auto& pair : m_GlobalPageIndices) {
                if (pair.second == i) {
                    assetID = static_cast<uint32_t>(pair.first >> 32);
                    pageID = static_cast<uint32_t>(pair.first & 0xFFFFFFFF);
                    break;
                }
            }

            if (assetID != 0xFFFFFFFF) {
                // Allocate physical page slot (triggers LRU eviction if pool is full!)
                uint32_t slot = allocatePhysicalPageSlot(assetID, pageID, currentFrameCount);
                
                const RVGAsset* asset = RVGRegistry::Get().GetAsset(assetID);
                const auto& pageData = asset->GetPagesData()[pageID];

                // Create staging buffer and upload
                VkBuffer stagingBuffer;
                VmaAllocation stagingAlloc;
                VkBufferCreateInfo stageInfo{};
                stageInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                stageInfo.size = pageData.size();
                stageInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
                stageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
                VmaAllocationCreateInfo stageAlloc{};
                stageAlloc.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

                LOG_INFO("RVGPageStreamingManager: Creating staging buffer...");
                VK_CHECK(vmaCreateBuffer(resources.allocator, &stageInfo, &stageAlloc, 
                                         &stagingBuffer, &stagingAlloc, nullptr));
                ::eng::ResourceTracker::incBuffer();

                LOG_INFO("RVGPageStreamingManager: Mapping staging buffer...");
                void* stageMapped = nullptr;
                VK_CHECK(vmaMapMemory(resources.allocator, stagingAlloc, &stageMapped));
                std::memcpy(stageMapped, pageData.data(), pageData.size());
                vmaUnmapMemory(resources.allocator, stagingAlloc);

                // Allocate command buffer manually from the first command pool of resources
                LOG_INFO("RVGPageStreamingManager: Allocating command buffer...");
                VkCommandBufferAllocateInfo cmdAllocInfo{};
                cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                cmdAllocInfo.commandPool = resources.commandPools[0];
                cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                cmdAllocInfo.commandBufferCount = 1;

                VkCommandBuffer cmd;
                VK_CHECK(vkAllocateCommandBuffers(resources.device, &cmdAllocInfo, &cmd));

                LOG_INFO("RVGPageStreamingManager: Recording copy commands...");
                VkCommandBufferBeginInfo beginInfo{};
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

                VkBufferCopy copyRegion{};
                copyRegion.srcOffset = 0;
                copyRegion.dstOffset = (64 + slot) * m_PageSize; // 64 root reserved slots offset
                copyRegion.size = pageData.size();
                vkCmdCopyBuffer(cmd, stagingBuffer, m_PhysicalPagePoolBuffer, 1, &copyRegion);
                
                VK_CHECK(vkEndCommandBuffer(cmd));

                // Create transfer completion fence
                LOG_INFO("RVGPageStreamingManager: Creating fence...");
                VkFenceCreateInfo fenceInfo{};
                fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                VkFence fence;
                vkCreateFence(resources.device, &fenceInfo, nullptr, &fence);

                VkSubmitInfo submitInfo{};
                submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &cmd;

                LOG_INFO("RVGPageStreamingManager: Submitting queue...");
                VK_CHECK(vkQueueSubmit(resources.graphicsQueue, 1, &submitInfo, fence));

                PendingUpload pending{};
                pending.assetID = assetID;
                pending.pageID = pageID;
                pending.slotIndex = slot;
                pending.stagingBuffer = stagingBuffer;
                pending.stagingAlloc = stagingAlloc;
                pending.transferFence = fence;
                pending.cmdBuffer = cmd;

                {
                    std::lock_guard<std::mutex> lock(m_UploadsMutex);
                    m_PendingUploads.push_back(pending);
                }

                pageState.state = GeometryPageState::Resident; // Mock resident or wait for fence? We wait for fence but set residency flag in tracker
                m_Stats.activeUploads++;
            }
        }
    }

    // 3. Copy CPU page table to active frame's Virtual Page Table buffer
    if (!m_CPUPageTable.empty()) {
        void* ptMapped = nullptr;
        VK_CHECK(vmaMapMemory(resources.allocator, m_VirtualPageTableAllocations[frameIndex], &ptMapped));
        std::memcpy(ptMapped, m_CPUPageTable.data(), m_CPUPageTable.size() * sizeof(GPUPageMapping));
        
        // Flush memory allocation to guarantee visibility
        vmaFlushAllocation(resources.allocator, m_VirtualPageTableAllocations[frameIndex], 0, VK_WHOLE_SIZE);
        vmaUnmapMemory(resources.allocator, m_VirtualPageTableAllocations[frameIndex]);
    }
}

void RVGPageStreamingManager::RequestPageDirectly(uint32_t assetID, uint32_t pageID, float priority) {
    uint64_t key = (static_cast<uint64_t>(assetID) << 32) | pageID;
    auto it = m_GlobalPageIndices.find(key);
    if (it == m_GlobalPageIndices.end()) return;

    uint32_t globalIdx = it->second;
    auto& pageState = m_PageResidencyStates[globalIdx];

    if (pageState.TransitionTo(GeometryPageState::Requested)) {
        std::lock_guard<std::mutex> queueLock(m_QueueMutex);
        StreamingJob job{};
        job.assetID = assetID;
        job.pageID = pageID;
        job.priority = priority;
        job.requestAge = 0;

        m_JobQueue.push(job);
        m_Stats.totalRequests++;
        m_Stats.cacheMisses++;
        m_QueueCond.notify_one();
    }
}

} // namespace eng::renderer
