#include "Core/Memory/AllocationTracker.h"
#include "Core/Logging/Logger.h"
#include "Core/Diagnostics/Assert.h"
#include <map>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace eng::memory {

    thread_local bool s_InsideGuard = false;
    bool s_AllocationHookEnabled = true;

    static uint64_t CaptureCallstackHash() {
#ifdef _WIN32
        void* stack[16];
        unsigned short frames = CaptureStackBackTrace(3, 16, stack, nullptr);
        uint64_t hash = 14695981039346656037ULL;
        for (unsigned short i = 0; i < frames; ++i) {
            hash ^= reinterpret_cast<uint64_t>(stack[i]);
            hash *= 1099511628211ULL;
        }
        return hash;
#else
        return 0;
#endif
    }

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

    MemoryBudget* AllocationTracker::GetBudgets() {
        static MemoryBudget s_Budgets[static_cast<size_t>(MemoryCategory::Count)];
        return s_Budgets;
    }

    void AllocationTracker::RegisterAllocation(void* ptr, size_t size, const std::string& source) {
        RegisterAllocation(ptr, size, MemoryCategory::Unknown, source.c_str(), 0);
    }

    void AllocationTracker::RegisterAllocation(void* ptr, size_t size, MemoryCategory category, const char* file, int line) {
        if (!ptr) return;

        // Prevent recursion from within map and string allocation calls
        bool expected = false;
        if (s_InsideGuard) return;
        s_InsideGuard = true;

        {
            std::lock_guard<std::mutex> lock(GetMutex());
            auto& allocations = GetAllocationsMap();

            AllocationInfo info{};
            info.size = size;
            info.category = category;
            info.file = file ? file : "";
            info.line = line;
            info.callstackHash = CaptureCallstackHash();
            info.source = GetMemoryCategoryName(category);

            allocations[ptr] = info;

            auto& stats = GetStats();
            stats.totalAllocated += size;
            stats.activeAllocations = allocations.size();
            stats.totalAllocationsCount++;
            if (stats.totalAllocated > stats.peakUsage) {
                stats.peakUsage = stats.totalAllocated;
            }

            size_t catIdx = static_cast<size_t>(category);
            auto& catStats = stats.categories[catIdx];
            catStats.currentBytes += size;
            catStats.totalAllocationsCount++;
            if (catStats.currentBytes > catStats.peakBytes) {
                catStats.peakBytes = catStats.currentBytes;
            }

            // Budget assertion check (T1.2.10)
            MemoryBudget budget = GetBudgets()[catIdx];
            if (budget.limitBytes > 0 && catStats.currentBytes > budget.limitBytes) {
                s_InsideGuard = false;
                OMNIX_FATAL_ASSERT(false, "Memory allocation failed: Budget limit exceeded for category!");
            }
        }

        s_InsideGuard = false;
    }

    void AllocationTracker::RegisterDeallocation(void* ptr) {
        if (!ptr) return;

        if (s_InsideGuard) return;
        s_InsideGuard = true;

        {
            std::lock_guard<std::mutex> lock(GetMutex());
            auto& allocations = GetAllocationsMap();
            auto it = allocations.find(ptr);
            if (it != allocations.end()) {
                size_t size = it->second.size;
                MemoryCategory category = it->second.category;

                allocations.erase(it);

                auto& stats = GetStats();
                stats.totalAllocated -= size;
                stats.activeAllocations = allocations.size();

                size_t catIdx = static_cast<size_t>(category);
                stats.categories[catIdx].currentBytes -= size;
            }
        }

        s_InsideGuard = false;
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
            
            // Group leaks by callsite: (file, line) (T1.2.8)
            struct CallsiteInfo {
                size_t totalBytes = 0;
                size_t allocCount = 0;
            };
            std::map<std::pair<std::string, int>, CallsiteInfo> groupedLeaks;
            size_t totalLeaked = 0;

            for (const auto& [ptr, info] : allocations) {
                std::pair<std::string, int> callsite = { info.file, info.line };
                groupedLeaks[callsite].totalBytes += info.size;
                groupedLeaks[callsite].allocCount++;
                totalLeaked += info.size;
            }

            for (const auto& [callsite, cinfo] : groupedLeaks) {
                LOG_ERROR("  Callsite: %s:%d | Leaked: %zu bytes across %zu allocations",
                          callsite.first.empty() ? "unknown" : callsite.first.c_str(),
                          callsite.second, cinfo.totalBytes, cinfo.allocCount);
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

    void AllocationTracker::SetCategoryBudget(MemoryCategory category, size_t limitBytes) {
        std::lock_guard<std::mutex> lock(GetMutex());
        GetBudgets()[static_cast<size_t>(category)].limitBytes = limitBytes;
    }

} // namespace eng::memory

// Global operator new/delete hooks wrapping the allocation tracker (T1.2.5)
void* operator new(std::size_t size) {
    if (eng::memory::s_AllocationHookEnabled && !eng::memory::s_InsideGuard) {
        eng::memory::s_InsideGuard = true;
        void* ptr = std::malloc(size);
        eng::memory::AllocationTracker::RegisterAllocation(ptr, size, eng::memory::MemoryCategory::Unknown, "GlobalNew", 0);
        eng::memory::s_InsideGuard = false;
        return ptr;
    }
    return std::malloc(size);
}

void operator delete(void* ptr) noexcept {
    if (eng::memory::s_AllocationHookEnabled && !eng::memory::s_InsideGuard) {
        eng::memory::s_InsideGuard = true;
        eng::memory::AllocationTracker::RegisterDeallocation(ptr);
        std::free(ptr);
        eng::memory::s_InsideGuard = false;
        return;
    }
    std::free(ptr);
}

void* operator new[](std::size_t size) {
    return ::operator new(size);
}

void operator delete[](void* ptr) noexcept {
    ::operator delete(ptr);
}
