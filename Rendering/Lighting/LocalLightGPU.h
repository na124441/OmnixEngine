#pragma once

#include <glm/glm.hpp>
#include <cstdint>

namespace Omnix::Radiance
{
    enum class LocalLightType : uint32_t
    {
        Point = 0,
        Spot = 1
    };

    struct alignas(16) LocalLightGPU
    {
        glm::vec4 positionRange;
        // xyz = world position
        // w = range

        glm::vec4 colorIntensity;
        // rgb = color
        // w = intensity

        glm::vec4 directionType;
        // xyz = direction
        // w = type: 0 point, 1 spot

        glm::vec4 spotAngles;
        // x = inner angle radians
        // y = outer angle radians
        // z/w unused
    };
}
