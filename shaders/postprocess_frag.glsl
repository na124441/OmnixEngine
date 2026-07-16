#version 450
layout(location = 0) out vec4 outColor;
layout(location = 0) in vec2 inUV;

layout(set = 0, binding = 0) uniform sampler2D hdrColor;

layout(std430, set = 0, binding = 1) readonly buffer ExposureBuffer {
    float adaptedExposure;
    float prevExposure;
} exposure;

layout(set = 0, binding = 2) uniform sampler2D depthBuffer;

// Set 1 Bindings (Camera Frame UBO)
layout(set = 1, binding = 0) uniform RadianceFrame
{
    mat4 view;
    mat4 projection;
    mat4 inverseView;
    mat4 inverseProjection;
    mat4 inverseViewProjection;

    vec4 cameraPosition;
    vec4 viewportSize;
} frame;

layout(push_constant) uniform PushConstants {
    float exposure;
    float gamma;
    float bloomThreshold;
    float bloomIntensity;
    uint exposureMode;
    uint enableTonemapping;
    uint enableGammaCorrection;
    uint debugBeforePostProcess;
    float autoExposure;
    uint enableFog;
    float fogDensity;
    float fogHeightFalloff;

    // Color Grading
    float contrast;
    float saturation;
    float temp;
    float tint;

    vec3 lift;
    float pad0;

    vec3 gammaVal;
    float pad1;

    vec3 gain;
    float pad2;

    vec3 fogColor;
    float fogBaseHeight;
} pc;

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

// Uncharted 2 Filmic Operator
vec3 ToneMapFilmicCurve(vec3 x)
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 ToneMapFilmic(vec3 color)
{
    vec3 curr = ToneMapFilmicCurve(color * 1.6);
    vec3 whiteScale = 1.0 / ToneMapFilmicCurve(vec3(11.2));
    return clamp(curr * whiteScale, 0.0, 1.0);
}

vec3 ToneMapAgX(vec3 color)
{
    const mat3 srgb_to_agx = mat3(
        1.1968219355755106, -0.09802088114013627, -0.09902974574971842,
        -0.05881240182991702, 1.1514110345091724, -0.08983452445663673,
        -0.04597050393849557, -0.04780655474921477, 1.096735126857189
    );
    const mat3 agx_to_srgb = mat3(
        0.8424790622791138, 0.07841400802135468, 0.07922055572271347,
        0.04373268872499466, 0.8784686326980591, 0.07791625440120697,
        0.03853118419647217, 0.03923306614160538, 0.9221415519714355
    );

    color = srgb_to_agx * color;

    const float min_ev = -12.4;
    const float max_ev = 4.0;
    
    color = clamp(log2(color + 1e-5), min_ev, max_ev);
    color = (color - min_ev) / (max_ev - min_ev);

    vec3 x = color;
    vec3 curve = x * x * (3.0 - 2.0 * x); // Sigmoidal contrast curve

    curve = agx_to_srgb * curve;
    return clamp(curve, 0.0, 1.0);
}

vec3 ApplyWhiteBalance(vec3 c, float temp, float tint)
{
    c.r = c.r * (1.0 + temp * 0.12);
    c.b = c.b * (1.0 - temp * 0.12);
    c.g = c.g * (1.0 + tint * 0.08);
    return max(c, vec3(0.0));
}

vec3 ApplyColorGrading(vec3 color)
{
    // 1. White Balance
    color = ApplyWhiteBalance(color, pc.temp, pc.tint);

    // 2. Contrast
    color = (color - vec3(0.5)) * pc.contrast + vec3(0.5);
    color = max(color, vec3(0.0));

    // 3. Saturation
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luminance), color, pc.saturation);
    color = max(color, vec3(0.0));

    // 4. Lift, Gamma, Gain (Offset, Power, Slope)
    // Lift
    color = color * (vec3(1.0) - pc.lift) + pc.lift;
    // Gamma
    color = pow(max(color, vec3(0.0)), vec3(1.0) / max(pc.gammaVal, vec3(0.01)));
    // Gain
    color = color * pc.gain;

    return max(color, vec3(0.0));
}

vec3 ReconstructWorldPos(vec2 uv, float depth)
{
    vec4 clip = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 world = frame.inverseViewProjection * clip;
    return world.xyz / world.w;
}

vec3 ApplyFog(vec3 sceneColor, vec2 uv)
{
    if (pc.enableFog == 0u)
    {
        return sceneColor;
    }

    float depth = texture(depthBuffer, uv).r;
    // Skip fog for background/sky pixels
    if (depth >= 0.9999)
    {
        return sceneColor;
    }

    vec3 worldPos = ReconstructWorldPos(uv, depth);
    vec3 camPos = frame.cameraPosition.xyz;

    vec3 V = worldPos - camPos;
    float D = length(V);
    V = normalize(V);

    float C_y = camPos.y;
    float V_y = V.y;
    float H_0 = pc.fogBaseHeight;
    float b = pc.fogHeightFalloff;
    float d_0 = pc.fogDensity;

    // Integrate density along the ray
    float T = 0.0;
    if (abs(V_y) > 0.0001)
    {
        T = d_0 * ((1.0 - exp(-b * D * V_y)) / (b * V_y)) * exp(-b * (C_y - H_0));
    }
    else
    {
        T = d_0 * D * exp(-b * (C_y - H_0));
    }

    float f = exp(-T);
    return mix(pc.fogColor, sceneColor, clamp(f, 0.0, 1.0));
}

void main()
{
    vec3 hdrColorVal = max(texture(hdrColor, inUV).rgb, vec3(0.0));

    if (pc.debugBeforePostProcess == 1u)
    {
        outColor = vec4(hdrColorVal, 1.0);
        return;
    }

    // Determine Exposure value
    float expVal = pc.exposure;
    if (pc.exposureMode == 1u)
    {
        expVal = exposure.adaptedExposure;
    }

    vec3 color = hdrColorVal * expVal;

    // Apply analytic fog in linear HDR space
    color = ApplyFog(color, inUV);

    // 1. Tone Mapping
    if (pc.enableTonemapping > 0u)
    {
        if (pc.enableTonemapping == 1u)
        {
            if (pc.autoExposure == 1.0)
            {
                color = ToneMapReinhard(color);
            }
            else
            {
                if (pc.exposureMode == 0u)
                {
                    color = ToneMapACES(color);
                }
                else
                {
                    color = ToneMapFilmic(color);
                }
            }
        }
        else if (pc.enableTonemapping == 2u)
        {
            color = ToneMapACES(color);
        }
        else if (pc.enableTonemapping == 3u)
        {
            color = ToneMapFilmic(color);
        }
        else if (pc.enableTonemapping == 4u)
        {
            color = ToneMapAgX(color);
        }
        else if (pc.enableTonemapping == 5u)
        {
            color = ToneMapReinhard(color);
        }
    }

    // 2. Color Grading
    color = ApplyColorGrading(color);

    // 3. Gamma correction
    if (pc.enableGammaCorrection == 1u)
    {
        color = pow(color, vec3(1.0 / pc.gamma));
    }

    outColor = vec4(color, 1.0);
}
