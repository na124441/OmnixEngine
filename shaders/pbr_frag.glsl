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

    float clearcoatFactor;
    float clearcoatRoughness;
    float paddingFloat1;
    float paddingFloat2;

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

// Binding 3: Light Storage Buffer
layout(std430, set = 0, binding = 3) readonly buffer LightBuffer {
    vec4 ambientColorIntensity; // rgb = color, w = intensity
    vec4 directionalDirectionIntensity; // xyz = direction, w = intensity
    vec4 directionalColor; // rgb = color, w = unused
    vec4 pointPositionsRadius[16]; // xyz = pos, w = radius
    vec4 pointColorsIntensity[16]; // rgb = color, w = intensity
    uint pointLightCount;
    uint shadingMode;
    uint padding[2];
} light;

// Binding 4: ObjectID Storage Buffer
layout(std430, set = 0, binding = 4) readonly buffer ObjectIDBuffer {
    uint entityIDs[];
} objId;

// Textures remain at Set 1
layout(set = 1, binding = 1) uniform sampler2D albedoMap;
layout(set = 1, binding = 2) uniform sampler2D normalMap;

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

        float r = 1.0 / (duv1.x * duv2.y - duv1.y * duv2.x + 1e-6);
        vec3 T = (dp1 * duv2.y - dp2 * duv1.y) * r;
        vec3 B = (dp2 * duv1.x - dp1 * duv2.x) * r;

        T = normalize(T - dot(T, N) * N);
        B = cross(N, T);

        mat3 TBN = mat3(T, B, N);
        return normalize(TBN * tangentNormal);
    }
}

void main()
{
    // ----- Fetch Material Parameters -----
    vec4 baseColorFactor = mat.materials[vMaterialIndex].baseColorFactor;
    float roughness = mat.materials[vMaterialIndex].roughnessFactor;
    float metallic = mat.materials[vMaterialIndex].metallicFactor;
    float normalScale = mat.materials[vMaterialIndex].normalScale;
    float emissiveStrength = mat.materials[vMaterialIndex].emissiveStrength;

    float hasAlbedoMap = mat.materials[vMaterialIndex].hasAlbedoMap;
    float useNormalMap = mat.materials[vMaterialIndex].useNormalMap;
    uint shadingModel = mat.materials[vMaterialIndex].shadingModel;

    vec3 albedo = baseColorFactor.rgb;
    if (hasAlbedoMap > 0.5) {
        albedo = texture(albedoMap, vUV).rgb * baseColorFactor.rgb;
    }

    vec3 N = normalize(vNormal);
    if (useNormalMap > 0.5) {
        N = perturbNormal(N, normalize(vCameraPos - vWorldPos), vUV, normalScale, vTangent);
    }

    roughness = clamp(roughness, 0.04, 1.0);
    vec3 V = normalize(vCameraPos - vWorldPos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // ----- Shading Modes Check -----
    if (light.shadingMode == 10) {
        outColor = vec4(albedo, 1.0);
        return;
    }
    if (light.shadingMode == 1 || shadingModel == 1) {
        vec3 color = albedo;
        color = color / (color + vec3(1.0));
        color = pow(color, vec3(1.0/2.2));
        outColor = vec4(color, 1.0);
        return;
    }
    if (light.shadingMode == 2) {
        float z = gl_FragCoord.z;
        float near = frame.projection[3][2] / frame.projection[2][2];
        float far = frame.projection[3][2] / (1.0 + frame.projection[2][2]);
        float linear = (near * far) / (far - z * (far - near));
        float maxDepthVis = min(far, 100.0);
        float d = clamp((linear - near) / (maxDepthVis - near), 0.0, 1.0);
        outColor = vec4(vec3(d), 1.0);
        return;
    }
    if (light.shadingMode == 3) {
        outColor = vec4(normalize(N) * 0.5 + 0.5, 1.0);
        return;
    }
    if (light.shadingMode == 4) {
        outColor = vec4(vec3(roughness), 1.0);
        return;
    }
    if (light.shadingMode == 5) {
        outColor = vec4(vec3(metallic), 1.0);
        return;
    }
    if (light.shadingMode == 6) {
        outColor = vec4(vec3(1.0), 1.0);
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
        outColor = vec4(hashColor, 1.0);
        return;
    }
    if (light.shadingMode == 8) {
        outColor = vec4(vec3(0.0), 1.0);
        return;
    }
    if (light.shadingMode == 13) {
        outColor = vec4(normalize(vTangent.xyz) * 0.5 + 0.5, 1.0);
        return;
    }
    if (light.shadingMode == 15) {
        outColor = vec4(vUV, 0.0, 1.0);
        return;
    }
    if (light.shadingMode == 16) {
        outColor = vec4(F0, 1.0);
        return;
    }

    vec3 totalDiffuse = vec3(0.0);
    vec3 totalSpecular = vec3(0.0);

    // ----- 1. Directional Light -----
    {
        vec3 L = normalize(-light.directionalDirectionIntensity.xyz);
        vec3 radiance = light.directionalColor.rgb * light.directionalDirectionIntensity.w;
        vec3 diffVal, specVal;
        EvaluateDirectBRDF(albedo, N, V, L, metallic, roughness, true, diffVal, specVal);
        float NdotL = max(dot(N, L), 0.0);
        totalDiffuse += diffVal * NdotL * radiance;
        totalSpecular += specVal * NdotL * radiance;
    }

    // ----- 2. Point Lights -----
    for (uint i = 0; i < light.pointLightCount && i < 16; ++i) {
        vec3 lightPos = light.pointPositionsRadius[i].xyz;
        float radius = light.pointPositionsRadius[i].w;
        vec3 lightColor = light.pointColorsIntensity[i].rgb;
        float intensity = light.pointColorsIntensity[i].w;

        vec3 diff = lightPos - vWorldPos;
        float distance = length(diff);
        if (distance > radius) continue;

        vec3 L = normalize(diff);
        float attenuation = clamp(1.0 - (distance / radius), 0.0, 1.0);
        attenuation = attenuation * attenuation;

        vec3 radiance = lightColor * intensity * attenuation;
        vec3 diffVal, specVal;
        EvaluateDirectBRDF(albedo, N, V, L, metallic, roughness, true, diffVal, specVal);
        float NdotL = max(dot(N, L), 0.0);
        totalDiffuse += diffVal * NdotL * radiance;
        totalSpecular += specVal * NdotL * radiance;
    }

    // Diagnostics Shading Modes
    if (light.shadingMode == 17) {
        vec3 color = totalDiffuse;
        color = color / (color + vec3(1.0));
        color = pow(color, vec3(1.0/2.2));
        outColor = vec4(color, 1.0);
        return;
    }
    if (light.shadingMode == 18) {
        vec3 color = totalSpecular;
        color = color / (color + vec3(1.0));
        color = pow(color, vec3(1.0/2.2));
        outColor = vec4(color, 1.0);
        return;
    }

    // Ambient term
    vec3 ambient = light.ambientColorIntensity.rgb * light.ambientColorIntensity.w * albedo;

    vec3 color = ambient + totalDiffuse + totalSpecular;

    // Tone-mapping and gamma correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));

    outColor = vec4(color, 1.0);
}
