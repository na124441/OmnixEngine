#version 450
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

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

layout(std430, set = 1, binding = 2) readonly buffer VisibleInstanceBuffer {
    uint visibleInstances[];
} visibleInst;

void main()
{
    uint visibleIndex = gl_InstanceIndex;
    uint idx = visibleInst.visibleInstances[visibleIndex];
    mat4 worldMatrix = inst.instances[idx].worldMatrix;
    gl_Position = frame.projection * frame.view * worldMatrix * vec4(inPos, 1.0);
}
