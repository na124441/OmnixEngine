#pragma once
#include <cstdint>

namespace eng::renderer {

struct ArenaStats {
    uint64_t totalVertexMemory = 0;
    uint64_t totalIndexMemory = 0;
    uint64_t usedVertexMemory = 0;
    uint64_t usedIndexMemory = 0;
    uint64_t freeVertexMemory = 0;
    uint64_t freeIndexMemory = 0;
    double vertexFragmentation = 0.0;
    double indexFragmentation = 0.0;
    uint32_t allocationCount = 0;
    uint64_t uploadBytesPerFrame = 0;
    uint32_t growthCount = 0;
};

class GeometryArenaStats {
public:
    static void LogStats(const ArenaStats& stats);
};

} // namespace eng::renderer
