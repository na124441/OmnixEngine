#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <vulkan/vulkan.h>
#include "Core/Engine/VmaUsage.h"
#include "Rendering/Geometry/Assets/RVGAsset.h"

namespace eng::renderer {

struct EngineResources;

struct GPURVGAsset {
    uint32_t firstNodeIndex = 0;
    uint32_t nodeCount = 0;
    uint32_t firstClusterIndex = 0;
    uint32_t clusterCount = 0;
    uint32_t firstPageOffset = 0; // Byte offset in global resident geometry buffer
    uint32_t pageCount = 0;
    uint32_t pageSize = 4096;
    uint32_t padding = 0; // Align to 32 bytes
};

class RVGRegistry {
public:
    RVGRegistry() = default;
    ~RVGRegistry() { Shutdown(); }

    void Initialize(EngineResources& resources);
    void Shutdown();

    // Loads an RVG asset if not loaded, returns assetIndex. Returns UINT32_MAX on failure.
    uint32_t LoadAsset(EngineResources& resources, const std::string& filepath);

    uint32_t GetAssetCount() const { return static_cast<uint32_t>(m_Assets.size()); }
    const RVGAsset* GetAsset(uint32_t assetIndex) const { return m_Assets[assetIndex].get(); }

    VkBuffer GetAssetTableBuffer() const { return m_AssetTableBuffer; }
    VkBuffer GetNodesBuffer() const { return m_NodesBuffer; }
    VkBuffer GetClustersBuffer() const { return m_ClustersBuffer; }
    VkBuffer GetPageDescBuffer() const { return m_PageDescBuffer; }
    VkBuffer GetResidentGeometryBuffer() const;

    static RVGRegistry& Get() {
        static RVGRegistry registry;
        return registry;
    }

private:
    void rebuildBuffers(EngineResources& resources);

    std::vector<std::unique_ptr<RVGAsset>> m_Assets;
    std::unordered_map<std::string, uint32_t> m_AssetsMap;

    // GPU metadata & resident geometry buffers
    VkDevice m_Device = VK_NULL_HANDLE;
    VmaAllocator m_Allocator = VK_NULL_HANDLE;
    EngineResources* m_Resources = nullptr;

    VkBuffer m_AssetTableBuffer = VK_NULL_HANDLE;
    VmaAllocation m_AssetTableAlloc = VK_NULL_HANDLE;
    size_t m_AssetTableCapacity = 0;

    VkBuffer m_NodesBuffer = VK_NULL_HANDLE;
    VmaAllocation m_NodesAlloc = VK_NULL_HANDLE;
    size_t m_NodesCapacity = 0;

    VkBuffer m_ClustersBuffer = VK_NULL_HANDLE;
    VmaAllocation m_ClustersAlloc = VK_NULL_HANDLE;
    size_t m_ClustersCapacity = 0;

    VkBuffer m_PageDescBuffer = VK_NULL_HANDLE;
    VmaAllocation m_PageDescAlloc = VK_NULL_HANDLE;
    size_t m_PageDescCapacity = 0;

    VkBuffer m_ResidentGeometryBuffer = VK_NULL_HANDLE;
    VmaAllocation m_ResidentGeometryAlloc = VK_NULL_HANDLE;
    size_t m_ResidentGeometryCapacity = 0;
};

} // namespace eng::renderer
