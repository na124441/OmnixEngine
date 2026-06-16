#version 450
layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 inUV;

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

struct DirectionalLightData
{
    vec3 direction;
    float intensity;
    vec4 color;
};

// Binding 3: Light Storage Buffer
layout(std430, set = 0, binding = 3) readonly buffer LightBuffer {
    vec4 ambientColorIntensity; // rgb = color, w = intensity
    DirectionalLightData directional;
    vec4 pointPositionsRadius[16]; // xyz = pos, w = radius
    vec4 pointColorsIntensity[16]; // rgb = color, w = intensity
    uint pointLightCount;
    uint shadingMode;
    uint spotLightCount;
    uint paddingVal;
    vec4 spotPositionsRange[16];
    vec4 spotDirectionsIntensity[16];
    vec4 spotColors[16];
    vec4 spotAngles[16];
    
    // Shadow mapping settings
    mat4 directionalLightProjView;
    float shadowBias;
    float shadowNormalBias;
    float shadowSlopeBias;
    float shadowStrength;
    uint shadowLightCast;
    int pcfKernelSize;
    uint shadowResolution;
    uint paddingVal2;

    vec4 shadowParams;
    uvec4 shadowFlags;
} lighting;

#define light lighting

vec3 GetWorldViewDirection(vec2 uv)
{
    vec4 clip = vec4(uv * 2.0 - 1.0, 1.0, 1.0);

    vec4 view = frame.inverseProjection * clip;
    view /= view.w;

    vec3 viewDir = normalize(view.xyz);

    vec3 worldDir = normalize((frame.inverseView * vec4(viewDir, 0.0)).xyz);

    return worldDir;
}

vec3 EvaluateSky(vec3 worldDir)
{
    vec3 topColor = frame.skyTopColorIntensity.rgb;
    float skyIntensity = frame.skyTopColorIntensity.a;

    vec3 horizonColor = frame.skyHorizonColorBlend.rgb;
    float horizonBlend = frame.skyHorizonColorBlend.a;

    vec3 groundColor = frame.skyGroundColorIntensity.rgb;
    float groundIntensity = frame.skyGroundColorIntensity.a;

    float y = worldDir.y;

    if (y >= 0.0)
    {
        float t = clamp(y, 0.0, 1.0);
        t = pow(t, horizonBlend);

        vec3 sky = mix(horizonColor, topColor, t);
        return sky * skyIntensity;
    }
    else
    {
        float t = clamp(-y, 0.0, 1.0);
        vec3 ground = mix(horizonColor, groundColor, t);
        return ground * groundIntensity;
    }
}

// Set 1: GBuffer samplers
layout(set = 1, binding = 0) uniform sampler2D gbufferA; // Albedo + flags
layout(set = 1, binding = 1) uniform sampler2D gbufferB; // Normal + Roughness
layout(set = 1, binding = 2) uniform sampler2D gbufferC; // Metallic + AO + Entity ID
layout(set = 1, binding = 3) uniform sampler2D depthBuffer; // Depth
layout(set = 1, binding = 4) uniform sampler2D gbufferD; // Emissive + Shading Model
layout(set = 1, binding = 5) uniform sampler2D shadowMap; // Shadow Depth Map
layout(set = 1, binding = 6) uniform sampler2D ssaoMap; // SSAO Map

struct LocalLightGPU
{
    vec4 positionRange;
    vec4 colorIntensity;
    vec4 directionType;
    vec4 spotAngles;
};

struct ClusterBoundsGPU
{
    vec4 minPoint;
    vec4 maxPoint;
};

struct ClusterRangeGPU
{
    uint offset;
    uint count;
    uint overflow;
    uint pad;
};

layout(set = 3, binding = 0) readonly buffer LocalLights
{
    LocalLightGPU lights[];
} localLights;

layout(set = 3, binding = 1) readonly buffer ClusterBounds
{
    ClusterBoundsGPU bounds[];
} clusterBounds;

layout(set = 3, binding = 2) readonly buffer ClusterRanges
{
    ClusterRangeGPU ranges[];
} clusterRanges;

layout(set = 3, binding = 3) readonly buffer ClusterLightIndices
{
    uint lightIndices[];
} clusterLightIndices;

layout(set = 3, binding = 4) uniform ClusterSettings
{
    uint tileCountX;
    uint tileCountY;
    uint depthSliceCount;
    uint maxLightsPerCluster;

    uint clusterCount;
    uint lightCount;
    float nearPlane;
    float farPlane;
} settings;

layout(push_constant) uniform PushConstants
{
    uint localLightCount;
} pc;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;

    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    denom = 3.14159265 * denom * denom;

    return a2 / max(denom, 0.0001);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;

    return NdotV / max(NdotV * (1.0 - k) + k, 0.0001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float ggxV = GeometrySchlickGGX(max(dot(N, V), 0.0), roughness);
    float ggxL = GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);

    return ggxV * ggxL;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 EvaluateDirectionalLight(
    vec3 albedo,
    vec3 N,
    vec3 V,
    float metallic,
    float roughness)
{
    vec3 L = normalize(-light.directional.direction);
    vec3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.001);

    vec3 sunColor = light.directional.color.rgb;
    float sunIntensity = light.directional.intensity;

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = D * G * F;
    float denominator = max(4.0 * NdotV * NdotL, 0.001);
    vec3 specular = numerator / denominator;

    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / 3.14159265;

    return (diffuse + specular) * sunColor * sunIntensity * NdotL;
}

vec3 debugHeat(float value) {
    float t = clamp(value, 0.0, 1.0);
    vec3 cold = vec3(0.05, 0.20, 0.95);
    vec3 mid = vec3(0.05, 0.85, 0.25);
    vec3 hot = vec3(1.0, 0.18, 0.05);
    return mix(mix(cold, mid, smoothstep(0.0, 0.55, t)), hot, smoothstep(0.55, 1.0, t));
}

vec3 reconstructWorldPos(vec2 uv, float depth) {
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth, 1.0);
    mat4 invVP = inverse(frame.projection * frame.view);
    vec4 worldPos = invVP * ndc;
    return worldPos.xyz / worldPos.w;
}

float CalculateShadow(vec3 worldPos, vec3 N)
{
    if (lighting.shadowLightCast == 0)
    {
        return 0.0;
    }

    // Apply normal bias to prevent shadow acne (especially with PCF filtering)
    vec3 offsetWorldPos = worldPos + N * lighting.shadowNormalBias;
    vec4 lightSpace = lighting.directionalLightProjView * vec4(offsetWorldPos, 1.0);

    vec3 projCoords = lightSpace.xyz / lightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z > 1.0)
    {
        return 0.0;
    }

    vec3 L = normalize(-light.directional.direction);

    float constantBias = lighting.shadowParams.y;
    float slopeBias = lighting.shadowParams.z;

    float bias = constantBias + slopeBias * (1.0 - max(dot(N, L), 0.0));

    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);

    float shadow = 0.0;

    int kernelSize = int(lighting.shadowFlags.x);
    if (kernelSize <= 0) kernelSize = 3;
    int radius = (kernelSize - 1) / 2;
    if (radius < 0) radius = 0;

    float total = 0.0;

    for (int x = -radius; x <= radius; x++)
    {
        for (int y = -radius; y <= radius; y++)
        {
            float closestDepth = texture(
                shadowMap,
                projCoords.xy + vec2(x, y) * texelSize
            ).r;

            shadow += (projCoords.z - bias) > closestDepth ? 1.0 : 0.0;
            total += 1.0;
        }
    }

    shadow /= total;

    float strength = lighting.shadowParams.x;

    return shadow * strength;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float distributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return a2 / max(denom, 0.0001);
}

float geometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = geometrySchlickGGX(NdotV, roughness);
    float ggx2 = geometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 EvaluatePointLight(
    LocalLightGPU light,
    vec3 worldPos,
    vec3 albedo,
    vec3 N,
    vec3 V,
    float metallic,
    float roughness)
{
    vec3 lightPos = light.positionRange.xyz;
    float range = light.positionRange.w;

    vec3 Lvec = lightPos - worldPos;
    float distance = length(Lvec);

    if (distance >= range)
    {
        return vec3(0.0);
    }

    vec3 L = normalize(Lvec);
    vec3 H = normalize(V + L);

    float x = distance / range;
    float attenuation = clamp(1.0 - x * x, 0.0, 1.0);
    attenuation *= attenuation;

    vec3 color = light.colorIntensity.rgb;
    float intensity = light.colorIntensity.w;

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.001);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / 3.14159265;

    return (diffuse + specular) * color * intensity * attenuation * NdotL;
}

vec3 EvaluateSpotLight(
    LocalLightGPU light,
    vec3 worldPos,
    vec3 albedo,
    vec3 N,
    vec3 V,
    float metallic,
    float roughness)
{
    vec3 lightPos = light.positionRange.xyz;
    float range = light.positionRange.w;

    vec3 Lvec = lightPos - worldPos;
    float distance = length(Lvec);

    if (distance >= range)
    {
        return vec3(0.0);
    }

    vec3 L = normalize(Lvec);

    vec3 spotDirection = normalize(light.directionType.xyz);

    float theta = dot(normalize(-L), spotDirection);

    float innerAngle = light.spotAngles.x;
    float outerAngle = light.spotAngles.y;

    float inner = cos(innerAngle);
    float outer = cos(outerAngle);

    float cone = clamp((theta - outer) / max(inner - outer, 0.001), 0.0, 1.0);
    cone *= cone;

    if (cone <= 0.0)
    {
        return vec3(0.0);
    }

    float x = distance / range;
    float attenuation = clamp(1.0 - x * x, 0.0, 1.0);
    attenuation *= attenuation;

    // Reuse point light BRDF but multiply by cone.
    vec3 result = EvaluatePointLight(
        light,
        worldPos,
        albedo,
        N,
        V,
        metallic,
        roughness
    );

    return result * cone;
}

void main()
{
    // Sample GBuffer attributes
    vec4 gbufferASample = texture(gbufferA, inUV);
    vec4 gbufferBSample = texture(gbufferB, inUV);
    vec4 gbufferCSample = texture(gbufferC, inUV);
    vec4 gbufferDSample = texture(gbufferD, inUV);
    float depth = texture(depthBuffer, inUV).r;
    float exposure = frame.exposureSettings.x;



    // Background pixel: no geometry was rendered here.
    if (depth >= 0.9999) {
        vec3 worldDir = GetWorldViewDirection(inUV);
        vec3 sky = EvaluateSky(worldDir);
        outColor = vec4(sky, 1.0);
        return;
    }

    vec3 albedo = gbufferASample.rgb;
    vec3 N = gbufferBSample.rgb;
    float roughness = gbufferBSample.a;
    float metallic = gbufferCSample.r;
    float AO = gbufferCSample.g;
    float rawEntityID = gbufferCSample.b;
    vec3 emissive = gbufferDSample.rgb;

    // Mode 14: LightingOnly (overwrite albedo to neutral grey, emissive to black)
    if (light.shadingMode == 14) {
        albedo = vec3(0.5);
        emissive = vec3(0.0);
    }
    uint shadingModel = uint(round(gbufferDSample.a * 255.0));

    // ----- Shading Modes Check -----
    // Mode 10: Albedo Debug
    if (light.shadingMode == 10) {
        outColor = vec4(albedo, 0.0);
        return;
    }
    
    // Mode 1: Unlit (forces unlit debug view)
    if (light.shadingMode == 1) {
        vec3 color = albedo + emissive;
        color = color / (color + vec3(1.0));
        color = pow(color, vec3(1.0/2.2));
        outColor = vec4(color, 0.0);
        return;
    }

    // Mode 2: Depth Debug
    if (light.shadingMode == 2) {
        float near = frame.projection[3][2] / frame.projection[2][2];
        float far = frame.projection[3][2] / (1.0 + frame.projection[2][2]);
        float linear = (near * far) / (far - depth * (far - near));
        float maxDepthVis = min(far, 100.0);
        float d = clamp((linear - near) / (maxDepthVis - near), 0.0, 1.0);
        outColor = vec4(vec3(d), 0.0);
        return;
    }

    // Mode 3: Normal Debug
    if (light.shadingMode == 3) {
        vec3 normalColor = normalize(N) * 0.5 + 0.5;
        outColor = vec4(normalColor, 1.0);
        return;
    }

    // Mode 4: Roughness Debug
    if (light.shadingMode == 4) {
        outColor = vec4(vec3(roughness), 0.0);
        return;
    }

    // Mode 5: Metallic Debug
    if (light.shadingMode == 5) {
        outColor = vec4(vec3(metallic), 0.0);
        return;
    }

    // Mode 6: AO Debug
    if (light.shadingMode == 6) {
        float ssao = texture(ssaoMap, inUV).r;
        outColor = vec4(vec3(AO * ssao), 0.0);
        return;
    }

    // Mode 7: Entity ID Debug
    if (light.shadingMode == 7) {
        float id = rawEntityID;
        vec3 color = vec3(
            fract(sin(id * 12.9898) * 43758.5453),
            fract(sin(id * 78.233) * 43758.5453),
            fract(sin(id * 45.164) * 43758.5453)
        );
        if (id == 0.0) color = vec3(0.0);
        outColor = vec4(color, 0.0);
        return;
    }

    // Mode 8: Emissive Debug
    if (light.shadingMode == 8) {
        outColor = vec4(emissive, 0.0);
        return;
    }

    // Mode 9: Shadow Map Debug
    if (light.shadingMode == 9) {
        float shadowVal = texture(shadowMap, inUV).r;
        outColor = vec4(vec3(shadowVal), 1.0);
        return;
    }

    // Mode 11: Wireframe-style edge debug from depth/normal discontinuities.
    if (light.shadingMode == 11) {
        float depthEdge = smoothstep(0.0002, 0.0025, fwidth(depth));
        float normalEdge = smoothstep(0.05, 0.35, length(fwidth(N)));
        float edge = clamp(max(depthEdge, normalEdge), 0.0, 1.0);
        vec3 base = albedo * 0.35;
        outColor = vec4(mix(base, vec3(0.05, 0.95, 1.0), edge), 0.0);
        return;
    }

    // Mode 12: Light Complexity Debug
    if (light.shadingMode == 12) {
        vec3 vWorldPos = reconstructWorldPos(inUV, depth);
        uint affectingLights = 0;
        if (light.directional.intensity > 0.0) {
            affectingLights += 1;
        }
        for (uint i = 0; i < light.pointLightCount && i < 16; ++i) {
            float radius = light.pointPositionsRadius[i].w;
            if (distance(light.pointPositionsRadius[i].xyz, vWorldPos) <= radius) {
                affectingLights += 1;
            }
        }
        for (uint i = 0; i < light.spotLightCount && i < 16; ++i) {
            vec3 lightPos = light.spotPositionsRange[i].xyz;
            float range = light.spotPositionsRange[i].w;
            vec3 lightDir = normalize(light.spotDirectionsIntensity[i].xyz);
            vec3 toPixel = vWorldPos - lightPos;
            float distToPixel = length(toPixel);
            float cosOuter = light.spotAngles[i].x;
            if (distToPixel <= range && dot(normalize(toPixel), lightDir) >= cosOuter) {
                affectingLights += 1;
            }
        }
        outColor = vec4(debugHeat(float(affectingLights) / 8.0), 0.0);
        return;
    }

    // Mode 13: Tangent Debug
    if (light.shadingMode == 13) {
        vec3 vWorldPos = reconstructWorldPos(inUV, depth);
        vec3 dp1 = dFdx(vWorldPos);
        vec3 dp2 = dFdy(vWorldPos);
        vec2 duv1 = dFdx(inUV);
        vec2 duv2 = dFdy(inUV);
        float r = 1.0 / (duv1.x * duv2.y - duv1.y * duv2.x + 1e-6);
        vec3 T = (dp1 * duv2.y - dp2 * duv1.y) * r;
        T = normalize(T - dot(T, N) * N);
        vec3 tangentColor = T * 0.5 + 0.5;
        outColor = vec4(tangentColor, 0.0);
        return;
    }

    // Material Shading Model: Unlit
    if (shadingModel == 1) {
        vec3 color = albedo * exposure + emissive;
        outColor = vec4(color, 1.0);
        return;
    }

    // Reconstruct world space position
    vec3 vWorldPos = reconstructWorldPos(inUV, depth);

    // Standard PBR lighting calculation
    roughness = clamp(roughness, 0.04, 1.0);
    vec3 V = normalize(frame.cameraPosition.xyz - vWorldPos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);

    // ----- 1. Directional Light -----
        // Apply shadow factor
        float shadow = CalculateShadow(vWorldPos, N);

        vec3 sunLighting = EvaluateDirectionalLight(albedo, N, V, metallic, roughness);
        sunLighting *= (1.0 - shadow);
        Lo += sunLighting;

    // ----- 2. Clustered Local Lights -----
    vec3 localLighting = vec3(0.0);
    
    // View-space NDC position reconstruction for slice lookup
    vec4 clip = vec4(inUV * 2.0 - 1.0, depth, 1.0);
    vec4 viewPos = frame.inverseProjection * clip;
    viewPos /= viewPos.w;
    float linearDepth = -viewPos.z;

    // Calculate tile coordinate
    uint tileX = uint((gl_FragCoord.x / frame.viewportSize.x) * float(settings.tileCountX));
    uint tileY = uint((gl_FragCoord.y / frame.viewportSize.y) * float(settings.tileCountY));

    // Clamp tile coords
    tileX = clamp(tileX, 0u, settings.tileCountX - 1u);
    tileY = clamp(tileY, 0u, settings.tileCountY - 1u);

    // Calculate depth slice
    float depthT = (linearDepth - settings.nearPlane) / (settings.farPlane - settings.nearPlane);
    uint slice = uint(clamp(depthT * float(settings.depthSliceCount), 0.0, float(settings.depthSliceCount - 1)));

    // Cluster index
    uint clusterIdx = tileX + tileY * settings.tileCountX + slice * settings.tileCountX * settings.tileCountY;

    // Retrieve range for this cluster
    ClusterRangeGPU range = clusterRanges.ranges[clusterIdx];
    uint offset = range.offset;
    uint count = range.count;

    for (uint i = 0; i < count; i++)
    {
        uint lightIndex = clusterLightIndices.lightIndices[offset + i];
        LocalLightGPU light = localLights.lights[lightIndex];

        if (uint(light.directionType.w) == 0u)
        {
            localLighting += EvaluatePointLight(
                light,
                vWorldPos,
                albedo,
                N,
                V,
                metallic,
                roughness
            );
        }
        else if (uint(light.directionType.w) == 1u)
        {
            localLighting += EvaluateSpotLight(
                light,
                vWorldPos,
                albedo,
                N,
                V,
                metallic,
                roughness
            );
        }
    }
    Lo += localLighting;

    // Ambient sky light (multiplied by finalAO)
    float ssao = texture(ssaoMap, inUV).r;
    float finalAO = AO * ssao;
    vec3 ambient = light.ambientColorIntensity.rgb * light.ambientColorIntensity.w * albedo * finalAO;

    vec3 color = (ambient + Lo) * exposure + emissive;

    outColor = vec4(color, 1.0);
}
