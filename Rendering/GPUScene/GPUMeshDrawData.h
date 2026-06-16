#pragma once
#include <cstdint>

namespace eng::renderer {

struct GPUMeshDrawData
{
    uint32_t indexCount;
    uint32_t firstIndex;
    int32_t vertexOffset;
    uint32_t materialSlotOffset;
};

} // namespace eng::renderer
