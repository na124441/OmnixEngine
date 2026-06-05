#version 450
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

// Global camera UBO (set = 0, binding = 0)
layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    vec4 cameraPos; // xyz = world-space camera position
} cam;

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec2 vUV;
layout(location = 3) out vec3 vCameraPos;

void main()
{
    mat4 model = pc.model;
    vec4 worldPos = model * vec4(inPos, 1.0);
    vWorldPos = worldPos.xyz;

    // Normal matrix = inverse transpose of model (no scaling assumed)
    mat3 normalMat = transpose(inverse(mat3(model)));
    vNormal = normalize(normalMat * inNormal);

    vUV = inUV;
    vCameraPos = cam.cameraPos.xyz;
    gl_Position = cam.proj * cam.view * worldPos;
}
