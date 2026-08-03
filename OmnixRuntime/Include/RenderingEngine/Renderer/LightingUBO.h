#pragma once
#include <glm/glm.hpp>

struct alignas(16) ShadowGPUSettings
{
    glm::vec4 shadowParams;
    // x = strength
    // y = constant bias
    // z = slope bias
    // w = softness

    glm::uvec4 shadowFlags;
    // x = PCF kernel size
};

struct alignas(16) LocalLightShadowGPU
{
    glm::mat4 lightSpaceMatrix;
    glm::vec4 atlasViewport; // x=u, y=v, z=size_u, w=size_v in [0, 1]
    float bias;
    float normalBias;
    float farPlane;
    float shadowEnabled;
};

struct alignas(16) ReflectionProbeGPU
{
    glm::vec4 positionIntensity;  // xyz = position, w = intensity
    glm::vec4 boxMinPriority;     // xyz = boxMin, w = priority
    glm::vec4 boxMaxBlend;        // xyz = boxMax, w = blendDistance
    glm::uvec4 flags;             // x = isBox, y = valid, zw = unused
};

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
    alignas(16) glm::vec4 pointLayerMasks[16];
    alignas(16) glm::vec4 spotLayerMasks[16];

    // Shadow mapping uniform data
    alignas(16) glm::mat4 directionalLightProjView;
    alignas(16) glm::mat4 directionalLightProjViews[4];
    alignas(16) glm::vec4 cascadeSplitDepths;
    alignas(16) float shadowBias;
    float shadowNormalBias;
    float shadowSlopeBias;
    float shadowStrength;
    uint32_t shadowLightCast;
    int32_t pcfKernelSize;
    uint32_t shadowResolution;
    uint32_t paddingVal2;

    alignas(16) ShadowGPUSettings shadowSettings;

    alignas(16) uint32_t skyLightMode;
    float skyLightRotation;
    float skyLightDiffuseIntensity;
    float skyLightSpecularIntensity;
    alignas(16) float skyLightExposureOffset;

    alignas(16) LocalLightShadowGPU spotLightShadows[16];
    alignas(16) LocalLightShadowGPU pointLightShadows[16][6];

    alignas(16) uint32_t reflectionProbeCount;
    uint32_t padProbe0, padProbe1, padProbe2;
    alignas(16) ReflectionProbeGPU reflectionProbes[4];
};
