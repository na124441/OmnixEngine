#version 450
layout(location = 0) out vec4 outColor;

// Global camera UBO (set = 0, binding = 0)
layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    vec4 cameraPos; // xyz = world-space camera position
} cam;

// Material set (set = 1)
layout(set = 1, binding = 0) uniform MaterialUBO {
    vec4  albedoColor;
    float roughness;
    float metallic;
    float hasAlbedoMap;
    float hasNormalMap;
} mat;

layout(set = 1, binding = 1) uniform sampler2D albedoMap;
layout(set = 1, binding = 2) uniform sampler2D normalMap; // optional

// Lighting set (set = 2)
layout(set = 2, binding = 0) uniform LightUBO {
    vec4 ambientColorIntensity; // rgb = color, w = intensity
    vec4 directionalDirectionIntensity; // xyz = direction, w = intensity
    vec4 directionalColor; // rgb = color, w = unused
    vec4 pointPositionsRadius[16]; // xyz = pos, w = radius
    vec4 pointColorsIntensity[16]; // rgb = color, w = intensity
    uint pointLightCount;
    uint shadingMode;
    uint padding[2];
} light;

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vUV;
layout(location = 3) in vec3 vCameraPos;

const float PI = 3.14159265359;

// ---------------------------------------------------------------------
// Helper functions
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

// ---------------------------------------------------------------------
void main()
{
    // ----- Sample material textures -----
    vec3 albedo = mat.albedoColor.rgb;
    if (mat.hasAlbedoMap > 0.5) {
        albedo = texture(albedoMap, vUV).rgb * mat.albedoColor.rgb;
    }

    // Unlit View Mode Check
    if (light.shadingMode == 1) {
        // Tone-mapping (Reinhard) and gamma correction
        vec3 color = albedo;
        color = color / (color + vec3(1.0));
        color = pow(color, vec3(1.0/2.2));
        outColor = vec4(color, 1.0);
        return;
    }

    // Normal map – if the texture is missing, use geometry normal
    vec3 N = normalize(vNormal);
    if (mat.hasNormalMap > 0.5) {
        vec3 normalSample = texture(normalMap, vUV).rgb * 2.0 - 1.0;
        N = normalize(vNormal + normalSample);
    }

    // Metallic & roughness
    float metallic  = mat.metallic;
    float roughness = clamp(mat.roughness, 0.04, 1.0);

    // FIX: Use actual camera world-space position for view direction.
    // Previously this was normalize(-vWorldPos) which assumed camera at origin.
    vec3 V = normalize(vCameraPos - vWorldPos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);

    // ----- 1. Directional Light -----
    {
        vec3 L = normalize(-light.directionalDirectionIntensity.xyz);
        vec3 H = normalize(V + L);

        float NdotL = max(dot(N, L), 0.0);

        // Only compute PBR contribution if the surface faces the light
        if (NdotL > 0.0) {
            // Fresnel (Schlick)
            vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

            // Distribution (GGX)
            float NDF = distributionGGX(N, H, roughness);

            // Geometry (Smith)
            float G = geometrySmith(N, V, L, roughness);

            // Specular term
            vec3 numerator = NDF * G * F;
            float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.001;
            vec3 specular = numerator / denominator;

            // kS = specular reflectance, kD = (1 - kS) * (1 - metallic)
            vec3 kS = F;
            vec3 kD = vec3(1.0) - kS;
            kD *= 1.0 - metallic;

            // FIX: radiance does NOT include NdotL — it is applied in the final multiply.
            // Previously NdotL was baked into radiance AND multiplied again in Lo +=.
            vec3 radiance = light.directionalColor.rgb * light.directionalDirectionIntensity.w;

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
        if (distance > radius) continue; // Out of range

        vec3 L = normalize(diff);
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);

        // Attenuation based on distance and radius (quadratic fade)
        float attenuation = clamp(1.0 - (distance / radius), 0.0, 1.0);
        attenuation = attenuation * attenuation;

        vec3 radiance = lightColor * intensity * attenuation;

        // Fresnel (Schlick)
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        // Distribution (GGX)
        float NDF = distributionGGX(N, H, roughness);

        // Geometry (Smith)
        float G = geometrySmith(N, V, L, roughness);

        // Specular term
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.001;
        vec3 specular = numerator / denominator;

        // kS = specular reflectance, kD = (1 - kS) * (1 - metallic)
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    // Ambient term (from light UBO)
    vec3 ambient = light.ambientColorIntensity.rgb * light.ambientColorIntensity.w * albedo;

    vec3 color = ambient + Lo;

    // Tone-mapping (Reinhard) and gamma correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));

    outColor = vec4(color, 1.0);
}
