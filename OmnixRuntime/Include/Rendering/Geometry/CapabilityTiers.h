#pragma once
#include <vulkan/vulkan.h>
#include <string>

namespace eng::renderer {

struct CapabilityReport {
    // Queries
    bool hasDrawIndexedIndirectCount = false;
    bool hasBufferDeviceAddress = false;
    bool hasDescriptorIndexing = false;
    bool hasMeshShaderEXT = false;
    bool hasMeshShaderNV = false;
    
    // Subgroups
    uint32_t subgroupSize = 0;
    VkShaderStageFlags subgroupSupportedStages = 0;
    VkSubgroupFeatureFlags subgroupSupportedOperations = 0;
    
    // Storage Buffer Limits
    uint32_t maxStorageBufferRange = 0;
    
    // Atomics
    bool hasFragmentStoresAndAtomics = false;
    bool hasVertexPipelineStoresAndAtomics = false;
    bool hasBufferAtomics = false; // VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures etc, or subgroup atomics

    uint32_t selectedTier = 0; // 0, 1, 2, or 3
    std::string tierDescription;
};

class CapabilityTracker {
public:
    static CapabilityReport CheckCapabilities(VkPhysicalDevice physicalDevice);
    static void PrintReport(const CapabilityReport& report);
};

} // namespace eng::renderer
