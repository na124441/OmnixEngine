#include "RVGWriter.h"
#include "Rendering/Geometry/Assets/RVGFileFormat.h"
#include "Core/Engine/Log.h"
#include <iostream>
#include <fstream>
#include <vector>

namespace eng::cooker {

static uint32_t ComputeChecksum(const uint8_t* data, size_t size) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

bool RVGWriter::Write(
    const std::string& outputPath,
    const std::vector<CookedCluster>& allClusters,
    const std::vector<HierarchyNode>& nodes,
    const std::vector<PackedPage>& pages,
    const std::vector<PackedClusterInfo>& packedInfos,
    const glm::vec3& boundsMin,
    const glm::vec3& boundsMax
) {
    std::ofstream file(outputPath, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR(("RVGWriter: Failed to open output file: " + outputPath).c_str());
        return false;
    }

    // 1. Build Section buffers in memory to calculate offsets and sizes
    std::vector<uint8_t> metadataSection;
    {
        uint32_t clusterCount = static_cast<uint32_t>(allClusters.size());
        uint32_t nodeCount = static_cast<uint32_t>(nodes.size());
        uint32_t pageCount = static_cast<uint32_t>(pages.size());
        
        metadataSection.resize(sizeof(clusterCount) * 3 + sizeof(glm::vec3) * 2);
        uint8_t* ptr = metadataSection.data();
        
        std::memcpy(ptr, &clusterCount, sizeof(clusterCount)); ptr += sizeof(clusterCount);
        std::memcpy(ptr, &nodeCount, sizeof(nodeCount)); ptr += sizeof(nodeCount);
        std::memcpy(ptr, &pageCount, sizeof(pageCount)); ptr += sizeof(pageCount);
        std::memcpy(ptr, &boundsMin, sizeof(boundsMin)); ptr += sizeof(boundsMin);
        std::memcpy(ptr, &boundsMax, sizeof(boundsMax));
    }

    std::vector<uint8_t> hierarchySection;
    for (const auto& node : nodes) {
        uint32_t clusterId = node.clusterId;
        float error = node.geometricError;
        uint32_t parentId = node.parentNodeId;
        uint32_t childCount = static_cast<uint32_t>(node.childNodeIds.size());

        size_t startOffset = hierarchySection.size();
        hierarchySection.resize(startOffset + sizeof(uint32_t) * 3 + sizeof(float) + sizeof(uint32_t) * childCount);
        uint8_t* ptr = &hierarchySection[startOffset];

        std::memcpy(ptr, &clusterId, sizeof(clusterId)); ptr += sizeof(clusterId);
        std::memcpy(ptr, &error, sizeof(error)); ptr += sizeof(error);
        std::memcpy(ptr, &parentId, sizeof(parentId)); ptr += sizeof(parentId);
        std::memcpy(ptr, &childCount, sizeof(childCount)); ptr += sizeof(childCount);
        if (childCount > 0) {
            std::memcpy(ptr, node.childNodeIds.data(), sizeof(uint32_t) * childCount);
        }
    }

    std::vector<uint8_t> clusterSection;
    {
        // Map from clusterIndex to packed info details
        std::unordered_map<uint32_t, PackedClusterInfo> infoMap;
        for (const auto& info : packedInfos) {
            infoMap[info.clusterId] = info;
        }

        for (uint32_t i = 0; i < allClusters.size(); ++i) {
            const auto& c = allClusters[i];
            const auto& info = infoMap[i];

            glm::vec4 sphere = c.boundsSphere;
            glm::vec3 axis = c.coneAxis;
            float cutoff = c.coneCutoff;
            uint32_t pageIndex = info.pageIndex;
            uint32_t vertexOffset = info.vertexOffset;
            uint32_t indexOffset = info.indexOffset;
            uint32_t vertexCount = static_cast<uint32_t>(c.vertices.size());
            uint32_t indexCount = static_cast<uint32_t>(c.indices.size());

            size_t startOffset = clusterSection.size();
            clusterSection.resize(startOffset + sizeof(glm::vec4) + sizeof(glm::vec3) + sizeof(float) + sizeof(uint32_t) * 5);
            uint8_t* ptr = &clusterSection[startOffset];

            std::memcpy(ptr, &sphere, sizeof(sphere)); ptr += sizeof(sphere);
            std::memcpy(ptr, &axis, sizeof(axis)); ptr += sizeof(axis);
            std::memcpy(ptr, &cutoff, sizeof(cutoff)); ptr += sizeof(cutoff);
            std::memcpy(ptr, &pageIndex, sizeof(pageIndex)); ptr += sizeof(pageIndex);
            std::memcpy(ptr, &vertexOffset, sizeof(vertexOffset)); ptr += sizeof(vertexOffset);
            std::memcpy(ptr, &indexOffset, sizeof(indexOffset)); ptr += sizeof(indexOffset);
            std::memcpy(ptr, &vertexCount, sizeof(vertexCount)); ptr += sizeof(vertexCount);
            std::memcpy(ptr, &indexCount, sizeof(indexCount));
        }
    }

    std::vector<uint8_t> pageDescSection;
    for (const auto& page : pages) {
        uint32_t pageSize = static_cast<uint32_t>(page.data.size());
        size_t startOffset = pageDescSection.size();
        pageDescSection.resize(startOffset + sizeof(uint32_t));
        std::memcpy(&pageDescSection[startOffset], &pageSize, sizeof(pageSize));
    }

    std::vector<uint8_t> pagesSection;
    for (const auto& page : pages) {
        pagesSection.insert(pagesSection.end(), page.data.begin(), page.data.end());
    }

    std::vector<uint8_t> fallbackSection;
    {
        // Use the root cluster geometry as the fallback mesh representation
        const auto& root = allClusters.back();
        uint32_t vertCount = static_cast<uint32_t>(root.vertices.size());
        uint32_t indexCount = static_cast<uint32_t>(root.indices.size());

        fallbackSection.resize(sizeof(uint32_t) * 2 + vertCount * sizeof(eng::renderer::PbrVertex) + indexCount * sizeof(uint32_t));
        uint8_t* ptr = fallbackSection.data();

        std::memcpy(ptr, &vertCount, sizeof(vertCount)); ptr += sizeof(vertCount);
        std::memcpy(ptr, &indexCount, sizeof(indexCount)); ptr += sizeof(indexCount);
        if (vertCount > 0) {
            std::memcpy(ptr, root.vertices.data(), vertCount * sizeof(eng::renderer::PbrVertex));
            ptr += vertCount * sizeof(eng::renderer::PbrVertex);
        }
        if (indexCount > 0) {
            std::memcpy(ptr, root.indices.data(), indexCount * sizeof(uint32_t));
        }
    }

    // 2. Prepare Header and Directory Entries
    eng::renderer::RVGHeader header;
    header.sectionCount = 6;

    std::vector<eng::renderer::RVGSectionEntry> entries(header.sectionCount);
    
    // We will place sections right after header and directories table
    uint64_t currentOffset = sizeof(eng::renderer::RVGHeader) + sizeof(eng::renderer::RVGSectionEntry) * header.sectionCount;

    auto fillEntry = [&](uint32_t index, eng::renderer::RVGSectionType type, const std::vector<uint8_t>& sectionData) {
        entries[index].type = static_cast<uint32_t>(type);
        entries[index].offset = currentOffset;
        entries[index].size = sectionData.size();
        entries[index].checksum = ComputeChecksum(sectionData.data(), sectionData.size());
        currentOffset += sectionData.size();
    };

    fillEntry(0, eng::renderer::RVGSectionType::Metadata, metadataSection);
    fillEntry(1, eng::renderer::RVGSectionType::Hierarchy, hierarchySection);
    fillEntry(2, eng::renderer::RVGSectionType::Clusters, clusterSection);
    fillEntry(3, eng::renderer::RVGSectionType::Pages, pageDescSection); // Page sizes
    fillEntry(4, eng::renderer::RVGSectionType::Pages, pagesSection);     // Raw pages data
    fillEntry(5, eng::renderer::RVGSectionType::FallbackMesh, fallbackSection);

    // 3. Write binary elements to file
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(reinterpret_cast<const char*>(entries.data()), sizeof(eng::renderer::RVGSectionEntry) * header.sectionCount);

    file.write(reinterpret_cast<const char*>(metadataSection.data()), metadataSection.size());
    file.write(reinterpret_cast<const char*>(hierarchySection.data()), hierarchySection.size());
    file.write(reinterpret_cast<const char*>(clusterSection.data()), clusterSection.size());
    file.write(reinterpret_cast<const char*>(pageDescSection.data()), pageDescSection.size());
    file.write(reinterpret_cast<const char*>(pagesSection.data()), pagesSection.size());
    file.write(reinterpret_cast<const char*>(fallbackSection.data()), fallbackSection.size());

    LOG_INFO(("RVGWriter: Serialized cooked output successfully. File size: " + std::to_string(currentOffset) + " bytes.").c_str());

    return true;
}

} // namespace eng::cooker
