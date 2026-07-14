#ifndef BRDF_GLSL
#define BRDF_GLSL

const float PI = 3.14159265359;

// GGX Normal Distribution Function
float distributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return a2 / max(denom, 0.00001);
}

// Smith height-uncorrelated geometry visibility
float geometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / max(NdotV * (1.0 - k) + k, 0.00001);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = geometrySchlickGGX(NdotV, roughness);
    float ggx2 = geometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

// Fresnel Schlick approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Burley (Disney) diffuse implementation
float burleyDiffuse(float NdotL, float NdotV, float LdotH, float roughness)
{
    float FD90 = 0.5 + 2.0 * roughness * LdotH * LdotH;
    float F_L = 1.0 + (FD90 - 1.0) * pow(clamp(1.0 - NdotL, 0.0, 1.0), 5.0);
    float F_V = 1.0 + (FD90 - 1.0) * pow(clamp(1.0 - NdotV, 0.0, 1.0), 5.0);
    return (F_L * F_V) / PI;
}

// Numerically safe PBR Evaluation helper
vec3 EvaluateDirectBRDF(
    vec3 albedo,
    vec3 N,
    vec3 V,
    vec3 L,
    float metallic,
    float roughness,
    bool useBurley,
    out vec3 diffuseColor,
    out vec3 specularColor)
{
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0001);
    float NdotH = max(dot(N, H), 0.0);
    float LdotH = max(dot(L, H), 0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = fresnelSchlick(LdotH, F0);

    float D = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);

    vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.0001);
    
    // Protect against NaN/Inf in specular evaluation
    if (isnan(specular.x) || isinf(specular.x) || isnan(specular.y) || isinf(specular.y) || isnan(specular.z) || isinf(specular.z)) {
        specular = vec3(0.0);
    }

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 diffuse = vec3(0.0);
    if (useBurley) {
        diffuse = kD * albedo * burleyDiffuse(NdotL, NdotV, LdotH, roughness);
    } else {
        diffuse = kD * albedo / PI;
    }

    // Protect against NaN/Inf in diffuse evaluation
    if (isnan(diffuse.x) || isinf(diffuse.x) || isnan(diffuse.y) || isinf(diffuse.y) || isnan(diffuse.z) || isinf(diffuse.z)) {
        diffuse = vec3(0.0);
    }

    diffuseColor = diffuse;
    specularColor = specular;

    return (diffuse + specular) * NdotL;
}

#endif // BRDF_GLSL
