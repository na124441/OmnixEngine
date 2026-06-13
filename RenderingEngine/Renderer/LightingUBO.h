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
    uint32_t spotLightCount;
    uint32_t paddingVal;
    alignas(16) glm::vec4 spotPositionsRange[16];
    alignas(16) glm::vec4 spotDirectionsIntensity[16];
    alignas(16) glm::vec4 spotColors[16];
    alignas(16) glm::vec4 spotAngles[16];

    // Shadow mapping uniform data
    alignas(16) glm::mat4 lightSpaceMatrix;
    alignas(16) float shadowBias;
    float shadowNormalBias;
    float shadowStrength;
    uint32_t shadowLightCast;
};
