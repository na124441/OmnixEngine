#include "Core/pch.h"
#include "Rendering/Geometry/Assets/RVGRegistry.h"
#include "Core/Engine/EngineResources.h"
#include "Core/Engine/Log.h"
#include "Core/Engine/ResourceTracker.h"
#include "Core/Vulkan/VkUtils.h"
#include <cstring>
#include <algorithm>
#include "Rendering/Geometry/Streaming/RVGPageStreamingManager.h"

namespace eng::renderer {

void RVGRegistry::Initialize(EngineResources& resources) {
    m_Device = resources.device;
    m_Allocator = resources.allocator;
    m_Resources = &resources;
    LOG_INFO("RVGRegistry: Initialized.");
}

void RVGRegistry::Shutdown() {
    if (m_Allocator != VK_NULL_HANDLE) {
        if (m_AssetTableBuffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(m_Allocator, m_AssetTableBuffer, m_AssetTableAlloc);
            m_AssetTableBuffer = VK_NULL_HANDLE;
            m_AssetTableAlloc = VK_NULL_HANDLE;
            ::eng::ResourceTracker::decBuffer();
        }
        if (m_NodesBuffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(m_Allocator, m_NodesBuffer, m_NodesAlloc);
            m_NodesBuffer = VK_NULL_HANDLE;
            m_NodesAlloc = VK_NULL_HANDLE;
            ::eng::ResourceTracker::decBuffer();
        }
        if (m_ClustersBuffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(m_Allocator, m_ClustersBuffer, m_ClustersAlloc);
            m_ClustersBuffer = VK_NULL_HANDLE;
            m_ClustersAlloc = VK_NULL_HANDLE;
            ::eng::ResourceTracker::decBuffer();
        }
        if (m_PageDescBuffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(m_Allocator, m_PageDescBuffer, m_PageDescAlloc);
            m_PageDescBuffer = VK_NULL_HANDLE;
            m_PageDescAlloc = VK_NULL_HANDLE;
            ::eng::ResourceTracker::decBuffer();
        }
        if (m_ResidentGeometryBuffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(m_Allocator, m_ResidentGeometryBuffer, m_ResidentGeometryAlloc);
            m_ResidentGeometryBuffer = VK_NULL_HANDLE;
            m_ResidentGeometryAlloc = VK_NULL_HANDLE;
            ::eng::ResourceTracker::decBuffer();
        }
    }
    m_Assets.clear();
    m_AssetsMap.clear();
    m_AssetTableCapacity = 0;
    m_NodesCapacity = 0;
    m_ClustersCapacity = 0;
    m_PageDescCapacity = 0;
    m_ResidentGeometryCapacity = 0;
    LOG_INFO("RVGRegistry: Shutdown complete.");
}

uint32_t RVGRegistry::LoadAsset(EngineResources& resources, const std::string& filepath) {
    auto it = m_AssetsMap.find(filepath);
    if (it != m_AssetsMap.end()) {
        return it->second;
    }

    auto asset = std::make_unique<RVGAsset>();
    if (!asset->LoadFromFile(filepath)) {
        LOG_ERROR(("RVGRegistry: Failed to load asset: " + filepath).c_str());
        return UINT32_MAX;
    }

    uint32_t assetIndex = static_cast<uint32_t>(m_Assets.size());
    m_AssetsMap[filepath] = assetIndex;
    m_Assets.push_back(std::move(asset));

    // Rebuild GPU buffers to accommodate the new asset
    rebuildBuffers(resources);

    return assetIndex;
}

void RVGRegistry::rebuildBuffers(EngineResources& resources) {
    // 1. Calculate required capacities
    size_t requiredAssetTableSize = m_Assets.size() * sizeof(GPURVGAsset);
    size_t requiredNodesCount = 0;
    size_t requiredClustersCount = 0;
    size_t requiredPageDescCount = 0;
    size_t requiredGeometryBytes = 0;

    for (const auto& asset : m_Assets) {
        requiredNodesCount += asset->GetNodeCount();
        requiredClustersCount += asset->GetClusterCount();
        requiredPageDescCount += asset->GetPageCount();
        for (const auto& page : asset->GetPagesData()) {
            requiredGeometryBytes += page.size();
        }
    }

    size_t requiredNodesSize = requiredNodesCount * sizeof(RVGNode);
    size_t requiredClustersSize = requiredClustersCount * sizeof(RVGCluster);
    size_t requiredPageDescSize = requiredPageDescCount * sizeof(uint32_t);

    // Dynamic growth strategy (ensure capacity is at least required and aligned)
    bool recreateAssetTable = requiredAssetTableSize > m_AssetTableCapacity;
    bool recreateNodes = requiredNodesSize > m_NodesCapacity;
    bool recreateClusters = requiredClustersSize > m_ClustersCapacity;
    bool recreatePageDesc = requiredPageDescSize > m_PageDescCapacity;
    bool recreateGeometry = requiredGeometryBytes > m_ResidentGeometryCapacity;

    auto createOrGrowBuffer = [&](VkBuffer& buffer, VmaAllocation& alloc, size_t& currentCapacity, size_t requiredSize, VkBufferUsageFlags usage) {
        if (buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(m_Allocator, buffer, alloc);
            ::eng::ResourceTracker::decBuffer();
        }
        currentCapacity = std::max(requiredSize * 2, static_cast<size_t>(65536)); // double capacity or minimum 64KB
        
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = currentCapacity;
        bufInfo.usage = usage;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU; // CPU writable directly

        LOG_INFO("[RVGRegistry] Calling vmaCreateBuffer: allocator=%p, size=%llu", m_Allocator, (unsigned long long)currentCapacity);
        VK_CHECK(vmaCreateBuffer(m_Allocator, &bufInfo, &allocInfo, &buffer, &alloc, nullptr));
        ::eng::ResourceTracker::incBuffer();
    };

    if (recreateAssetTable) createOrGrowBuffer(m_AssetTableBuffer, m_AssetTableAlloc, m_AssetTableCapacity, requiredAssetTableSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    if (recreateNodes) createOrGrowBuffer(m_NodesBuffer, m_NodesAlloc, m_NodesCapacity, requiredNodesSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    if (recreateClusters) createOrGrowBuffer(m_ClustersBuffer, m_ClustersAlloc, m_ClustersCapacity, requiredClustersSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    if (recreatePageDesc) createOrGrowBuffer(m_PageDescBuffer, m_PageDescAlloc, m_PageDescCapacity, requiredPageDescSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    if (recreateGeometry) createOrGrowBuffer(m_ResidentGeometryBuffer, m_ResidentGeometryAlloc, m_ResidentGeometryCapacity, requiredGeometryBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    // 2. Map and upload data sequentially
    void* assetTableMap = nullptr;
    void* nodesMap = nullptr;
    void* clustersMap = nullptr;
    void* pageDescMap = nullptr;
    void* geometryMap = nullptr;

    VK_CHECK(vmaMapMemory(m_Allocator, m_AssetTableAlloc, &assetTableMap));
    VK_CHECK(vmaMapMemory(m_Allocator, m_NodesAlloc, &nodesMap));
    VK_CHECK(vmaMapMemory(m_Allocator, m_ClustersAlloc, &clustersMap));
    VK_CHECK(vmaMapMemory(m_Allocator, m_PageDescAlloc, &pageDescMap));
    VK_CHECK(vmaMapMemory(m_Allocator, m_ResidentGeometryAlloc, &geometryMap));

    uint32_t nodeOffset = 0;
    uint32_t clusterOffset = 0;
    uint32_t pageOffset = 0;
    uint32_t geometryByteOffset = 0;

    for (size_t i = 0; i < m_Assets.size(); ++i) {
        const auto& asset = m_Assets[i];

        // Fill GPU Asset Metadata
        GPURVGAsset gpuAsset{};
        gpuAsset.firstNodeIndex = nodeOffset;
        gpuAsset.nodeCount = asset->GetNodeCount();
        gpuAsset.firstClusterIndex = clusterOffset;
        gpuAsset.clusterCount = asset->GetClusterCount();
        
        // Register asset pages with RVGPageStreamingManager and store table offset
        uint32_t firstPageTableOffset = RVGPageStreamingManager::Get().RegisterAssetPages(static_cast<uint32_t>(i), asset.get());
        gpuAsset.firstPageOffset = firstPageTableOffset;
        
        gpuAsset.pageCount = asset->GetPageCount();
        if (asset->GetPageSizes().size() > 0) {
            gpuAsset.pageSize = asset->GetPageSizes()[0];
        }
        std::memcpy(static_cast<uint8_t*>(assetTableMap) + i * sizeof(GPURVGAsset), &gpuAsset, sizeof(GPURVGAsset));

        // Copy Nodes
        std::memcpy(static_cast<uint8_t*>(nodesMap) + nodeOffset * sizeof(RVGNode), asset->GetNodes().data(), asset->GetNodeCount() * sizeof(RVGNode));
        nodeOffset += asset->GetNodeCount();

        // Copy Clusters
        std::memcpy(static_cast<uint8_t*>(clustersMap) + clusterOffset * sizeof(RVGCluster), asset->GetClusters().data(), asset->GetClusterCount() * sizeof(RVGCluster));
        clusterOffset += asset->GetClusterCount();

        // Copy Page Descriptions (Sizes)
        std::memcpy(static_cast<uint8_t*>(pageDescMap) + pageOffset * sizeof(uint32_t), asset->GetPageSizes().data(), asset->GetPageCount() * sizeof(uint32_t));
        pageOffset += asset->GetPageCount();

        // Upload root fallback page (page 0) to permanently resident region of physical page pool
        if (m_Resources) {
            RVGPageStreamingManager::Get().UploadRootPage(*m_Resources, static_cast<uint32_t>(i), asset->GetPagesData()[0]);
        }
    }

    vmaUnmapMemory(m_Allocator, m_AssetTableAlloc);
    vmaUnmapMemory(m_Allocator, m_NodesAlloc);
    vmaUnmapMemory(m_Allocator, m_ClustersAlloc);
    vmaUnmapMemory(m_Allocator, m_PageDescAlloc);
    vmaUnmapMemory(m_Allocator, m_ResidentGeometryAlloc);

    LOG_INFO("RVGRegistry: Rebuilt GPU buffers and uploaded loaded asset data successfully.");
}

VkBuffer RVGRegistry::GetResidentGeometryBuffer() const {
    VkBuffer poolBuffer = RVGPageStreamingManager::Get().GetPhysicalPagePoolBuffer();
    if (poolBuffer != VK_NULL_HANDLE) {
        return poolBuffer;
    }
    return m_ResidentGeometryBuffer;
}

} // namespace eng::renderer
