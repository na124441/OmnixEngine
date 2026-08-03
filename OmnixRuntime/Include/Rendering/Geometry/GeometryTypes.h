#pragma once
#include "Rendering/Geometry/GeometryHandle.h"
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace eng::renderer {

struct GeometryAllocation {
    uint64_t vertexByteOffset = 0;
    uint64_t vertexByteSize = 0;
    uint64_t indexByteOffset = 0;
    uint64_t indexByteSize = 0;
    uint32_t firstIndex = 0;
    int32_t vertexOffset = 0;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    uint32_t generation = 0;
    uint32_t flags = 0;
};

enum class GeometryRenderingPath {
    ConventionalCPU,
    ConventionalGPUDriven,
    VirtualGeometry,
    Fallback
};

inline const char* GeometryRenderingPathToString(GeometryRenderingPath path) {
    switch (path) {
        case GeometryRenderingPath::ConventionalCPU: return "ConventionalCPU";
        case GeometryRenderingPath::ConventionalGPUDriven: return "ConventionalGPUDriven";
        case GeometryRenderingPath::VirtualGeometry: return "VirtualGeometry";
        case GeometryRenderingPath::Fallback: return "Fallback";
    }
    return "Unknown";
}

struct SubmeshDesc {
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
    int32_t vertexOffset = 0;
    uint32_t materialIndex = 0;
};

struct MeshAsset {
    std::string sourcePath;
    uint64_t sourceAssetHash = 0;
    std::vector<float> cpuVertexData; // Interleaved or position-only for reference
    std::vector<uint32_t> cpuIndexData;
    std::vector<SubmeshDesc> submeshes;
    std::vector<uint32_t> materialSlots;
    glm::vec3 localBoundsMin{0.0f};
    glm::vec3 localBoundsMax{0.0f};
    std::string importSettings;
};

struct MeshRuntime {
    GeometryHandle handle;
    uint32_t vertexOffset = 0;
    uint32_t indexOffset = 0;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    bool isGpuResident = false;
    uint32_t runtimeGeneration = 0;
    bool isFallback = false;
};

struct ClusterDescriptor {
    glm::vec4 boundsSphere; // xyz = center, w = radius
    glm::vec3 coneAxis;
    float coneCutoff = 0.0f;
    uint32_t firstTriangle = 0;
    uint32_t triangleCount = 0;
};

struct PageDescriptor {
    uint32_t offset = 0;
    uint32_t size = 0;
};

struct VirtualGeometryAsset {
    std::string hierarchyMetadata;
    std::vector<ClusterDescriptor> clusters;
    std::vector<PageDescriptor> pages;
    GeometryHandle rootGeometry;
    GeometryHandle fallbackMesh;
    std::vector<uint32_t> materialSlots;
    glm::vec3 boundsMin{0.0f};
    glm::vec3 boundsMax{0.0f};
    float maxGeometricError = 0.0f;
};

struct VirtualGeometryRuntime {
    VirtualGeometryHandle handle;
    bool pagesResident = false;
    uint32_t loadedPageCount = 0;
    uint32_t totalPageCount = 0;
};

} // namespace eng::renderer
