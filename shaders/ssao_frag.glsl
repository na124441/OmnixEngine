#version 450
layout(location = 0) in vec2 inUV;
layout(location = 0) out float outColor;

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

layout(set = 1, binding = 0) uniform sampler2D depthTex; // GBuffer Depth
layout(set = 1, binding = 1) uniform sampler2D normalTex; // GBuffer Normal
layout(set = 1, binding = 2) uniform sampler2D noiseTex; // 4x4 Noise

layout(set = 1, binding = 3) uniform SSAOConstants {
    vec4 samples[64];
    mat4 projection;
    float radius;
    float bias;
    float intensity;
    float screenWidth;
    float screenHeight;
    float enabled;
} ssao;

vec3 reconstructViewPos(vec2 uv, float depth) {
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 viewPos = inverse(ssao.projection) * ndc;
    return viewPos.xyz / viewPos.w;
}

void main() {
    if (ssao.enabled < 0.5) {
        outColor = 1.0;
        return;
    }

    float depth = texture(depthTex, inUV).r;
    if (depth >= 1.0) {
        outColor = 1.0;
        return;
    }

    vec3 viewPos = reconstructViewPos(inUV, depth);

    // Get normal in view space
    vec3 worldNormal = texture(normalTex, inUV).xyz * 2.0 - 1.0;
    vec3 viewNormal = normalize(mat3(frame.view) * worldNormal);

    // Get noise rotation vector
    vec2 noiseScale = vec2(ssao.screenWidth / 4.0, ssao.screenHeight / 4.0);
    vec3 randomVec = normalize(texture(noiseTex, inUV * noiseScale).xyz);

    // Reconstruct TBN
    vec3 tangent = normalize(randomVec - viewNormal * dot(randomVec, viewNormal));
    vec3 bitangent = cross(viewNormal, tangent);
    mat3 TBN = mat3(tangent, bitangent, viewNormal);

    // Calculate occlusion
    float occlusion = 0.0;
    for (int i = 0; i < 64; ++i) {
        // Offset along normal-oriented hemisphere
        vec3 samplePos = viewPos + TBN * ssao.samples[i].xyz * ssao.radius;

        // Project sample to screen
        vec4 offset = ssao.projection * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xy = offset.xy * 0.5 + 0.5;

        // Check if sample coordinate is off-screen
        if (offset.x < 0.0 || offset.x > 1.0 || offset.y < 0.0 || offset.y > 1.0) {
            continue;
        }

        // Sample depth
        float sampleDepth = texture(depthTex, offset.xy).r;
        vec3 sampleViewPos = reconstructViewPos(offset.xy, sampleDepth);

        // Range check to avoid dark halos around distant geometry
        float rangeCheck = smoothstep(0.0, 1.0, ssao.radius / abs(viewPos.z - sampleViewPos.z));
        if (sampleViewPos.z >= samplePos.z + ssao.bias) {
            occlusion += rangeCheck;
        }
    }

    occlusion = 1.0 - (occlusion / 64.0);
    outColor = pow(occlusion, ssao.intensity);
}
