#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#include <type_traits>

namespace eng::renderer {

// 6.4 GPU Mesh Record
struct GPUMeshRecord
{
    uint32_t firstIndex = 0;
    int32_t vertexOffset = 0;
    uint32_t indexCount = 0;
    uint32_t vertexCount = 0;
    glm::vec4 localBoundsSphere = glm::vec4(0.0f); // xyz = center, w = radius
    uint32_t materialSlotOffset = 0;
    uint32_t submeshOffset = 0;
    uint32_t submeshCount = 0;
    uint32_t flags = 0;
};

// Confirm CPU/GPU alignment & size static assertions
static_assert(sizeof(GPUMeshRecord) == 48, "GPUMeshRecord size must be exactly 48 bytes.");
static_assert(offsetof(GPUMeshRecord, firstIndex) == 0, "firstIndex offset must be 0.");
static_assert(offsetof(GPUMeshRecord, vertexOffset) == 4, "vertexOffset offset must be 4.");
static_assert(offsetof(GPUMeshRecord, indexCount) == 8, "indexCount offset must be 8.");
static_assert(offsetof(GPUMeshRecord, vertexCount) == 12, "vertexCount offset must be 12.");
static_assert(offsetof(GPUMeshRecord, localBoundsSphere) == 16, "localBoundsSphere offset must be 16.");
static_assert(offsetof(GPUMeshRecord, materialSlotOffset) == 32, "materialSlotOffset offset must be 32.");
static_assert(offsetof(GPUMeshRecord, submeshOffset) == 36, "submeshOffset offset must be 36.");
static_assert(offsetof(GPUMeshRecord, submeshCount) == 40, "submeshCount offset must be 40.");
static_assert(offsetof(GPUMeshRecord, flags) == 44, "flags offset must be 44.");

// Keep GPUMeshDrawData as alias for compatibility in other passes/files
using GPUMeshDrawData = GPUMeshRecord;

} // namespace eng::renderer
