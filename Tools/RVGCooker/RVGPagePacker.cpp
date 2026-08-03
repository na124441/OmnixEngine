#include "RVGPagePacker.h"
#include <iostream>
#include <cstring>

namespace eng::cooker {

bool RVGPagePacker::PackPages(
    const std::vector<CookedCluster>& clusters,
    uint32_t pageSize
) {
    m_Pages.clear();
    m_PackedInfos.clear();

    if (clusters.empty()) return true;

    PackedPage currentPage;
    currentPage.data.resize(pageSize, 0);
    currentPage.size = 0;

    for (uint32_t i = 0; i < clusters.size(); ++i) {
        const auto& c = clusters[i];
        
        uint32_t verticesBytes = static_cast<uint32_t>(c.vertices.size() * sizeof(eng::renderer::PbrVertex));
        uint32_t indicesBytes = static_cast<uint32_t>(c.indices.size() * sizeof(uint32_t));
        uint32_t clusterBytes = verticesBytes + indicesBytes;

        if (clusterBytes > pageSize) {
            std::cerr << "Error: Cluster " << i << " payload (" << clusterBytes 
                      << " bytes) exceeds virtual page size (" << pageSize << " bytes).\n";
            return false;
        }

        uint32_t alignedStart = (currentPage.size + 3) & ~3;

        if (alignedStart + clusterBytes > pageSize) {
            m_Pages.push_back(currentPage);

            currentPage.data.clear();
            currentPage.data.resize(pageSize, 0);
            currentPage.size = 0;
            alignedStart = 0;
        }

        uint32_t localVertOffset = alignedStart;
        std::memcpy(&currentPage.data[localVertOffset], c.vertices.data(), verticesBytes);

        uint32_t localIndexOffset = localVertOffset + verticesBytes;
        std::memcpy(&currentPage.data[localIndexOffset], c.indices.data(), indicesBytes);

        currentPage.size = localIndexOffset + indicesBytes;

        PackedClusterInfo info;
        info.clusterId = i;
        info.pageIndex = static_cast<uint32_t>(m_Pages.size());
        info.vertexOffset = localVertOffset;
        info.indexOffset = localIndexOffset;
        m_PackedInfos.push_back(info);
    }

    if (currentPage.size > 0) {
        m_Pages.push_back(currentPage);
    }

    std::cout << "Packed " << clusters.size() << " clusters into " << m_Pages.size() 
              << " pages of size " << pageSize << " bytes.\n";

    return true;
}

} // namespace eng::cooker
