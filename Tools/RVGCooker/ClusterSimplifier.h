#pragma once
#include <vector>
#include <cstdint>
#include "ClusterBuilder.h"
#include "ClusterGroupBuilder.h"

namespace eng::cooker {

class ClusterSimplifier {
public:
    ClusterSimplifier() = default;
    ~ClusterSimplifier() = default;

    bool Simplify(
        const ClusterGroup& group,
        const std::vector<CookedCluster>& allClusters,
        std::vector<eng::renderer::PbrVertex>& outVertices,
        std::vector<uint32_t>& outIndices,
        float& outGeometricError,
        bool lockBoundaries = true
    );
};

} // namespace eng::cooker
