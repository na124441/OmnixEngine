#version 450

layout(location = 0) out vec4 outGBufferA; // Resolved GBuffer A (Albedo + flags)
layout(location = 1) out vec4 outGBufferB; // Resolved GBuffer B (Normal + roughness)
layout(location = 2) out vec4 outGBufferC; // Resolved GBuffer C (Metallic + AO)
layout(location = 3) out vec4 outGBufferD; // Resolved GBuffer D (Emissive + shading model)

layout(location = 0) in vec2 inUV;

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

layout(std430, set = 0, binding = 1) readonly buffer InstanceBuffer {
    InstanceData instances[];
} inst;

struct GPURVGCluster {
    vec4 boundsSphere;
    vec4 coneAxisCutoff;
    uint pageIndex;
    uint vertexOffset;
    uint indexOffset;
    uint vertexCount;
    uint indexCount;
    uint padding[3];
};

layout(std430, set = 0, binding = 10) readonly buffer RVGClusterBuffer {
    GPURVGCluster clusters[];
} clusterBuf;

struct GPUMeshRecord {
    uint firstIndex;
    int vertexOffset;
    uint indexCount;
    uint vertexCount;
    vec4 localBoundsSphere;
    uint materialSlotOffset;
    uint submeshOffset;
    uint submeshCount;
    uint flags;
};

layout(std430, set = 0, binding = 6) readonly buffer MeshTable {
    GPUMeshRecord meshes[];
} meshTable;

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

layout(std430, set = 0, binding = 2) readonly buffer MaterialBuffer {
    MaterialData materials[];
} mat;

// Push Constants for target material selection
layout(push_constant) uniform PushConstants {
    uint targetMaterialIndex;
} pcs;

// Visibility buffer inputs (set 2, binding 0-2)
layout(set = 2, binding = 0) uniform usampler2D visInstanceTex;
layout(set = 2, binding = 1) uniform usampler2D visClusterTex;
layout(set = 2, binding = 2) uniform usampler2D visPrimitiveTex;

struct PbrVertex {
    vec3 pos;
    vec3 normal;
    vec2 uv;
    vec4 tangent;
};

// Global geometry storage buffers (set 2, binding 3-4)
layout(std430, set = 2, binding = 3) readonly buffer GlobalVertexBuffer {
    PbrVertex vertices[];
} globalVB;

layout(std430, set = 2, binding = 4) readonly buffer GlobalIndexBuffer {
    uint indices[];
} globalIB;

// Material textures bound on set 1
layout(set = 1, binding = 1) uniform sampler2D albedoMap;
layout(set = 1, binding = 2) uniform sampler2D normalMap;
layout(set = 1, binding = 3) uniform sampler2D metallicRoughnessMap;
layout(set = 1, binding = 4) uniform sampler2D aoMap;
layout(set = 1, binding = 5) uniform sampler2D emissiveMap;

void main()
{
    ivec2 texCoord = ivec2(gl_FragCoord.xy);
    uint instanceID = texelFetch(visInstanceTex, texCoord, 0).r;
    uint clusterID = texelFetch(visClusterTex, texCoord, 0).r;
    uint primitiveID = texelFetch(visPrimitiveTex, texCoord, 0).r;

    // Background check
    if (instanceID == 0xFFFFFFFF) {
        discard;
    }

    InstanceData instance = inst.instances[instanceID];
    uint matIndex = instance.materialIndex;

    // Filter pixels by active material index (Reference Path)
    if (matIndex != pcs.targetMaterialIndex) {
        discard;
    }

    mat4 worldMatrix = instance.worldMatrix;

    uint idx0, idx1, idx2;
    uint vertOffset = 0;

    if (clusterID == 0xFFFFFFFF) {
        // Conventional Mesh
        GPUMeshRecord mesh = meshTable.meshes[instance.meshIndex];
        idx0 = globalIB.indices[mesh.firstIndex + primitiveID * 3 + 0];
        idx1 = globalIB.indices[mesh.firstIndex + primitiveID * 3 + 1];
        idx2 = globalIB.indices[mesh.firstIndex + primitiveID * 3 + 2];
        vertOffset = uint(mesh.vertexOffset);
    } else {
        // Virtual Geometry Cluster
        GPURVGCluster cluster = clusterBuf.clusters[clusterID];
        idx0 = globalIB.indices[cluster.indexOffset + primitiveID * 3 + 0];
        idx1 = globalIB.indices[cluster.indexOffset + primitiveID * 3 + 1];
        idx2 = globalIB.indices[cluster.indexOffset + primitiveID * 3 + 2];
        vertOffset = cluster.vertexOffset;
    }

    PbrVertex v0 = globalVB.vertices[vertOffset + idx0];
    PbrVertex v1 = globalVB.vertices[vertOffset + idx1];
    PbrVertex v2 = globalVB.vertices[vertOffset + idx2];

    // Calculate clip space coordinates
    vec4 c0 = frame.projection * frame.view * worldMatrix * vec4(v0.pos, 1.0);
    vec4 c1 = frame.projection * frame.view * worldMatrix * vec4(v1.pos, 1.0);
    vec4 c2 = frame.projection * frame.view * worldMatrix * vec4(v2.pos, 1.0);

    // Screen space coordinates (pixels)
    vec2 p0 = ((c0.xy / c0.w) * 0.5 + 0.5) * frame.viewportSize.xy;
    vec2 p1 = ((c1.xy / c1.w) * 0.5 + 0.5) * frame.viewportSize.xy;
    vec2 p2 = ((c2.xy / c2.w) * 0.5 + 0.5) * frame.viewportSize.xy;

    // Compute barycentric coordinates
    vec2 p = gl_FragCoord.xy;
    float area = (p1.y - p2.y) * (p0.x - p2.x) + (p2.x - p1.x) * (p0.y - p2.y);
    if (abs(area) < 1e-6) discard;

    float w0 = ((p1.y - p2.y) * (p.x - p2.x) + (p2.x - p1.x) * (p.y - p2.y)) / area;
    float w1 = ((p2.y - p0.y) * (p.x - p2.x) + (p0.x - p2.x) * (p.y - p2.y)) / area;
    float w2 = 1.0 - w0 - w1;

    // Perspective-correct interpolation weights
    float invW0 = 1.0 / max(c0.w, 1e-6);
    float invW1 = 1.0 / max(c1.w, 1e-6);
    float invW2 = 1.0 / max(c2.w, 1e-6);
    float denom = w0 * invW0 + w1 * invW1 + w2 * invW2;
    denom = max(denom, 1e-6);

    vec3 b = vec3(w0 * invW0, w1 * invW1, w2 * invW2) / denom;

    // Reconstruct attributes
    vec3 localPos = v0.pos * b.x + v1.pos * b.y + v2.pos * b.z;
    vec3 worldPos = (worldMatrix * vec4(localPos, 1.0)).xyz;
    vec2 uv = v0.uv * b.x + v1.uv * b.y + v2.uv * b.z;
    vec3 localNormal = normalize(v0.normal * b.x + v1.normal * b.y + v2.normal * b.z);
    
    // Normal transform correctness accounting for non-uniform and negative scale
    mat3 normalMat = transpose(inverse(mat3(worldMatrix)));
    vec3 normal = normalize(normalMat * localNormal);

    // Reconstruct tangent frame (TBN)
    vec4 localTangent = v0.tangent * b.x + v1.tangent * b.y + v2.tangent * b.z;
    vec3 tangent = normalize(normalMat * localTangent.xyz);
    vec3 bitangent = cross(normal, tangent) * localTangent.w;

    MaterialData material = mat.materials[matIndex];

    // Evaluate maps using Set 1 descriptors
    vec4 albedo = material.baseColorFactor;
    if (material.hasAlbedoMap > 0.5) {
        albedo *= texture(albedoMap, uv);
    }

    vec3 N = normal;
    if (material.useNormalMap > 0.5) {
        vec3 tangentNormal = texture(normalMap, uv).xyz * 2.0 - 1.0;
        tangentNormal.xy *= material.normalScale;
        mat3 TBN = mat3(tangent, bitangent, normal);
        N = normalize(TBN * tangentNormal);
    }

    float metallic = material.metallicFactor;
    float roughness = material.roughnessFactor;
    if (material.hasMetallicRoughnessMap > 0.5) {
        vec4 mrSample = texture(metallicRoughnessMap, uv);
        metallic *= mrSample.b;
        roughness *= mrSample.g;
    }

    float ao = 1.0;
    if (material.hasAOMap > 0.5) {
        ao = texture(aoMap, uv).r;
    }

    vec3 emissive = vec3(0.0);
    if (material.hasEmissiveMap > 0.5) {
        emissive = texture(emissiveMap, uv).rgb * material.emissiveStrength;
    }

    // Populate GBuffer attachments
    outGBufferA = vec4(albedo.rgb, float(instanceID) / 4095.0);
    outGBufferB = vec4(N * 0.5 + 0.5, roughness);
    outGBufferC = vec4(metallic, ao, float(instance.objectID & 0xFF) / 255.0, 1.0);
    outGBufferD = vec4(emissive, float(material.shadingModel) / 255.0);
}
