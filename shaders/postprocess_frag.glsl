#version 450
layout(location = 0) out vec4 outColor;
layout(location = 0) in vec2 inUV;
layout(set = 0, binding = 0) uniform sampler2D hdrColor;

layout(push_constant) uniform PostProcessSettings {
    float exposure;
    float gamma;
    float bloomThreshold;
    float bloomIntensity;
    uint exposureMode;
    uint enableTonemapping;
    uint enableGammaCorrection;
    uint debugBeforePostProcess;
    float autoExposure;
} settings;

vec3 acesTonemap(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 color = max(texture(hdrColor, inUV).rgb, vec3(0.0));

    if (settings.debugBeforePostProcess != 0u) {
        outColor = vec4(clamp(color, 0.0, 1.0), 1.0);
        return;
    }

    float activeExposure = settings.exposureMode == 1u ? settings.autoExposure : settings.exposure;
    color *= max(activeExposure, 0.0);

    vec3 bloomCandidate = max(color - vec3(settings.bloomThreshold), vec3(0.0));
    color += bloomCandidate * max(settings.bloomIntensity, 0.0);

    if (settings.enableTonemapping != 0u) {
        color = acesTonemap(color);
    } else {
        color = clamp(color, 0.0, 1.0);
    }

    if (settings.enableGammaCorrection != 0u) {
        color = pow(max(color, vec3(0.0)), vec3(1.0 / max(settings.gamma, 0.001)));
    }

    outColor = vec4(color, 1.0);
}
