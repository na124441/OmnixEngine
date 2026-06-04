#pragma once
#include "Core/Memory/MemoryStatistics.h"
#include <cstddef>
#include <string>
#include <unordered_map>
#include <mutex>

namespace eng::memory {

    struct AllocationInfo {
        size_t size;
        std::string source;
    };

    class AllocationTracker {
    public:
        static void RegisterAllocation(void* ptr, size_t size, const std::string& source);
        static void RegisterDeallocation(void* ptr);

        static MemoryStatistics GetStatistics();
        static void Reset();

        static void DumpLeakReport();
        static size_t GetActiveAllocationsCount();

    private:
        static std::mutex& GetMutex();
        static std::unordered_map<void*, AllocationInfo>& GetAllocationsMap();
        static MemoryStatistics& GetStats();
    };

} // namespace eng::memory
