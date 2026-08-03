#include "Rendering/Geometry/CapabilityTiers.h"
#include "Core/Engine/Log.h"
#include <sstream>
#include <iostream>
#include <vector>

namespace eng::renderer {

CapabilityReport CapabilityTracker::CheckCapabilities(VkPhysicalDevice physicalDevice)
{
    CapabilityReport report{};

    if (physicalDevice == VK_NULL_HANDLE) {
        LOG_ERROR("CapabilityTracker: Null physical device passed.");
        return report;
    }

    // 1. Get properties and features using features2/properties2 chains
    VkPhysicalDeviceProperties2 properties2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    VkPhysicalDeviceSubgroupProperties subgroupProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES };
    properties2.pNext = &subgroupProperties;
    vkGetPhysicalDeviceProperties2(physicalDevice, &properties2);

    report.subgroupSize = subgroupProperties.subgroupSize;
    report.subgroupSupportedStages = subgroupProperties.supportedStages;
    report.subgroupSupportedOperations = subgroupProperties.supportedOperations;
    report.maxStorageBufferRange = properties2.properties.limits.maxStorageBufferRange;

    VkPhysicalDeviceFeatures2 features2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    
    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };
    features2.pNext = &bufferDeviceAddressFeatures;

    VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES };
    bufferDeviceAddressFeatures.pNext = &descriptorIndexingFeatures;

    VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT };
    descriptorIndexingFeatures.pNext = &meshShaderFeaturesEXT;

    vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

    report.hasFragmentStoresAndAtomics = features2.features.fragmentStoresAndAtomics;
    report.hasVertexPipelineStoresAndAtomics = features2.features.vertexPipelineStoresAndAtomics;
    report.hasBufferDeviceAddress = bufferDeviceAddressFeatures.bufferDeviceAddress;
    report.hasDescriptorIndexing = descriptorIndexingFeatures.descriptorBindingPartiallyBound && 
                                   descriptorIndexingFeatures.runtimeDescriptorArray;
    report.hasMeshShaderEXT = meshShaderFeaturesEXT.meshShader && meshShaderFeaturesEXT.taskShader;

    // Check drawIndirectCount (Vulkan 1.2 feature or extension)
    VkPhysicalDeviceVulkan12Features features12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    VkPhysicalDeviceFeatures2 features2_12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    features2_12.pNext = &features12;
    vkGetPhysicalDeviceFeatures2(physicalDevice, &features2_12);
    report.hasDrawIndexedIndirectCount = features12.drawIndirectCount;

    // Check for mesh shader extensions via extension enumeration
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
    if (extensionCount > 0) {
        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensions.data());
        for (const auto& ext : extensions) {
            std::string name(ext.extensionName);
            // Do not promote EXT mesh shader support from extension presence alone.
            // The feature bits queried above decide whether mesh/task shaders are usable.
            if (name == "VK_NV_mesh_shader") {
                report.hasMeshShaderNV = true;
            }
        }
    }

    // Check atomic support (subgroup basic atomics are usually part of subgroup operations)
    report.hasBufferAtomics = (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_BASIC_BIT) != 0;

    // 2. Select Tier
    // Tier 0: CPU-driven indexed rendering (Fallback)
    // Tier 1: GPU-driven indexed indirect rendering (Requires drawIndirectCount, BDA, Descriptor Indexing, Atomics)
    // Tier 2: Mesh shader cluster rendering (Requires Tier 1 + Mesh Shader extension)
    // Tier 3: Software micro-triangle rasterization (Requires Tier 2 + Subgroup/Atomics/specific limits)
    if (report.hasDrawIndexedIndirectCount && report.hasBufferDeviceAddress && report.hasDescriptorIndexing && report.hasBufferAtomics) {
        if (report.hasMeshShaderEXT || report.hasMeshShaderNV) {
            // Check if limits are sufficient for software rasterizer
            if (report.maxStorageBufferRange >= 128 * 1024 * 1024 && (report.subgroupSupportedOperations & VK_SUBGROUP_FEATURE_ARITHMETIC_BIT)) {
                report.selectedTier = 3;
                report.tierDescription = "Tier 3: Software Micro-Triangle Rasterization (Full RVG Native)";
            } else {
                report.selectedTier = 2;
                report.tierDescription = "Tier 2: Mesh Shader Cluster Rendering";
            }
        } else {
            report.selectedTier = 1;
            report.tierDescription = "Tier 1: GPU-Driven Indexed Indirect Rendering";
        }
    } else {
        report.selectedTier = 0;
        report.tierDescription = "Tier 0: CPU-Driven Indexed Rendering (Reference Path)";
    }

    return report;
}

void CapabilityTracker::PrintReport(const CapabilityReport& report)
{
    std::stringstream ss;
    ss << "\n==================================================\n";
    ss << "       RADIANCE RVG STARTUP CAPABILITY REPORT      \n";
    ss << "==================================================\n";
    ss << " [Features]\n";
    ss << "  - vkCmdDrawIndexedIndirectCount: " << (report.hasDrawIndexedIndirectCount ? "SUPPORTED" : "UNSUPPORTED") << "\n";
    ss << "  - Buffer Device Address (BDA):  " << (report.hasBufferDeviceAddress ? "SUPPORTED" : "UNSUPPORTED") << "\n";
    ss << "  - Descriptor Indexing:          " << (report.hasDescriptorIndexing ? "SUPPORTED" : "UNSUPPORTED") << "\n";
    ss << "  - Mesh Shader EXT:              " << (report.hasMeshShaderEXT ? "SUPPORTED" : "UNSUPPORTED") << "\n";
    ss << "  - Mesh Shader NV:               " << (report.hasMeshShaderNV ? "SUPPORTED" : "UNSUPPORTED") << "\n";
    ss << "\n [Subgroups]\n";
    ss << "  - Subgroup Size:                " << report.subgroupSize << "\n";
    ss << "  - Supported Stages:             0x" << std::hex << report.subgroupSupportedStages << std::dec << "\n";
    ss << "  - Supported Operations:         0x" << std::hex << report.subgroupSupportedOperations << std::dec << "\n";
    ss << "\n [Limits & Atomics]\n";
    ss << "  - Max Storage Buffer Range:     " << report.maxStorageBufferRange << " bytes\n";
    ss << "  - Fragment Stores & Atomics:    " << (report.hasFragmentStoresAndAtomics ? "SUPPORTED" : "UNSUPPORTED") << "\n";
    ss << "  - Vertex Stores & Atomics:      " << (report.hasVertexPipelineStoresAndAtomics ? "SUPPORTED" : "UNSUPPORTED") << "\n";
    ss << "  - Buffer Atomics:               " << (report.hasBufferAtomics ? "SUPPORTED" : "UNSUPPORTED") << "\n";
    ss << "\n [Selection]\n";
    ss << "  - Selected Capability Tier:     " << report.tierDescription << "\n";
    ss << "==================================================\n";

    LOG_INFO(ss.str());
}

} // namespace eng::renderer
