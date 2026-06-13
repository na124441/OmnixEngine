#version 450
layout(location = 0) out vec4 outGBufferA; // Albedo + Material Flags
layout(location = 1) out vec4 outGBufferB; // Normal + Roughness
layout(location = 2) out vec4 outGBufferC; // Metallic + Ambient Occlusion + Entity ID
layout(location = 3) out vec4 outGBufferD; // Emissive + Shading Model

// Binding 0: Camera Uniform Buffer
layout(set = 0, binding = 0) uniform CameraBuffer {
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
    vec4 cameraPlanes;
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

// Textures remain at Set 1
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

// Cotangent frame formulation for normal perturbation
vec3 perturbNormal(vec3 N, vec3 V, vec2 uv, float normalScale)
{
    vec3 mapNormal = texture(normalMap, uv).xyz * 2.0 - 1.0;
    mapNormal.xy *= normalScale; // Scale normal perturbation

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
    uint blendMode = mat.materials[vMaterialIndex].blendMode;
    uint shadingModel = mat.materials[vMaterialIndex].shadingModel;

    vec4 albedo = baseColorFactor;
    if (hasAlbedoMap > 0.5) {
        albedo = texture(albedoMap, vUV) * baseColorFactor;
    }

    if (blendMode == 1 && albedo.a < 0.5) {
        discard;
    }

    vec3 N = normalize(vNormal);
    if (hasNormalMap > 0.5) {
        N = perturbNormal(N, normalize(vCameraPos - vWorldPos), vUV, normalScale);
    }

    float roughness = roughnessFactor;
    float metallic = metallicFactor;
    if (hasMetallicRoughnessMap > 0.5) {
        vec4 mrSample = texture(metallicRoughnessMap, vUV);
        // glTF: roughness = green channel, metallic = blue channel
        roughness *= mrSample.g;
        metallic *= mrSample.b;
    }

    float ao = 1.0;
    if (hasAOMap > 0.5) {
        ao = texture(aoMap, vUV).r;
    }

    vec3 emissive = vec3(0.0);
    if (hasEmissiveMap > 0.5) {
        emissive = texture(emissiveMap, vUV).rgb * emissiveStrength;
    }

    // Write GBuffer outputs
    outGBufferA = vec4(albedo.rgb, 0.0); // Material flags
    outGBufferB = vec4(N, roughness);
    outGBufferC = vec4(metallic, ao, float(vEntityID) / 255.0, 1.0); // Metallic, AO, Entity ID
    outGBufferD = vec4(emissive, float(shadingModel) / 255.0); // Emissive (RGB) + Shading Model (A)
}
