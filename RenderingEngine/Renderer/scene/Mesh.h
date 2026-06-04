#pragma once
#include <vulkan/vulkan.h>
#include "Core/Vulkan/VkUtils.h"
#include "Core/types/Vertex.h"
#include "Core/Engine/ResourceTracker.h"
#include "Core/Engine/Log.h"
#include "Core/Engine/VmaUsage.h"
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

namespace eng::renderer {

struct EngineResources;

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
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VmaAllocation vertexAlloc = VK_NULL_HANDLE;
    VkDeviceSize vertexSize = 0;

    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VmaAllocation indexAlloc = VK_NULL_HANDLE;
    VkDeviceSize indexSize = 0;

    uint32_t indexCount = 0;

    // Helper for move semantics
    void moveFrom(Mesh&& rhs);

    uint32_t getIndexCount() const { return indexCount; }

    glm::vec3 minBounds = glm::vec3(0.0f);
    glm::vec3 maxBounds = glm::vec3(0.0f);
};

} // namespace eng::renderer
