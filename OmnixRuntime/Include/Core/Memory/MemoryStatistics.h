#pragma once
#include <cstddef>
#include <cstdint>

namespace eng::memory {

    enum class MemoryCategory : uint8_t {
        Unknown,
        Graphics,
        Physics,
        Audio,
        ECS,
        Scene,
        Editor,
        Count
    };

    inline const char* GetMemoryCategoryName(MemoryCategory cat) noexcept {
        switch (cat) {
            case MemoryCategory::Unknown:  return "Unknown";
            case MemoryCategory::Graphics: return "Graphics";
            case MemoryCategory::Physics:  return "Physics";
            case MemoryCategory::Audio:    return "Audio";
            case MemoryCategory::ECS:      return "ECS";
            case MemoryCategory::Scene:    return "Scene";
            case MemoryCategory::Editor:   return "Editor";
            default:                       return "Invalid";
        }
    }

    struct CategoryStats {
        size_t currentBytes = 0;
        size_t peakBytes = 0;
        size_t totalAllocationsCount = 0;
    };

    struct MemoryStatistics {
        size_t totalAllocated = 0;       // Bytes currently allocated
        size_t activeAllocations = 0;   // Count of active allocations
        size_t peakUsage = 0;           // Peak memory usage (bytes)
        size_t totalAllocationsCount = 0; // Total count of allocations made over runtime
        CategoryStats categories[static_cast<size_t>(MemoryCategory::Count)]{};
    };

} // namespace eng::memory
