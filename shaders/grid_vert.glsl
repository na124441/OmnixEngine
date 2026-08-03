#version 450
layout(location = 0) out vec3 nearPoint;
layout(location = 1) out vec3 farPoint;

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

// Fullscreen quad vertices in NDC space
vec3 gridPlane[6] = vec3[](
    vec3(-1, -1, 0), vec3( 1, -1, 0), vec3(-1,  1, 0),
    vec3(-1,  1, 0), vec3( 1, -1, 0), vec3( 1,  1, 0)
);

vec3 UnprojectPoint(float x, float y, float z, mat4 view, mat4 proj) {
    mat4 viewProjInverse = inverse(proj * view);
    vec4 unprojectedPoint = viewProjInverse * vec4(x, y, z, 1.0);
    return unprojectedPoint.xyz / unprojectedPoint.w;
}

void main() {
    vec3 p = gridPlane[gl_VertexIndex];
    nearPoint = UnprojectPoint(p.x, p.y, 0.0, frame.view, frame.projection); // Near plane point
    farPoint = UnprojectPoint(p.x, p.y, 1.0, frame.view, frame.projection);   // Far plane point
    gl_Position = vec4(p, 1.0); // Fullscreen quad
}
