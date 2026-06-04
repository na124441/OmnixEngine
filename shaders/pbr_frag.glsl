#version 450
layout(location = 0) out vec4 outColor;

// Global camera UBO (set = 0, binding = 0) – not used directly in fragment
layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
} cam;

// Material set (set = 1)
layout(set = 1, binding = 0) uniform MaterialUBO {
    float roughness;
    float metallic;
    vec2  padding; // std140 padding
} mat;

layout(set = 1, binding = 1) uniform sampler2D albedoMap;
layout(set = 1, binding = 2) uniform sampler2D normalMap; // optional

// Lighting set (set = 2)
layout(set = 2, binding = 0) uniform LightUBO {
    vec4 direction; // xyz = direction, w = unused
    vec4 color;    // rgb = colour, w = intensity
    vec4 ambient;  // rgb = ambient colour, w = intensity
} light;

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vUV;

const float PI = 3.14159265359;

// ---------------------------------------------------------------------
// Helper functions (same as the ones in the analysis section)
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float distributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return a2 / denom;
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
    vec3 albedo = texture(albedoMap, vUV).rgb;

    // Normal map – if the texture is missing the shader will sample black (0,0,0)
    // convert from [0,1] to [-1,1]
    vec3 normalSample = texture(normalMap, vUV).rgb * 2.0 - 1.0;
    vec3 N = normalize(vNormal + normalSample); // simple normal mapping

    // Metallic & roughness are stored in the material UBO (could also be textures)
    float metallic  = mat.metallic;
    float roughness = clamp(mat.roughness, 0.04, 1.0); // avoid division by zero

    // ----- Lighting -----
    vec3 V = normalize(-vWorldPos); // camera at origin
    vec3 L = normalize(-light.direction.xyz);
    vec3 H = normalize(V + L);

    // Fresnel (Schlick)
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    // Distribution (GGX)
    float NDF = distributionGGX(N, H, roughness);

    // Geometry (Smith)
    float G = geometrySmith(N, V, L, roughness);

    // Specular term
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
    vec3 specular = numerator / denominator;

    // kS = specular reflectance, kD = (1 - kS) * (1 - metallic)
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    // Radiance from directional light
    float NdotL = max(dot(N, L), 0.0);
    vec3 radiance = light.color.rgb * light.color.w * NdotL;

    // Combine diffuse + specular
    vec3 Lo = (kD * albedo / PI + specular) * radiance;

    // Ambient term (from light UBO)
    vec3 ambient = light.ambient.rgb * light.ambient.w * albedo;

    vec3 color = ambient + Lo;

    // Tone‑mapping (Reinhard) and gamma correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));

    outColor = vec4(color, 1.0);
}
