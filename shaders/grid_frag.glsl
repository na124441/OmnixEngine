#version 450
layout(location = 0) in vec3 nearPoint;
layout(location = 1) in vec3 farPoint;

layout(location = 0) out vec4 outColor;

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

vec4 grid(vec3 fragPos3D, float scale) {
    vec2 coord = fragPos3D.xz * scale;
    vec2 derivative = fwidth(coord);
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(grid.x, grid.y);
    float minimumz = min(derivative.y, 1.0);
    float minimumx = min(derivative.x, 1.0);
    vec4 color = vec4(0.35, 0.35, 0.35, 1.0 - min(line, 1.0));
    
    // highlight origin axes
    float threshold = 0.1 / scale;
    if (abs(fragPos3D.x) < threshold) {
        color.z = 1.0; // blue for Z axis
        color.x = 0.0;
        color.y = 0.0;
        color.w = 0.8;
    }
    if (abs(fragPos3D.z) < threshold) {
        color.x = 1.0; // red for X axis
        color.y = 0.0;
        color.z = 0.0;
        color.w = 0.8;
    }
    return color;
}

float computeDepth(vec3 pos) {
    vec4 clip_space_pos = frame.projection * frame.view * vec4(pos, 1.0);
    return (clip_space_pos.z / clip_space_pos.w);
}

void main() {
    float t = -nearPoint.y / (farPoint.y - nearPoint.y);
    if (t < 0.0) {
        discard;
    }
    vec3 fragPos3D = nearPoint + t * (farPoint - nearPoint);
    
    // Depth test output
    gl_FragDepth = computeDepth(fragPos3D);
    
    // Fade out at far plane or with distance
    float distance = length(fragPos3D.xz - frame.cameraPosition.xz);
    float maxDistance = 120.0;
    float fade = 1.0 - clamp(distance / maxDistance, 0.0, 1.0);
    fade = fade * fade; // smooth quadratic falloff
    
    // 1m grid
    vec4 color1 = grid(fragPos3D, 1.0);
    // 10m grid
    vec4 color10 = grid(fragPos3D, 0.1);
    
    vec4 color = color1 * 0.3 + color10 * 0.7;
    color.a *= fade;
    
    if (color.a < 0.02) {
        discard;
    }
    
    outColor = color;
}
