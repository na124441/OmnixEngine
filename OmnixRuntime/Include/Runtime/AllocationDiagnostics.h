#pragma once
#include <cstddef>

namespace eng::runtime {
    void TrackAllocation(const char* name, size_t sizeBytes);
    void TrackDeallocation(const char* name, size_t sizeBytes);
    void ReportMemoryLeaks();
}
