#pragma once
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>
#include "RenderingEngine/Core/types/Vertex.h"

namespace eng::cooker {

struct CookedCluster {
    std::vector<eng::renderer::PbrVertex> vertices;
    std::vector<uint32_t> indices;
    
    glm::vec3 boundsMin = glm::vec3(0.0f);
    glm::vec3 boundsMax = glm::vec3(0.0f);
    glm::vec4 boundsSphere = glm::vec4(0.0f); // xyz = center, w = radius
    glm::vec3 coneAxis = glm::vec3(0.0f);
    float coneCutoff = 0.0f; // semi-angle cosine (cutoff)
    
    std::vector<uint32_t> sourceTriangleIndices; // Maps local triangle index to raw index in source mesh
    uint32_t materialIndex = 0;
};

class ClusterBuilder {
public:
    ClusterBuilder() = default;
    ~ClusterBuilder() = default;

    bool BuildClusters(
        const std::vector<eng::renderer::PbrVertex>& vertices,
        const std::vector<uint32_t>& indices,
        uint32_t vertexLimit,
        uint32_t triangleLimit
    );

    const std::vector<CookedCluster>& GetClusters() const { return m_Clusters; }

private:
    void computeClusterBounds(CookedCluster& cluster);
    void computeNormalCone(CookedCluster& cluster);

    std::vector<CookedCluster> m_Clusters;
};

} // namespace eng::cooker
