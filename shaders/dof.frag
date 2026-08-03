#version 450
layout(location = 0) out vec4 outColor;
layout(location = 0) in vec2 inUV;

layout(set = 0, binding = 0) uniform sampler2D hdrColor;
layout(set = 0, binding = 1) uniform sampler2D depthBuffer;

layout(push_constant) uniform PushConstants {
    float focusDistance;
    float focalLength;
    float aperture;
    float cameraNear;
    float cameraFar;
    uint enabled;
} pc;

float LinearizeDepth(float d)
{
    float z = d * 2.0 - 1.0;
    return (2.0 * pc.cameraNear * pc.cameraFar) / (pc.cameraFar + pc.cameraNear - z * (pc.cameraFar - pc.cameraNear));
}

void main()
{
    vec4 baseColor = texture(hdrColor, inUV);
    if (pc.enabled == 0u) {
        outColor = baseColor;
        return;
    }

    float depth = texture(depthBuffer, inUV).r;
    if (depth >= 0.9999) {
        depth = 1.0;
    }

    float linearDepth = LinearizeDepth(depth);

    // Artist-friendly Circle of Confusion approximation
    float coc = clamp(abs(linearDepth - pc.focusDistance) * (pc.aperture / max(pc.focusDistance, 0.1)) * 0.1, 0.0, 1.0);

    if (coc < 0.005) {
        outColor = baseColor;
        return;
    }

    // Perform a variable radius gather blur
    vec4 blurColor = vec4(0.0);
    float totalWeight = 0.0;
    
    const vec2 samples[9] = {
        vec2(0.0, 0.0),
        vec2(-1.0, -1.0), vec2(0.0, -1.0), vec2(1.0, -1.0),
        vec2(-1.0, 0.0),                  vec2(1.0, 0.0),
        vec2(-1.0, 1.0),  vec2(0.0, 1.0),  vec2(1.0, 1.0)
    };

    float blurRadius = coc * 12.0; // Max blur radius in texels
    vec2 texelSize = 1.0 / textureSize(hdrColor, 0);

    for (int i = 0; i < 9; i++) {
        vec2 offset = samples[i] * blurRadius * texelSize;
        vec4 sampleColor = texture(hdrColor, inUV + offset);
        blurColor += sampleColor;
        totalWeight += 1.0;
    }

    outColor = blurColor / totalWeight;
}
