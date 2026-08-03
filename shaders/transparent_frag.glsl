#version 450
layout(location = 0) out vec4 outColor;

// Binding 0: Camera Uniform Buffer
layout(set = 0, binding = 0) uniform RadianceFrame
{
    mat4 view;
    mat4 projection;
    mat4 inverseView;
    mat4 inverseProjection;
    mat4 inverseViewProjection;

    vec4 cameraPosition;
    vec4 viewportSize;

    vec4 skyTopColorIntensity;
    vec4 skyHorizonColorBlend;
    vec4 skyGroundColorIntensity;

    vec4 sunDirectionIntensity;
    vec4 sunColorAngularSize;

    vec4 exposureSettings;
    uvec4 renderFlags;
} frame;

struct InstanceData {
    mat4 worldMatrix;
    mat4 previousWorldMatrix;
    vec4 boundsCenterRadius;
    uint meshIndex;
    uint materialIndex;
    uint objectID;
    uint flags;
};

// Binding 1: Instance Storage Buffer
layout(std430, set = 0, binding = 1) readonly buffer InstanceBuffer {
    InstanceData instances[];
} inst;

struct MaterialData {
    vec4 baseColorFactor;
    float roughnessFactor;
    float metallicFactor;
    float normalScale;
    float emissiveStrength;

    float hasAlbedoMap;
    float useNormalMap;
    float hasMetallicRoughnessMap;
    float hasAOMap;

    float hasEmissiveMap;
    uint blendMode;
    uint shadingModel;
    uint padding;
};

// Binding 2: Material Storage Buffer
layout(std430, set = 0, binding = 2) readonly buffer MaterialBuffer {
    MaterialData materials[];
} mat;

struct DirectionalLightData
{
    vec3 direction;
    float intensity;
    vec4 color;
};

struct LocalLightShadowGPU
{
    mat4 lightSpaceMatrix;
    vec4 atlasViewport;
    float bias;
    float normalBias;
    float farPlane;
    float shadowEnabled;
};

struct ReflectionProbeGPU
{
    vec4 positionIntensity;  // xyz = position, w = intensity
    vec4 boxMinPriority;     // xyz = boxMin, w = priority
    vec4 boxMaxBlend;        // xyz = boxMax, w = blendDistance
    uvec4 flags;             // x = isBox, y = valid, zw = unused
};

// Binding 3: Light Storage Buffer
layout(std430, set = 0, binding = 3) readonly buffer LightBuffer {
    vec4 ambientColorIntensity; // rgb = color, w = intensity
    DirectionalLightData directional;
    vec4 pointPositionsRadius[16]; // xyz = pos, w = radius
    vec4 pointColorsIntensity[16]; // rgb = color, w = intensity
    uint pointLightCount;
    uint shadingMode;
    uint spotLightCount;
    uint paddingVal;
    vec4 spotPositionsRange[16];
    vec4 spotDirectionsIntensity[16];
    vec4 spotColors[16];
    vec4 spotAngles[16];
    vec4 pointLayerMasks[16];
    vec4 spotLayerMasks[16];
    
    // Shadow mapping settings
    mat4 directionalLightProjView;
    mat4 directionalLightProjViews[4];
    vec4 cascadeSplitDepths;
    float shadowBias;
    float shadowNormalBias;
    float shadowSlopeBias;
    float shadowStrength;
    uint shadowLightCast;
    int pcfKernelSize;
    uint shadowResolution;
    uint paddingVal2;

    vec4 shadowParams;
    uvec4 shadowFlags;

    uint skyLightMode;
    float skyLightRotation;
    float skyLightDiffuseIntensity;
    float skyLightSpecularIntensity;
    float skyLightExposureOffset;

    LocalLightShadowGPU spotLightShadows[16];
    LocalLightShadowGPU pointLightShadows[16][6];

    uint reflectionProbeCount;
    uint padProbe0;
    uint padProbe1;
    uint padProbe2;
    ReflectionProbeGPU reflectionProbes[4];
} light;

// Set 1: Material textures
layout(set = 1, binding = 1) uniform sampler2D albedoMap;
layout(set = 1, binding = 2) uniform sampler2D normalMap;
layout(set = 1, binding = 3) uniform sampler2D metallicRoughnessMap;
layout(set = 1, binding = 4) uniform sampler2D aoMap;
layout(set = 1, binding = 5) uniform sampler2D emissiveMap;

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vUV;
layout(location = 3) in vec3 vCameraPos;
layout(location = 4) flat in uint vMaterialIndex;
layout(location = 5) flat in uint vEntityID;
layout(location = 7) in vec4 vTangent;

#include "brdf.glsl"

// TBN frame formulation for normal perturbation
vec3 perturbNormal(vec3 N, vec3 V, vec2 uv, float normalScale, vec4 tangent)
{
    vec3 tangentNormal = texture(normalMap, uv).xyz * 2.0 - 1.0;
    tangentNormal.xy *= normalScale; // Scale normal perturbation

    if (length(tangent.xyz) > 1e-4) {
        vec3 T = normalize(tangent.xyz);
        T = normalize(T - dot(T, N) * N);
        vec3 B = cross(N, T) * tangent.w;
        mat3 TBN = mat3(T, B, N);
        return normalize(TBN * tangentNormal);
    } else {
        vec3 dp1 = dFdx(vWorldPos);
        vec3 dp2 = dFdy(vWorldPos);
        vec2 duv1 = dFdx(uv);
        vec2 duv2 = dFdy(uv);

        vec3 dp2perp = cross(dp2, N);
        vec3 dp1perp = cross(N, dp1);
        vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
        vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

        float invmax = inversesqrt(max(dot(T, T), dot(B, B)));
        return normalize(T * (tangentNormal.x * invmax) + B * (tangentNormal.y * invmax) + N * tangentNormal.z);
    }
}

void main()
{
    // Fetch Material Parameters
    vec4 baseColorFactor = mat.materials[vMaterialIndex].baseColorFactor;
    float roughnessFactor = mat.materials[vMaterialIndex].roughnessFactor;
    float metallicFactor = mat.materials[vMaterialIndex].metallicFactor;
    float normalScale = mat.materials[vMaterialIndex].normalScale;
    float emissiveStrength = mat.materials[vMaterialIndex].emissiveStrength;

    float hasAlbedoMap = mat.materials[vMaterialIndex].hasAlbedoMap;
    float useNormalMap = mat.materials[vMaterialIndex].useNormalMap;
    float hasMetallicRoughnessMap = mat.materials[vMaterialIndex].hasMetallicRoughnessMap;
    float hasAOMap = mat.materials[vMaterialIndex].hasAOMap;
    float hasEmissiveMap = mat.materials[vMaterialIndex].hasEmissiveMap;
    uint shadingModel = mat.materials[vMaterialIndex].shadingModel;

    vec4 albedo = baseColorFactor;
    if (hasAlbedoMap > 0.5) {
        albedo = texture(albedoMap, vUV) * baseColorFactor;
    }

    vec3 N = normalize(vNormal);
    if (useNormalMap > 0.5) {
        N = perturbNormal(N, normalize(vCameraPos - vWorldPos), vUV, normalScale, vTangent);
    }

    float roughness = roughnessFactor;
    float metallic = metallicFactor;
    if (hasMetallicRoughnessMap > 0.5) {
        vec4 mrSample = texture(metallicRoughnessMap, vUV);
        roughness *= mrSample.g;
        metallic *= mrSample.b;
    }

    float AO = 1.0;
    if (hasAOMap > 0.5) {
        AO = texture(aoMap, vUV).r;
    }

    vec3 emissive = vec3(0.0);
    if (hasEmissiveMap > 0.5) {
        emissive = texture(emissiveMap, vUV).rgb * emissiveStrength;
    }

    // Default exposure (loaded from camera planes)
    float exposure = frame.exposureSettings.x;

    // Direct lighting computation (PBR Cook-Torrance)
    vec3 V = normalize(vCameraPos - vWorldPos);
    vec3 F0 = mix(vec3(0.04), albedo.rgb, metallic);

    // ----- Shading Modes Check -----
    if (light.shadingMode == 10) {
        outColor = vec4(albedo.rgb, albedo.a);
        return;
    }
    if (light.shadingMode == 1 || shadingModel == 1) {
        vec3 color = albedo.rgb + emissive;
        color = color / (color + vec3(1.0));
        color = pow(color, vec3(1.0/2.2));
        outColor = vec4(color, albedo.a);
        return;
    }
    if (light.shadingMode == 2) {
        float z = gl_FragCoord.z;
        float near = frame.projection[3][2] / frame.projection[2][2];
        float far = frame.projection[3][2] / (1.0 + frame.projection[2][2]);
        float linear = (near * far) / (far - z * (far - near));
        float maxDepthVis = min(far, 100.0);
        float d = clamp((linear - near) / (maxDepthVis - near), 0.0, 1.0);
        outColor = vec4(vec3(d), albedo.a);
        return;
    }
    if (light.shadingMode == 3) {
        outColor = vec4(normalize(N) * 0.5 + 0.5, albedo.a);
        return;
    }
    if (light.shadingMode == 4) {
        outColor = vec4(vec3(roughness), albedo.a);
        return;
    }
    if (light.shadingMode == 5) {
        outColor = vec4(vec3(metallic), albedo.a);
        return;
    }
    if (light.shadingMode == 6) {
        outColor = vec4(vec3(AO), albedo.a);
        return;
    }
    if (light.shadingMode == 7) {
        float id = float(vEntityID);
        vec3 hashColor = vec3(
            fract(sin(id * 12.9898) * 43758.5453),
            fract(sin(id * 78.233) * 43758.5453),
            fract(sin(id * 45.164) * 43758.5453)
        );
        if (id == 0.0) hashColor = vec3(0.0);
        outColor = vec4(hashColor, albedo.a);
        return;
    }
    if (light.shadingMode == 8) {
        outColor = vec4(emissive, albedo.a);
        return;
    }
    if (light.shadingMode == 13) {
        outColor = vec4(normalize(vTangent.xyz) * 0.5 + 0.5, albedo.a);
        return;
    }
    if (light.shadingMode == 15) {
        outColor = vec4(vUV, 0.0, albedo.a);
        return;
    }
    if (light.shadingMode == 16) {
        outColor = vec4(F0, albedo.a);
        return;
    }

    vec3 Lo = vec3(0.0);

    // 1. Directional Light
    if (light.directional.intensity > 0.0) {
        vec3 L = normalize(light.directional.direction);
        vec3 radiance = light.directional.color.rgb * light.directional.intensity;
        vec3 diffVal, specVal;
        vec3 brdf = EvaluateDirectBRDF(albedo.rgb, N, V, L, metallic, roughness, true, diffVal, specVal);
        Lo += brdf * radiance;
    }

    // 2. Point Lights
    for (uint i = 0; i < light.pointLightCount && i < 16; ++i) {
        vec3 lightPos = light.pointPositionsRadius[i].xyz;
        float radius = light.pointPositionsRadius[i].w;
        vec3 lightColor = light.pointColorsIntensity[i].rgb;
        float intensity = light.pointColorsIntensity[i].w;

        vec3 diff = lightPos - vWorldPos;
        float distance = length(diff);
        if (distance > radius) continue;

        vec3 L = normalize(diff);
        float x = distance / radius;
        float attenuation = clamp(1.0 - x * x, 0.0, 1.0);
        attenuation = attenuation * attenuation;

        vec3 radiance = lightColor * intensity * attenuation;
        vec3 diffVal, specVal;
        vec3 brdf = EvaluateDirectBRDF(albedo.rgb, N, V, L, metallic, roughness, true, diffVal, specVal);
        Lo += brdf * radiance;
    }

    // 3. Spot Lights
    for (uint i = 0; i < light.spotLightCount && i < 16; ++i) {
        vec3 lightPos = light.spotPositionsRange[i].xyz;
        float range = light.spotPositionsRange[i].w;
        vec3 lightDir = normalize(light.spotDirectionsIntensity[i].xyz);
        float intensity = light.spotDirectionsIntensity[i].w;
        vec3 lightColor = light.spotColors[i].rgb;
        float cosInner = light.spotColors[i].w;
        float cosOuter = light.spotAngles[i].x;

        vec3 diff = lightPos - vWorldPos;
        float distance = length(diff);
        if (distance > range) continue;

        vec3 L = normalize(diff);
        
        // Spot cone attenuation
        float theta = dot(normalize(-diff), lightDir);
        float cone = clamp((theta - cosOuter) / max(cosInner - cosOuter, 0.0001), 0.0, 1.0);
        cone = cone * cone;
        if (cone <= 0.0) continue;

        float x = distance / range;
        float distAttenuation = clamp(1.0 - x * x, 0.0, 1.0);
        distAttenuation = distAttenuation * distAttenuation;

        vec3 radiance = lightColor * intensity * distAttenuation * cone;
        vec3 diffVal, specVal;
        vec3 brdf = EvaluateDirectBRDF(albedo.rgb, N, V, L, metallic, roughness, true, diffVal, specVal);
        Lo += brdf * radiance;
    }

    // Ambient lighting
    vec3 ambient = light.ambientColorIntensity.rgb * light.ambientColorIntensity.w * albedo.rgb * AO;

    // Final color with exposure (emissive added after exposure)
    vec3 color = (ambient + Lo) * exposure + emissive;

    outColor = vec4(color, albedo.a);
}
