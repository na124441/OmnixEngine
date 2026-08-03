#pragma once
#include <vector>
#include <cstdint>
#include "ClusterBuilder.h"

namespace eng::cooker {

struct ClusterGroup {
    std::vector<uint32_t> clusterIndices; // Indices of clusters in the input cluster list
};

class ClusterGroupBuilder {
public:
    ClusterGroupBuilder() = default;
    ~ClusterGroupBuilder() = default;

    bool GroupClusters(
        const std::vector<CookedCluster>& clusters,
        uint32_t maxGroupSize
    );

    const std::vector<ClusterGroup>& GetGroups() const { return m_Groups; }

private:
    std::vector<ClusterGroup> m_Groups;
};

} // namespace eng::cooker
