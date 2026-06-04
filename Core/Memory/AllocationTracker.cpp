#include "Core/Memory/AllocationTracker.h"
#include "Core/Logging/Logger.h"

namespace eng::memory {

    std::mutex& AllocationTracker::GetMutex() {
        static std::mutex s_Mutex;
        return s_Mutex;
    }

    std::unordered_map<void*, AllocationInfo>& AllocationTracker::GetAllocationsMap() {
        static std::unordered_map<void*, AllocationInfo> s_AllocationsMap;
        return s_AllocationsMap;
    }

    MemoryStatistics& AllocationTracker::GetStats() {
        static MemoryStatistics s_Stats;
        return s_Stats;
    }

    void AllocationTracker::RegisterAllocation(void* ptr, size_t size, const std::string& source) {
        if (!ptr) return;
        std::lock_guard<std::mutex> lock(GetMutex());
        
        auto& allocations = GetAllocationsMap();
        allocations[ptr] = { size, source };

        auto& stats = GetStats();
        stats.totalAllocated += size;
        stats.activeAllocations = allocations.size();
        stats.totalAllocationsCount++;
        if (stats.totalAllocated > stats.peakUsage) {
            stats.peakUsage = stats.totalAllocated;
        }

        LOG_TRACE("[Memory] Allocated %zu bytes in %s (Active: %zu bytes, peak: %zu bytes)",
                  size, source.c_str(), stats.totalAllocated, stats.peakUsage);
    }

    void AllocationTracker::RegisterDeallocation(void* ptr) {
        if (!ptr) return;
        std::lock_guard<std::mutex> lock(GetMutex());

        auto& allocations = GetAllocationsMap();
        auto it = allocations.find(ptr);
        if (it != allocations.end()) {
            size_t size = it->second.size;
            std::string source = it->second.source;
            allocations.erase(it);

            auto& stats = GetStats();
            stats.totalAllocated -= size;
            stats.activeAllocations = allocations.size();

            LOG_TRACE("[Memory] Deallocated %zu from %s (Active: %zu bytes)",
                      size, source.c_str(), stats.totalAllocated);
        } else {
            LOG_WARN("[Memory] Attempted to untrack unknown pointer: %p", ptr);
        }
    }

    MemoryStatistics AllocationTracker::GetStatistics() {
        std::lock_guard<std::mutex> lock(GetMutex());
        return GetStats();
    }

    void AllocationTracker::Reset() {
        std::lock_guard<std::mutex> lock(GetMutex());
        GetAllocationsMap().clear();
        GetStats() = MemoryStatistics();
    }

    void AllocationTracker::DumpLeakReport() {
        std::lock_guard<std::mutex> lock(GetMutex());
        auto& allocations = GetAllocationsMap();
        
        LOG_INFO("[Memory] Leak check: %zu active allocations at shutdown.", allocations.size());
        if (!allocations.empty()) {
            LOG_ERROR("!!! MEMORY LEAKS DETECTED !!!");
            size_t totalLeaked = 0;
            for (auto const& [ptr, info] : allocations) {
                LOG_ERROR("  Leak: address %p | size: %zu bytes | source: '%s'",
                          ptr, info.size, info.source.c_str());
                totalLeaked += info.size;
            }
            LOG_ERROR("Total Leaked Memory: %zu bytes", totalLeaked);
        } else {
            LOG_INFO("[Memory] Leak check: 0 leaked allocations. Everything gets cleaned up cleanly!");
        }
    }

    size_t AllocationTracker::GetActiveAllocationsCount() {
        std::lock_guard<std::mutex> lock(GetMutex());
        return GetAllocationsMap().size();
    }

} // namespace eng::memory
