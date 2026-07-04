#pragma once
#include <vector>
#include <cstdint>
#include "ClusterBuilder.h"

namespace eng::cooker {

struct PackedClusterInfo {
    uint32_t clusterId = 0;
    uint32_t pageIndex = 0;
    uint32_t vertexOffset = 0; // Local offset in vertices list inside the page
    uint32_t indexOffset = 0;  // Local offset in indices list inside the page
};

struct PackedPage {
    std::vector<uint8_t> data;
    uint32_t size = 0;
};

class RVGPagePacker {
public:
    RVGPagePacker() = default;
    ~RVGPagePacker() = default;

    bool PackPages(
        const std::vector<CookedCluster>& clusters,
        uint32_t pageSize
    );

    const std::vector<PackedPage>& GetPages() const { return m_Pages; }
    const std::vector<PackedClusterInfo>& GetPackedClusterInfos() const { return m_PackedInfos; }

private:
    std::vector<PackedPage> m_Pages;
    std::vector<PackedClusterInfo> m_PackedInfos;
};

} // namespace eng::cooker
