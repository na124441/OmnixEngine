#version 450
layout(location = 0) out vec4 outColor;

// Binding 0: Camera Uniform Buffer
layout(set = 0, binding = 0) uniform RadianceFrame
{
    mat4 view;
    mat4 projection;
    mat4 inverseView;
    mat4 inverseProjection;

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
    vec4 minBounds_materialIndex;
    vec4 maxBounds_entityID;
};

// Binding 1: Instance Storage Buffer
layout(std430, set = 0, binding = 1) readonly buffer InstanceBuffer {
    InstanceData instances[];
} inst;

struct MaterialData {
    vec4 albedoColor;
    float roughness;
    float metallic;
    float hasAlbedoMap;
    float hasNormalMap;
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

const float PI = 3.14159265359;

// Helper functions
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
    // ----- Fetch Material Parameters -----
    vec4 albedoColor = mat.materials[vMaterialIndex].albedoColor;
    float roughness = mat.materials[vMaterialIndex].roughness;
    float metallic = mat.materials[vMaterialIndex].metallic;
    float hasAlbedoMap = mat.materials[vMaterialIndex].hasAlbedoMap;
    float hasNormalMap = mat.materials[vMaterialIndex].hasNormalMap;

    vec3 albedo = albedoColor.rgb;
    if (hasAlbedoMap > 0.5) {
        albedo = texture(albedoMap, vUV).rgb * albedoColor.rgb;
    }

    // Unlit View Mode Check
    if (light.shadingMode == 1) {
        vec3 color = albedo;
        color = color / (color + vec3(1.0));
        color = pow(color, vec3(1.0/2.2));
        outColor = vec4(color, 1.0);
        return;
    }

    // Depth Debug View Mode Check
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

    vec3 N = normalize(vNormal);
    if (hasNormalMap > 0.5) {
        vec3 normalSample = texture(normalMap, vUV).rgb * 2.0 - 1.0;
        N = normalize(vNormal + normalSample);
    }

    roughness = clamp(roughness, 0.04, 1.0);

    vec3 V = normalize(vCameraPos - vWorldPos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);

    // ----- 1. Directional Light -----
    {
        vec3 L = normalize(-light.directionalDirectionIntensity.xyz);
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

            Lo += (kD * albedo / PI + specular) * radiance * NdotL;
        }
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
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);

        float attenuation = clamp(1.0 - (distance / radius), 0.0, 1.0);
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

        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    // Ambient term
    vec3 ambient = light.ambientColorIntensity.rgb * light.ambientColorIntensity.w * albedo;

    vec3 color = ambient + Lo;

    // Tone-mapping and gamma correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));

    outColor = vec4(color, 1.0);
}
