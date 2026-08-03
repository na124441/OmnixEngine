#pragma once
#include <string>
#include <vector>
#include "ClusterBuilder.h"
#include "HierarchyBuilder.h"
#include "RVGPagePacker.h"

namespace eng::cooker {

class RVGWriter {
public:
    RVGWriter() = default;
    ~RVGWriter() = default;

    bool Write(
        const std::string& outputPath,
        const std::vector<CookedCluster>& allClusters,
        const std::vector<HierarchyNode>& nodes,
        const std::vector<PackedPage>& pages,
        const std::vector<PackedClusterInfo>& packedInfos,
        const glm::vec3& boundsMin,
        const glm::vec3& boundsMax
    );
};

} // namespace eng::cooker
