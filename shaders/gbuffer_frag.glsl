#version 450
layout(location = 0) out vec4 outGBufferA; // Albedo + Material Flags
layout(location = 1) out vec4 outGBufferB; // Normal + Roughness
layout(location = 2) out vec4 outGBufferC; // Metallic + Ambient Occlusion
layout(location = 3) out vec4 outGBufferD; // Emissive + Shading Model
layout(location = 4) out uint outObjectID;  // ObjectID (32-bit uint)

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

// TBN frame formulation for normal perturbation
vec3 perturbNormal(vec3 N, vec3 V, vec2 uv, float normalScale)
{
    vec3 tangentNormal = texture(normalMap, uv).xyz * 2.0 - 1.0;
    tangentNormal.xy *= normalScale; // Scale normal perturbation

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
    if (useNormalMap > 0.5) {
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
    outGBufferC = vec4(metallic, ao, 0.0, 1.0); // Metallic, AO, 0.0 (Unused/Clean)
    outGBufferD = vec4(emissive, float(shadingModel) / 255.0); // Emissive (RGB) + Shading Model (A)
    outObjectID = vEntityID;
}
