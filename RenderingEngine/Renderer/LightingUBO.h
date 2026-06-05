#pragma once
#include <glm/glm.hpp>

/* Matches the layout in the fragment shader (set = 2, binding = 0) */
struct LightData
{
    alignas(16) glm::vec4 ambientColorIntensity; // rgb = color, w = intensity
    alignas(16) glm::vec4 directionalDirectionIntensity; // xyz = direction, w = intensity
    alignas(16) glm::vec4 directionalColor; // rgb = color, w = unused
    alignas(16) glm::vec4 pointPositionsRadius[16]; // xyz = pos, w = radius
    alignas(16) glm::vec4 pointColorsIntensity[16]; // rgb = color, w = intensity
    alignas(16) uint32_t pointLightCount;
    uint32_t shadingMode; // 0 = Lit, 1 = Unlit
    uint32_t padding[2];
};
