#version 450
layout(location = 0) out vec4 outColor;

// Binding 0: Camera Uniform Buffer
layout(set = 0, binding = 0) uniform CameraBuffer {
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
    vec4 cameraPlanes; // x = near, y = far, z = exposure
} cam;

struct InstanceData {
    mat4 worldMatrix;
    mat4 previousWorldMatrix;
    vec4 minBounds_materialIndex;
    vec4 maxBounds_entityID;
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
    float hasNormalMap;
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
    uint spotLightCount;
    uint paddingVal;
    vec4 spotPositionsRange[16];
    vec4 spotDirectionsIntensity[16];
    vec4 spotColors[16];
    vec4 spotAngles[16];
    
    // Shadow mapping settings (not used by transparent shader)
    mat4 lightSpaceMatrix;
    float shadowBias;
    float shadowNormalBias;
    float shadowSlopeBias;
    float shadowStrength;
    uint shadowLightCast;
    int pcfKernelSize;
    uint shadowResolution;
    uint paddingVal2;
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

const float PI = 3.14159265359;

// Cotangent frame formulation for normal perturbation
vec3 perturbNormal(vec3 N, vec3 V, vec2 uv, float normalScale)
{
    vec3 mapNormal = texture(normalMap, uv).xyz * 2.0 - 1.0;
    mapNormal.xy *= normalScale;

    vec3 dp1 = dFdx(vWorldPos);
    vec3 dp2 = dFdy(vWorldPos);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);

    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    float invmax = inversesqrt(max(dot(T, T), dot(B, B)));
    return normalize(T * (mapNormal.x * invmax) + B * (mapNormal.y * invmax) + N * mapNormal.z);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float distributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return a2 / max(denom, 0.0001);
}

float geometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = geometrySchlickGGX(NdotV, roughness);
    float ggx2 = geometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
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
    float hasNormalMap = mat.materials[vMaterialIndex].hasNormalMap;
    float hasMetallicRoughnessMap = mat.materials[vMaterialIndex].hasMetallicRoughnessMap;
    float hasAOMap = mat.materials[vMaterialIndex].hasAOMap;
    float hasEmissiveMap = mat.materials[vMaterialIndex].hasEmissiveMap;
    uint shadingModel = mat.materials[vMaterialIndex].shadingModel;

    vec4 albedo = baseColorFactor;
    if (hasAlbedoMap > 0.5) {
        albedo = texture(albedoMap, vUV) * baseColorFactor;
    }

    vec3 N = normalize(vNormal);
    if (hasNormalMap > 0.5) {
        N = perturbNormal(N, normalize(vCameraPos - vWorldPos), vUV, normalScale);
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
    float exposure = cam.cameraPlanes.z;

    // Direct lighting computation (PBR Cook-Torrance)
    vec3 V = normalize(vCameraPos - vWorldPos);
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo.rgb, metallic);

    vec3 Lo = vec3(0.0);

    // 1. Directional Light
    if (light.directionalDirectionIntensity.w > 0.0) {
        vec3 L = normalize(light.directionalDirectionIntensity.xyz);
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);

        if (NdotL > 0.0) {
            vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
            float NDF = distributionGGX(N, H, roughness);
            float G = geometrySmith(N, V, L, roughness);

            vec3 numerator = NDF * G * F;
            float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.001;
            vec3 specular = numerator / denominator;

            vec3 kS = F;
            vec3 kD = vec3(1.0) - kS;
            kD *= 1.0 - metallic;

            vec3 radiance = light.directionalColor.rgb * light.directionalDirectionIntensity.w;
            Lo += (kD * albedo.rgb / PI + specular) * radiance * NdotL;
        }
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
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);

        float x = distance / radius;
        float attenuation = clamp(1.0 - x * x, 0.0, 1.0);
        attenuation = attenuation * attenuation;

        vec3 radiance = lightColor * intensity * attenuation;

        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        float NDF = distributionGGX(N, H, roughness);
        float G = geometrySmith(N, V, L, roughness);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.001;
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        Lo += (kD * albedo.rgb / PI + specular) * radiance * NdotL;
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

        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);

        float x = distance / range;
        float distAttenuation = clamp(1.0 - x * x, 0.0, 1.0);
        distAttenuation = distAttenuation * distAttenuation;

        vec3 radiance = lightColor * intensity * distAttenuation * cone;

        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        float NDF = distributionGGX(N, H, roughness);
        float G = geometrySmith(N, V, L, roughness);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.001;
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        Lo += (kD * albedo.rgb / PI + specular) * radiance * NdotL;
    }

    // Ambient lighting
    vec3 ambient = light.ambientColorIntensity.rgb * light.ambientColorIntensity.w * albedo.rgb * AO;

    // Final color with exposure (emissive added after exposure)
    vec3 color = (ambient + Lo) * exposure + emissive;

    outColor = vec4(color, albedo.a);
}
