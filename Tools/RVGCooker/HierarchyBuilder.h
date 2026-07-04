#pragma once
#include <vector>
#include <cstdint>
#include "ClusterBuilder.h"

namespace eng::cooker {

struct HierarchyNode {
    uint32_t clusterId = 0;
    float geometricError = 0.0f;
    std::vector<uint32_t> childNodeIds;
    uint32_t parentNodeId = 0xFFFFFFFF;
};

class HierarchyBuilder {
public:
    HierarchyBuilder() = default;
    ~HierarchyBuilder() = default;

    bool BuildHierarchy(
        const std::vector<CookedCluster>& leafClusters,
        uint32_t vertexLimit,
        uint32_t triangleLimit
    );

    const std::vector<HierarchyNode>& GetNodes() const { return m_Nodes; }
    const std::vector<CookedCluster>& GetAllClusters() const { return m_AllClusters; }

private:
    std::vector<HierarchyNode> m_Nodes;
    std::vector<CookedCluster> m_AllClusters;
};

} // namespace eng::cooker
