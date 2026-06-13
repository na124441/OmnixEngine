#version 450
layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 inUV;

// Binding 0: Camera Uniform Buffer
layout(set = 0, binding = 0) uniform CameraBuffer {
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
    vec4 cameraPlanes;
} cam;

// Binding 3: Light Storage Buffer
layout(std430, set = 0, binding = 3) readonly buffer LightBuffer {
    vec4 ambientColorIntensity; // rgb = color, w = intensity
    vec4 directionalDirectionIntensity; // xyz = direction, w = intensity
    vec4 directionalColor; // rgb = color, w = unused
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
    mat4 lightSpaceMatrix;
    float shadowBias;
    float shadowNormalBias;
    float shadowStrength;
    uint shadowLightCast;
} light;

// Set 1: GBuffer samplers
layout(set = 1, binding = 0) uniform sampler2D gbufferA; // Albedo + flags
layout(set = 1, binding = 1) uniform sampler2D gbufferB; // Normal + Roughness
layout(set = 1, binding = 2) uniform sampler2D gbufferC; // Metallic + AO + Entity ID
layout(set = 1, binding = 3) uniform sampler2D depthBuffer; // Depth
layout(set = 1, binding = 4) uniform sampler2D gbufferD; // Emissive + Shading Model
layout(set = 1, binding = 5) uniform sampler2D shadowMap; // Shadow Depth Map

const float PI = 3.14159265359;

vec3 debugHeat(float value) {
    float t = clamp(value, 0.0, 1.0);
    vec3 cold = vec3(0.05, 0.20, 0.95);
    vec3 mid = vec3(0.05, 0.85, 0.25);
    vec3 hot = vec3(1.0, 0.18, 0.05);
    return mix(mix(cold, mid, smoothstep(0.0, 0.55, t)), hot, smoothstep(0.55, 1.0, t));
}

vec3 reconstructWorldPos(vec2 uv, float depth) {
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth, 1.0);
    mat4 invVP = inverse(cam.proj * cam.view);
    vec4 worldPos = invVP * ndc;
    return worldPos.xyz / worldPos.w;
}

float computeShadow(vec3 worldPos, vec3 normal) {
    if (light.shadowLightCast == 0) {
        return 1.0;
    }
    
    // Offset along normal to avoid shadow acne
    vec3 biasedWorldPos = worldPos + normal * light.shadowNormalBias;
    
    // Project to light space
    vec4 lightSpacePos = light.lightSpaceMatrix * vec4(biasedWorldPos, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    
    // Transform to [0,1] range for UV coordinates
    vec3 uvCoords = projCoords;
    uvCoords.xy = projCoords.xy * 0.5 + 0.5;
    
    // If outside the shadow map bounds, it is unshadowed
    if (uvCoords.x < 0.0 || uvCoords.x > 1.0 || uvCoords.y < 0.0 || uvCoords.y > 1.0 || uvCoords.z < 0.0 || uvCoords.z > 1.0) {
        return 1.0;
    }
    
    float currentDepth = uvCoords.z;
    float bias = light.shadowBias;
    
    // 3x3 PCF Filtering
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, uvCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (currentDepth - bias > pcfDepth) ? 0.0 : 1.0;
        }
    }
    shadow /= 9.0;
    
    return mix(1.0, shadow, light.shadowStrength);
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

void main()
{
    // Sample GBuffer attributes
    vec4 gbufferASample = texture(gbufferA, inUV);
    vec4 gbufferBSample = texture(gbufferB, inUV);
    vec4 gbufferCSample = texture(gbufferC, inUV);
    vec4 gbufferDSample = texture(gbufferD, inUV);
    float depth = texture(depthBuffer, inUV).r;
    float exposure = cam.cameraPlanes.z;

    // Selected object outline check
    uint selectedID = uint(round(cam.cameraPlanes.w));
    if (selectedID != 0) {
        uint centerID = uint(round(gbufferCSample.b * 255.0));
        vec2 texelSize = 1.0 / vec2(textureSize(gbufferC, 0));
        
        float leftID = texture(gbufferC, inUV + vec2(-texelSize.x, 0.0)).b;
        float rightID = texture(gbufferC, inUV + vec2(texelSize.x, 0.0)).b;
        float upID = texture(gbufferC, inUV + vec2(0.0, -texelSize.y)).b;
        float downID = texture(gbufferC, inUV + vec2(0.0, texelSize.y)).b;
        
        uint lID = uint(round(leftID * 255.0));
        uint rID = uint(round(rightID * 255.0));
        uint uID = uint(round(upID * 255.0));
        uint dID = uint(round(downID * 255.0));
        
        bool isEdge = false;
        if (centerID == selectedID) {
            if (lID != selectedID || rID != selectedID || uID != selectedID || dID != selectedID) {
                isEdge = true;
            }
        } else {
            if (lID == selectedID || rID == selectedID || uID == selectedID || dID == selectedID) {
                isEdge = true;
            }
        }
        
        if (isEdge) {
            outColor = vec4(1.0, 0.55, 0.0, 1.0); // Orange outline
            return;
        }
    }

    // Discard background pixels (where depth is 1.0)
    if (depth >= 1.0) {
        vec3 bgColor = light.ambientColorIntensity.rgb * light.ambientColorIntensity.w;
        if (light.shadingMode == 0) {
            bgColor *= exposure;
        }
        float bgAlpha = (light.shadingMode >= 1) ? 0.0 : 1.0;
        outColor = vec4(bgColor, bgAlpha);
        return;
    }

    vec3 albedo = gbufferASample.rgb;
    vec3 N = gbufferBSample.rgb;
    float roughness = gbufferBSample.a;
    float metallic = gbufferCSample.r;
    float AO = gbufferCSample.g;
    float rawEntityID = gbufferCSample.b;
    vec3 emissive = gbufferDSample.rgb;
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
        float near = cam.cameraPlanes.x;
        float far = cam.cameraPlanes.y;
        float linear = (near * far) / (far - depth * (far - near));
        float maxDepthVis = min(far, 100.0);
        float d = clamp((linear - near) / (maxDepthVis - near), 0.0, 1.0);
        outColor = vec4(vec3(d), 0.0);
        return;
    }

    // Mode 3: Normal Debug
    if (light.shadingMode == 3) {
        vec3 normalColor = N * 0.5 + 0.5;
        outColor = vec4(normalColor, 0.0);
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
        outColor = vec4(vec3(AO), 0.0);
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
        if (light.directionalDirectionIntensity.w > 0.0) {
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

    // Material Shading Model: Unlit
    if (shadingModel == 1) {
        vec3 color = (albedo + emissive) * exposure;
        outColor = vec4(color, 1.0);
        return;
    }

    // Reconstruct world space position
    vec3 vWorldPos = reconstructWorldPos(inUV, depth);

    // Standard PBR lighting calculation
    roughness = clamp(roughness, 0.04, 1.0);
    vec3 V = normalize(cam.cameraPos.xyz - vWorldPos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);

    // ----- 1. Directional Light -----
    {
        vec3 L = normalize(-light.directionalDirectionIntensity.xyz);
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);

        if (NdotL > 0.0) {
            vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
            float NDF = distributionGGX(N, H, roughness);
            float G = geometrySmith(N, V, L, roughness);

            vec3 numerator = NDF * G * F;
            float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.001;
            vec3 specular = numerator / denominator;

            vec3 kS = F;
            vec3 kD = vec3(1.0) - kS;
            kD *= 1.0 - metallic;

            // Apply shadow factor
            float shadowFactor = computeShadow(vWorldPos, N);

            vec3 radiance = light.directionalColor.rgb * light.directionalDirectionIntensity.w * shadowFactor;
            Lo += (kD * albedo / PI + specular) * radiance * NdotL;
        }
    }

    // ----- 2. Point Lights -----
    for (uint i = 0; i < light.pointLightCount && i < 16; ++i) {
        vec3 lightPos = light.pointPositionsRadius[i].xyz;
        float radius = light.pointPositionsRadius[i].w;
        vec3 lightColor = light.pointColorsIntensity[i].rgb;
        float intensity = light.pointColorsIntensity[i].w;

        vec3 diff = lightPos - vWorldPos;
        float distance = length(diff);
        if (distance > radius) continue;

        vec3 L = normalize(diff);
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);

        float attenuation = clamp(1.0 - (distance / radius), 0.0, 1.0);
        attenuation = attenuation * attenuation;

        vec3 radiance = lightColor * intensity * attenuation;

        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        float NDF = distributionGGX(N, H, roughness);
        float G = geometrySmith(N, V, L, roughness);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.001;
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    // ----- 3. Spot Lights -----
    for (uint i = 0; i < light.spotLightCount && i < 16; ++i) {
        vec3 lightPos = light.spotPositionsRange[i].xyz;
        float range = light.spotPositionsRange[i].w;
        vec3 lightDir = normalize(light.spotDirectionsIntensity[i].xyz);
        float intensity = light.spotDirectionsIntensity[i].w;
        vec3 lightColor = light.spotColors[i].rgb;
        float cosInner = light.spotColors[i].w;
        float cosOuter = light.spotAngles[i].x;

        vec3 diff = lightPos - vWorldPos;
        float distance = length(diff);
        if (distance > range) continue;

        vec3 L = normalize(diff);
        
        // Spot attenuation
        float theta = dot(-L, lightDir);
        float epsilon = cosInner - cosOuter;
        float spotAttenuation = clamp((theta - cosOuter) / max(epsilon, 0.0001), 0.0, 1.0);
        if (spotAttenuation <= 0.0) continue;

        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);

        float attenuation = clamp(1.0 - (distance / range), 0.0, 1.0);
        attenuation = attenuation * attenuation;

        vec3 radiance = lightColor * intensity * attenuation * spotAttenuation;

        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        float NDF = distributionGGX(N, H, roughness);
        float G = geometrySmith(N, V, L, roughness);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.001;
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    // Ambient sky light (multiplied by AO)
    vec3 ambient = light.ambientColorIntensity.rgb * light.ambientColorIntensity.w * albedo * AO;

    vec3 color = (ambient + Lo + emissive) * exposure;

    outColor = vec4(color, 1.0);
}
