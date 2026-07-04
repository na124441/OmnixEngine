#include "GeometryArenaStats.h"
#include "Core/Engine/Log.h"
#include <sstream>

namespace eng::renderer {

void GeometryArenaStats::LogStats(const ArenaStats& stats) {
    std::stringstream ss;
    ss << "\n==================================================\n";
    ss << "            GEOMETRY ARENA DIAGNOSTICS             \n";
    ss << "==================================================\n";
    ss << " [Vertex Buffer]\n";
    ss << "  - Total Capacity: " << stats.totalVertexMemory << " bytes\n";
    ss << "  - Used Memory:    " << stats.usedVertexMemory << " bytes\n";
    ss << "  - Free Memory:    " << stats.freeVertexMemory << " bytes\n";
    ss << "  - Fragmentation:  " << stats.vertexFragmentation << " %\n";
    ss << "\n [Index Buffer]\n";
    ss << "  - Total Capacity: " << stats.totalIndexMemory << " bytes\n";
    ss << "  - Used Memory:    " << stats.usedIndexMemory << " bytes\n";
    ss << "  - Free Memory:    " << stats.freeIndexMemory << " bytes\n";
    ss << "  - Fragmentation:  " << stats.indexFragmentation << " %\n";
    ss << "\n [General]\n";
    ss << "  - Allocations:    " << stats.allocationCount << "\n";
    ss << "  - Upload Bandwidth: " << stats.uploadBytesPerFrame << " bytes/frame\n";
    ss << "  - Growth Events:  " << stats.growthCount << "\n";
    ss << "==================================================\n";

    LOG_INFO(ss.str());
}

} // namespace eng::renderer
