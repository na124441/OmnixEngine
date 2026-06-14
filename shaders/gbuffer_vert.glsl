#version 450
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

// Push Constant for the instance index
layout(push_constant) uniform PushConstants {
    uint instanceIndex;
} pc;

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
    vec4 minBounds_materialIndex; // xyz = min bounds, w = materialIndex
    vec4 maxBounds_entityID;      // xyz = max bounds, w = entityID
};

// Binding 1: Instance Storage Buffer
layout(std430, set = 0, binding = 1) readonly buffer InstanceBuffer {
    InstanceData instances[];
} inst;

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec2 vUV;
layout(location = 3) out vec3 vCameraPos;
layout(location = 4) flat out uint vMaterialIndex;
layout(location = 5) flat out uint vEntityID;

void main()
{
    uint idx = pc.instanceIndex;
    mat4 worldMatrix = inst.instances[idx].worldMatrix;

    vec4 worldPos = worldMatrix * vec4(inPos, 1.0);
    vWorldPos = worldPos.xyz;

    mat3 normalMat = transpose(inverse(mat3(worldMatrix)));
    vNormal = normalize(normalMat * inNormal);

    vUV = inUV;
    vCameraPos = frame.cameraPosition.xyz;
    vMaterialIndex = uint(inst.instances[idx].minBounds_materialIndex.w);
    vEntityID = uint(inst.instances[idx].maxBounds_entityID.w);

    gl_Position = frame.projection * frame.view * worldPos;
}
