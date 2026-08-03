#pragma once
#include "Core/Memory/MemoryStatistics.h"
#include <cstddef>
#include <string>
#include <unordered_map>
#include <mutex>

namespace eng::memory {

    // Global guard switches for operator new recursion prevention
    extern thread_local int s_InsideGuardCount;
    extern bool s_AllocationHookEnabled;

    struct AllocationInfo {
        size_t size = 0;
        std::string source;
        std::string file;
        int line = 0;
        uint64_t callstackHash = 0;
        MemoryCategory category = MemoryCategory::Unknown;
    };

    struct MemoryBudget {
        size_t limitBytes = 0;
    };

    /**
     * @class AllocationTracker
     * @brief Centralized tracking registry for all engine memory allocations, category statistics, budgeting, and leak detection (T1.2.6 - T1.2.8, T1.2.10).
     */
    class AllocationTracker {
    public:
        static void RegisterAllocation(void* ptr, size_t size, const std::string& source);
        static void RegisterAllocation(void* ptr, size_t size, MemoryCategory category, const char* file = "", int line = 0);
        static void RegisterDeallocation(void* ptr);

        static MemoryStatistics GetStatistics();
        static void Reset();

        static void DumpLeakReport();
        static size_t GetActiveAllocationsCount();

        /**
         * @brief Define a maximum memory limit for a category (T1.2.10).
         */
        static void SetCategoryBudget(MemoryCategory category, size_t limitBytes);

    private:
        static std::mutex& GetMutex();
        static std::unordered_map<void*, AllocationInfo>& GetAllocationsMap();
        static MemoryStatistics& GetStats();
        static MemoryBudget* GetBudgets();
    };

} // namespace eng::memory
