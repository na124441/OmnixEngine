#version 450
layout(location = 0) out vec4 outGBufferA; // Albedo + Material Flags
layout(location = 1) out vec4 outGBufferB; // Normal + Roughness
layout(location = 2) out vec4 outGBufferC; // Metallic + Ambient Occlusion
layout(location = 3) out vec4 outGBufferD; // Emissive + Shading Model
layout(location = 4) out uint outObjectID;  // ObjectID (uint32)
layout(location = 5) out vec2 outVelocity;  // Velocity (vec2)

// GPUSceneBindings
#define GPUSCENE_BINDING_CAMERA 0
#define GPUSCENE_BINDING_INSTANCES 1
#define GPUSCENE_BINDING_MATERIALS 2
#define GPUSCENE_BINDING_LIGHTS 3

// Binding 0: Camera Uniform Buffer
layout(set = 0, binding = GPUSCENE_BINDING_CAMERA) uniform RadianceFrame
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
layout(std430, set = 0, binding = GPUSCENE_BINDING_INSTANCES) readonly buffer InstanceBuffer {
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
layout(std430, set = 0, binding = GPUSCENE_BINDING_MATERIALS) readonly buffer MaterialBuffer {
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
layout(location = 6) in vec3 vDebugColor;
layout(location = 7) in vec4 vTangent;
layout(location = 8) flat in uint vLayerMask;

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

float hash(uint x) {
    x = ((x >> 16) ^ x) * 0x45d9f3bu;
    x = ((x >> 16) ^ x) * 0x45d9f3bu;
    x = (x >> 16) ^ x;
    return float(x) / 4294967295.0;
}

vec3 randomColor(uint id) {
    float r = hash(id + 0u);
    float g = hash(id + 1337u);
    float b = hash(id + 80085u);
    return vec3(r, g, b) * 0.6 + 0.3;
}

void main()
{
    // Fetch Material Parameters
    vec4 baseColorFactor = mat.materials[vMaterialIndex].baseColorFactor;
    float roughnessFactor = mat.materials[vMaterialIndex].roughnessFactor;
    float metallicFactor = mat.materials[vMaterialIndex].metallicFactor;
    float normalScale = mat.materials[vMaterialIndex].normalScale;
    float emissiveStrength = mat.materials[vMaterialIndex].emissiveStrength;
    float clearcoatFactor = mat.materials[vMaterialIndex].clearcoatFactor;
    float clearcoatRoughness = mat.materials[vMaterialIndex].clearcoatRoughness;

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
        N = perturbNormal(N, normalize(vCameraPos - vWorldPos), vUV, normalScale, vTangent);
    }

    float roughness = roughnessFactor;
    float metallic = metallicFactor;
    if (hasMetallicRoughnessMap > 0.5) {
        vec4 mrSample = texture(metallicRoughnessMap, vUV);
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

    // Viewport shading mode overrides
    uint viewportMode = frame.renderFlags.y; 
    uint colorType = frame.renderFlags.z & 0xFFu;
    uint matcapPreset = (frame.renderFlags.z >> 8) & 0xFFu;

    if (viewportMode == 2u) { // Solid Mode
        shadingModel = 1u; // Set to Unlit so deferred pass outputs albedo directly
        emissive = vec3(0.0);
        clearcoatFactor = 0.0;

        if (colorType == 1u) { // Single Color
            albedo.rgb = vec3(0.8);
        } else if (colorType == 2u) { // Random Color
            albedo.rgb = randomColor(vEntityID);
        } else if (colorType == 3u) { // MatCap Mode
            vec3 N_view = normalize(mat3(frame.view) * N);
            
            if (matcapPreset == 0u) { // Clay
                vec3 L = normalize(vec3(1.0, 1.0, 1.0));
                float ndl = max(dot(N_view, L), 0.0);
                float rim = pow(1.0 - max(N_view.z, 0.0), 3.0);
                albedo.rgb = vec3(0.5, 0.25, 0.18) * (ndl * 0.8 + 0.2) + vec3(0.2) * rim;
            } else if (matcapPreset == 1u) { // Red Wax
                vec3 L = normalize(vec3(0.5, 0.75, 1.0));
                vec3 H = normalize(L + vec3(0.0, 0.0, 1.0));
                float ndl = max(dot(N_view, L), 0.0);
                float ndh = max(dot(N_view, H), 0.0);
                float spec = pow(ndh, 32.0);
                float rim = pow(1.0 - max(N_view.z, 0.0), 4.0);
                albedo.rgb = vec3(0.7, 0.05, 0.05) * (ndl * 0.7 + 0.3) + vec3(1.0) * spec * 0.6 + vec3(0.4, 0.1, 0.1) * rim;
            } else if (matcapPreset == 2u) { // Zebra
                vec3 R = reflect(vec3(0.0, 0.0, -1.0), N_view);
                float stripe = step(0.0, sin(R.y * 30.0));
                albedo.rgb = vec3(mix(0.05, 0.95, stripe));
            } else { // NormalMap
                albedo.rgb = N_view * 0.5 + 0.5;
            }
        }
    }

    // G8: Check and apply debug coloring for virtual geometry modes
    if ((frame.renderFlags.w & 0xFF) != 0) {
        albedo.rgb = vDebugColor;
        emissive = vec3(0.0);
    }

    outGBufferA = vec4(albedo.rgb, clearcoatFactor);
    outGBufferB = vec4(N, roughness);
    outGBufferC = vec4(metallic, ao, float(vLayerMask) / 255.0, clearcoatRoughness); // Metallic, AO, LayerMask, clearcoatRoughness
    outGBufferD = vec4(emissive, float(shadingModel) / 255.0); // Emissive (RGB) + Shading Model (A)
    outObjectID = vEntityID;
    outVelocity = vec2(0.0);
}
