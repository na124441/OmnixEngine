#include "Core/pch.h"
#include "RVGAsset.h"
#include "Core/Engine/Log.h"
#include <fstream>
#include <iostream>
#include <cstring>

namespace eng::renderer {

static uint32_t ComputeChecksum(const uint8_t* data, size_t size) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

bool RVGAsset::LoadFromFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG_ERROR(("RVGAsset: Failed to open file: " + filepath).c_str());
        return false;
    }

    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    if (fileSize < sizeof(RVGHeader)) {
        LOG_ERROR("RVGAsset: File is too small to contain header.");
        return false;
    }

    file.read(reinterpret_cast<char*>(&m_Header), sizeof(RVGHeader));

    if (std::memcmp(m_Header.magic, "OMNIXRVG", 8) != 0) {
        LOG_ERROR("RVGAsset: Invalid magic number.");
        return false;
    }

    if (m_Header.version != 1) {
        LOG_ERROR("RVGAsset: Unsupported version.");
        return false;
    }

    if (m_Header.sectionCount == 0) {
        LOG_ERROR("RVGAsset: Section count is zero.");
        return false;
    }

    size_t entriesSize = sizeof(RVGSectionEntry) * m_Header.sectionCount;
    if (fileSize < sizeof(RVGHeader) + entriesSize) {
        LOG_ERROR("RVGAsset: File is too small to contain section directory.");
        return false;
    }

    std::vector<RVGSectionEntry> entries(m_Header.sectionCount);
    file.read(reinterpret_cast<char*>(entries.data()), entriesSize);

    // Read section payloads
    std::vector<uint8_t> metadataBuf;
    std::vector<uint8_t> hierarchyBuf;
    std::vector<uint8_t> clusterBuf;
    std::vector<uint8_t> pageDescBuf;
    std::vector<uint8_t> pagesDataBuf;
    std::vector<uint8_t> fallbackBuf;

    for (const auto& entry : entries) {
        if (entry.offset + entry.size > fileSize) {
            LOG_ERROR("RVGAsset: Section bounds exceed file size.");
            return false;
        }

        std::vector<uint8_t> payload(entry.size);
        file.seekg(entry.offset, std::ios::beg);
        file.read(reinterpret_cast<char*>(payload.data()), entry.size);

        if (ComputeChecksum(payload.data(), payload.size()) != entry.checksum) {
            LOG_ERROR("RVGAsset: Section checksum validation failed.");
            return false;
        }

        RVGSectionType sType = static_cast<RVGSectionType>(entry.type);
        switch (sType) {
            case RVGSectionType::Metadata:
                metadataBuf = std::move(payload);
                break;
            case RVGSectionType::Hierarchy:
                hierarchyBuf = std::move(payload);
                break;
            case RVGSectionType::Clusters:
                clusterBuf = std::move(payload);
                break;
            case RVGSectionType::Pages:
                // Since there are two sections of type Pages (one for sizes, one for pages data),
                // we distinguish them by order or size.
                // Looking at RVGWriter: fillEntry(3, Pages, pageDescSection) and fillEntry(4, Pages, pagesSection)
                if (pageDescBuf.empty()) {
                    pageDescBuf = std::move(payload);
                } else {
                    pagesDataBuf = std::move(payload);
                }
                break;
            case RVGSectionType::FallbackMesh:
                fallbackBuf = std::move(payload);
                break;
            default:
                break;
        }
    }

    // 1. Parse Metadata
    if (metadataBuf.size() < sizeof(uint32_t) * 3 + sizeof(glm::vec3) * 2) {
        LOG_ERROR("RVGAsset: Invalid metadata section size.");
        return false;
    }
    const uint8_t* mPtr = metadataBuf.data();
    std::memcpy(&m_ClusterCount, mPtr, sizeof(m_ClusterCount)); mPtr += sizeof(m_ClusterCount);
    std::memcpy(&m_NodeCount, mPtr, sizeof(m_NodeCount)); mPtr += sizeof(m_NodeCount);
    std::memcpy(&m_PageCount, mPtr, sizeof(m_PageCount)); mPtr += sizeof(m_PageCount);
    std::memcpy(&m_BoundsMin, mPtr, sizeof(m_BoundsMin)); mPtr += sizeof(m_BoundsMin);
    std::memcpy(&m_BoundsMax, mPtr, sizeof(m_BoundsMax));

    // 2. Parse Hierarchy nodes
    m_Nodes.reserve(m_NodeCount);
    const uint8_t* hPtr = hierarchyBuf.data();
    const uint8_t* hEnd = hPtr + hierarchyBuf.size();
    for (uint32_t i = 0; i < m_NodeCount; ++i) {
        if (hPtr >= hEnd) break;
        RVGNode node{};
        std::memcpy(&node.clusterId, hPtr, sizeof(uint32_t)); hPtr += sizeof(uint32_t);
        std::memcpy(&node.geometricError, hPtr, sizeof(float)); hPtr += sizeof(float);
        std::memcpy(&node.parentNodeId, hPtr, sizeof(uint32_t)); hPtr += sizeof(uint32_t);
        uint32_t childCount = 0;
        std::memcpy(&childCount, hPtr, sizeof(uint32_t)); hPtr += sizeof(uint32_t);
        node.childCount = childCount;
        if (childCount > 0) {
            uint32_t countToCopy = childCount > 4 ? 4 : childCount;
            std::memcpy(node.childNodeIds, hPtr, sizeof(uint32_t) * countToCopy);
            hPtr += sizeof(uint32_t) * childCount;
        }
        m_Nodes.push_back(node);
    }

    // 3. Parse Clusters descriptors
    if (clusterBuf.size() != m_ClusterCount * (sizeof(glm::vec4) + sizeof(glm::vec3) + sizeof(float) + sizeof(uint32_t) * 5)) {
        LOG_ERROR("RVGAsset: Invalid cluster section size.");
        return false;
    }
    m_Clusters.resize(m_ClusterCount);
    const uint8_t* cPtr = clusterBuf.data();
    for (uint32_t i = 0; i < m_ClusterCount; ++i) {
        auto& c = m_Clusters[i];
        std::memcpy(&c.boundsSphere, cPtr, sizeof(glm::vec4)); cPtr += sizeof(glm::vec4);
        glm::vec3 axis;
        std::memcpy(&axis, cPtr, sizeof(glm::vec3)); cPtr += sizeof(glm::vec3);
        float cutoff;
        std::memcpy(&cutoff, cPtr, sizeof(float)); cPtr += sizeof(float);
        c.coneAxisCutoff = glm::vec4(axis, cutoff);

        std::memcpy(&c.pageIndex, cPtr, sizeof(uint32_t)); cPtr += sizeof(uint32_t);
        std::memcpy(&c.vertexOffset, cPtr, sizeof(uint32_t)); cPtr += sizeof(uint32_t);
        std::memcpy(&c.indexOffset, cPtr, sizeof(uint32_t)); cPtr += sizeof(uint32_t);
        std::memcpy(&c.vertexCount, cPtr, sizeof(uint32_t)); cPtr += sizeof(uint32_t);
        std::memcpy(&c.indexCount, cPtr, sizeof(uint32_t)); cPtr += sizeof(uint32_t);
    }

    // 4. Parse Pages sizes
    if (pageDescBuf.size() != m_PageCount * sizeof(uint32_t)) {
        LOG_ERROR("RVGAsset: Invalid page description section size.");
        return false;
    }
    m_PageSizes.resize(m_PageCount);
    std::memcpy(m_PageSizes.data(), pageDescBuf.data(), pageDescBuf.size());

    // 5. Parse Pages Data
    m_PagesData.resize(m_PageCount);
    const uint8_t* pDataPtr = pagesDataBuf.data();
    for (uint32_t i = 0; i < m_PageCount; ++i) {
        uint32_t pSize = m_PageSizes[i];
        m_PagesData[i].resize(pSize);
        std::memcpy(m_PagesData[i].data(), pDataPtr, pSize);
        pDataPtr += pSize;
    }

    // 6. Parse Fallback Mesh
    if (!fallbackBuf.empty()) {
        const uint8_t* fPtr = fallbackBuf.data();
        uint32_t vertCount = 0;
        uint32_t indexCount = 0;
        std::memcpy(&vertCount, fPtr, sizeof(uint32_t)); fPtr += sizeof(uint32_t);
        std::memcpy(&indexCount, fPtr, sizeof(uint32_t)); fPtr += sizeof(uint32_t);

        m_FallbackVertices.resize(vertCount);
        if (vertCount > 0) {
            std::memcpy(m_FallbackVertices.data(), fPtr, vertCount * sizeof(PbrVertex));
            fPtr += vertCount * sizeof(PbrVertex);
        }

        m_FallbackIndices.resize(indexCount);
        if (indexCount > 0) {
            std::memcpy(m_FallbackIndices.data(), fPtr, indexCount * sizeof(uint32_t));
        }
    }

    LOG_INFO(("RVGAsset: Loaded asset successfully from: " + filepath).c_str());
    return true;
}

} // namespace eng::renderer
