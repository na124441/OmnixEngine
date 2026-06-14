#pragma once

#include <glm/glm.hpp>
#include <cstdint>

namespace Omnix::Radiance
{
    struct alignas(16) ClusterBoundsGPU
    {
        glm::vec4 minPoint;
        // xyz = min view-space approximation bounds or screen/depth slice
        // w = unused
        glm::vec4 maxPoint;
        // xyz = max view-space approximation bounds or screen/depth slice
        // w = unused
    };

    struct alignas(16) ClusterRangeGPU
    {
        uint32_t offset;
        uint32_t count;
        uint32_t overflow;
        uint32_t pad;
    };

    struct ClusterSettingsGPU
    {
        uint32_t tileCountX;
        uint32_t tileCountY;
        uint32_t depthSliceCount;
        uint32_t maxLightsPerCluster;

        uint32_t clusterCount;
        uint32_t lightCount;
        float nearPlane;
        float farPlane;
    };

    struct ClusterSettings
    {
        uint32_t tileSizeX = 64;
        uint32_t tileSizeY = 64;
        uint32_t depthSliceCount = 16;
        uint32_t maxLightsPerCluster = 64;
    };
}
