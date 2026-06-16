#version 450
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(push_constant) uniform PushConstants {
    uint instanceIndex;
    uint pad0, pad1, pad2; // 12 bytes of padding to align mat4 to 16-byte boundary
    mat4 lightSpaceMatrix;
} pc;

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

void main()
{
    uint idx = pc.instanceIndex;
    mat4 worldMatrix = inst.instances[idx].worldMatrix;
    gl_Position = pc.lightSpaceMatrix * worldMatrix * vec4(inPos, 1.0);
}
