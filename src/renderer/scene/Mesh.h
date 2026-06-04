#pragma once
#include <vulkan/vulkan.h>
#include "engine/ResourceTracker.h"
#include "engine/Log.h"
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
};

struct EngineResources;

/// Simple mesh that owns GPU buffers (vertex + index) and the index count.
class Mesh {
public:
    Mesh() = default;
    ~Mesh() { destroy(); }

    // Non‑copyable, movable
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& rhs) noexcept { moveFrom(std::move(rhs)); }
    Mesh& operator=(Mesh&& rhs) noexcept { if (this != &rhs){ destroy(); moveFrom(std::move(rhs)); } return *this; }

    /// Build a mesh from raw vertex / index arrays.
    template<typename VertexT, typename IndexT>
    bool init(const VertexT* vertices, size_t vertexCount,
              const IndexT* indices,   size_t indexCount,
              const EngineResources& eng);

    /// Bind the mesh for a draw call (vertex + index buffers)
    void bind(VkCommandBuffer cmd) const;

    uint32_t getIndexCount() const { return indexCount; }

    /// Explicit destroy (called automatically from dtor)
    void destroy();

private:
    void moveFrom(Mesh&& rhs);

    VkBuffer       vertexBuffer   = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory   = VK_NULL_HANDLE;
    VkDeviceSize   vertexSize    = 0;

    VkBuffer       indexBuffer    = VK_NULL_HANDLE;
    VkDeviceMemory indexMemory    = VK_NULL_HANDLE;
    VkDeviceSize   indexSize     = 0;

    uint32_t       indexCount     = 0;
};
