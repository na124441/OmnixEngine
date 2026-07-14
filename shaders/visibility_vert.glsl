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

layout(location = 0) flat out uint vInstanceID;
layout(location = 1) flat out uint vClusterID;
layout(location = 2) flat out uint vPrimitiveID;

void main()
{
    uint rawInstanceIndex = pc.instanceIndex;
    uint instanceIndex = rawInstanceIndex;
    uint nodeIndex = 0xFFFFFFFF; // Conventional mesh fallback

    // Detect if we are drawing virtual geometry
    uint tempInstanceIndex = rawInstanceIndex & 0xFFF;
    InstanceData tempInst = inst.instances[tempInstanceIndex];
    if ((tempInst.flags & 4) != 0) {
        instanceIndex = rawInstanceIndex & 0xFFF;
        nodeIndex = rawInstanceIndex >> 12;
    }

    mat4 worldMatrix = inst.instances[instanceIndex].worldMatrix;
    vec4 worldPos = worldMatrix * vec4(inPos, 1.0);

    vInstanceID = instanceIndex;
    vClusterID = nodeIndex;
    vPrimitiveID = 0;

    gl_Position = frame.projection * frame.view * worldPos;
}
