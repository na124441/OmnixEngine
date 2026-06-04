#pragma once
#include <cstddef>

namespace eng::memory {

    struct MemoryStatistics {
        size_t totalAllocated = 0;       // Bytes currently allocated
        size_t activeAllocations = 0;   // Count of active allocations
        size_t peakUsage = 0;           // Peak memory usage (bytes)
        size_t totalAllocationsCount = 0; // Total count of allocations made over runtime
    };

} // namespace eng::memory
