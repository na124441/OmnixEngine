#version 450
layout(location = 0) in vec2 inUV;
layout(location = 0) out float outColor;

layout(set = 0, binding = 0) uniform sampler2D ssaoRawTex;
layout(set = 0, binding = 1) uniform sampler2D depthTex;
layout(set = 0, binding = 2) uniform sampler2D normalTex;

void main() {
    float centerDepth = texture(depthTex, inUV).r;
    if (centerDepth >= 0.9999) {
        outColor = 1.0;
        return;
    }

    vec3 centerNormal = normalize(texture(normalTex, inUV).xyz);
    float centerSSAO = texture(ssaoRawTex, inUV).r;

    vec2 texelSize = 1.0 / textureSize(ssaoRawTex, 0);
    float totalSSAO = 0.0;
    float totalWeight = 0.0;

    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            vec2 uv = inUV + vec2(x, y) * texelSize;
            float sampleDepth = texture(depthTex, uv).r;
            vec3 sampleNormal = normalize(texture(normalTex, uv).xyz);
            float sampleSSAO = texture(ssaoRawTex, uv).r;

            // Bilateral weights based on depth and normal similarity
            float depthWeight = exp(-abs(centerDepth - sampleDepth) * 1500.0);
            float normalWeight = pow(max(dot(centerNormal, sampleNormal), 0.0), 16.0);
            float weight = depthWeight * normalWeight;

            totalSSAO += sampleSSAO * weight;
            totalWeight += weight;
        }
    }

    outColor = totalWeight > 0.0 ? (totalSSAO / totalWeight) : centerSSAO;
}
