#version 450
layout(location = 0) out vec4 outColor;
layout(location = 0) in vec2 inUV;

layout(set = 0, binding = 0) uniform sampler2D hdrColor;

layout(set = 1, binding = 0) uniform RadianceFrame
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

vec3 ToneMapReinhard(vec3 color)
{
    return color / (color + vec3(1.0));
}

vec3 ToneMapACES(vec3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;

    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 ApplyToneMapping(vec3 hdrColorVal)
{
    float exposure = frame.exposureSettings.x;
    uint mode = frame.renderFlags.x;

    vec3 color = hdrColorVal * exposure;

    if (mode == 1u)
    {
        color = ToneMapReinhard(color);
    }
    else if (mode == 2u)
    {
        color = ToneMapACES(color);
    }
    else
    {
        color = clamp(color, 0.0, 1.0);
    }

    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    return color;
}

void main()
{
    vec3 hdrColorVal = max(texture(hdrColor, inUV).rgb, vec3(0.0));

    vec3 ldrColor = ApplyToneMapping(hdrColorVal);

    outColor = vec4(ldrColor, 1.0);
}
