#version 450

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
} ubo;

layout(push_constant) uniform Push {
    mat4 model;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;

void main() {
    // Phase 5: View and Projection from UBO, Model from Push Constant
    gl_Position = ubo.proj * ubo.view * pc.model * vec4(inPosition, 1.0);
    fragColor = inColor;
}
