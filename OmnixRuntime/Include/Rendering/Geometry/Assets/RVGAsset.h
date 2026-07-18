#pragma once
#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "Rendering/Geometry/Assets/RVGFileFormat.h"
#include "Core/types/Vertex.h"

namespace eng::renderer {

struct RVGNode {
    uint32_t clusterId = 0xFFFFFFFF;
    float geometricError = 0.0f;
    uint32_t parentNodeId = 0xFFFFFFFF;
    uint32_t childCount = 0;
    uint32_t childNodeIds[4] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
};

struct RVGCluster {
    glm::vec4 boundsSphere; // xyz = center, w = radius
    glm::vec4 coneAxisCutoff; // xyz = axis, w = cutoff
    uint32_t pageIndex = 0;
    uint32_t vertexOffset = 0; // Local byte offset inside the page
    uint32_t indexOffset = 0;  // Local byte offset inside the page
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
};

class RVGAsset {
public:
    RVGAsset() = default;
    ~RVGAsset() = default;

    bool LoadFromFile(const std::string& filepath);

    const RVGHeader& GetHeader() const { return m_Header; }
    uint32_t GetClusterCount() const { return m_ClusterCount; }
    uint32_t GetNodeCount() const { return m_NodeCount; }
    uint32_t GetPageCount() const { return m_PageCount; }
    const glm::vec3& GetBoundsMin() const { return m_BoundsMin; }
    const glm::vec3& GetBoundsMax() const { return m_BoundsMax; }

    const std::vector<RVGNode>& GetNodes() const { return m_Nodes; }
    const std::vector<RVGCluster>& GetClusters() const { return m_Clusters; }
    const std::vector<uint32_t>& GetPageSizes() const { return m_PageSizes; }
    const std::vector<std::vector<uint8_t>>& GetPagesData() const { return m_PagesData; }

    // Fallback mesh for hybrid rendering
    const std::vector<PbrVertex>& GetFallbackVertices() const { return m_FallbackVertices; }
    const std::vector<uint32_t>& GetFallbackIndices() const { return m_FallbackIndices; }

private:
    RVGHeader m_Header;
    uint32_t m_ClusterCount = 0;
    uint32_t m_NodeCount = 0;
    uint32_t m_PageCount = 0;
    glm::vec3 m_BoundsMin = glm::vec3(0.0f);
    glm::vec3 m_BoundsMax = glm::vec3(0.0f);

    std::vector<RVGNode> m_Nodes;
    std::vector<RVGCluster> m_Clusters;
    std::vector<uint32_t> m_PageSizes;
    std::vector<std::vector<uint8_t>> m_PagesData;

    std::vector<PbrVertex> m_FallbackVertices;
    std::vector<uint32_t> m_FallbackIndices;
};

} // namespace eng::renderer
