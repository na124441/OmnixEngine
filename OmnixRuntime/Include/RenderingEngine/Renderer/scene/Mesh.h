#pragma once
#include <vulkan/vulkan.h>
#include "Core/Vulkan/VkUtils.h"
#include "Core/types/Vertex.h"
#include "Core/Engine/ResourceTracker.h"
#include "Core/Engine/Log.h"
#include "Core/Engine/VmaUsage.h"
#include "Rendering/Geometry/GeometryHandle.h"
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

namespace eng::renderer {

struct EngineResources;

struct MeshBounds
{
    glm::vec3 localCenter = glm::vec3(0.0f);
    float localRadius = 1.0f;
    glm::vec3 localMin = glm::vec3(0.0f);
    glm::vec3 localMax = glm::vec3(0.0f);
};

class Mesh {
public:
    Mesh() = default;
    ~Mesh() { destroy(); }

    template<typename VertexT, typename IndexT>
    bool init(const VertexT* vertices, size_t vertexCount,
             const IndexT* indices,   size_t indexCount,
             EngineResources& eng);

    void bind(VkCommandBuffer cmd) const;
    void destroy();
    void cleanup(VkDevice device); // kept for compatibility if needed

    // Data members (now public for ease of access in current architecture)
    GeometryHandle handle;
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VmaAllocation vertexAlloc = VK_NULL_HANDLE;
    VkDeviceSize vertexSize = 0;

    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VmaAllocation indexAlloc = VK_NULL_HANDLE;
    VkDeviceSize indexSize = 0;

    uint32_t indexCount = 0;
    uint32_t firstIndex = 0;
    int32_t vertexOffset = 0;
    uint32_t materialSlotOffset = 0;

    // Helper for move semantics
    void moveFrom(Mesh&& rhs);

    uint32_t getIndexCount() const { return indexCount; }

    glm::vec3 minBounds = glm::vec3(0.0f);
    glm::vec3 maxBounds = glm::vec3(0.0f);
    MeshBounds bounds;

    bool isVirtualGeometry = false;
    uint32_t rvgAssetIndex = 0xFFFFFFFF;

    bool hasNormals = false;
    bool hasUVs = false;
    bool hasTangents = false;
    bool normalsGenerated = false;
    bool tangentsGenerated = false;

    VkDevice      device = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;
};

} // namespace eng::renderer
