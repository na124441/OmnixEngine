#include "Core/pch.h"
#include "stb/stb_image.h"
#include "EnvironmentSystem.h"
#include "Core/Engine/EngineResources.h"
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace eng::renderer {

// Helper: map a direction to equirectangular UV
static glm::vec2 getEquirectangularUV(const glm::vec3& d) {
    float phi = std::atan2(d.z, d.x);
    float theta = std::asin(std::clamp(d.y, -1.0f, 1.0f));
    
    float u = 0.5f + phi / (2.0f * glm::pi<float>());
    float v = 0.5f - theta / glm::pi<float>();
    return glm::vec2(u, v);
}

// Helper: bilinear sample of equirectangular float image
static glm::vec4 sampleEquirectangular(const float* pixels, int w, int h, const glm::vec2& uv) {
    float x = uv.x * (w - 1);
    float y = uv.y * (h - 1);
    
    int x0 = std::clamp(static_cast<int>(std::floor(x)), 0, w - 1);
    int y0 = std::clamp(static_cast<int>(std::floor(y)), 0, h - 1);
    int x1 = std::clamp(x0 + 1, 0, w - 1);
    int y1 = std::clamp(y0 + 1, 0, h - 1);
    
    float tx = x - x0;
    float ty = y - y0;
    
    auto getPixel = [&](int px, int py) {
        int idx = (px + py * w) * 4;
        return glm::vec4(pixels[idx], pixels[idx+1], pixels[idx+2], pixels[idx+3]);
    };
    
    glm::vec4 c00 = getPixel(x0, y0);
    glm::vec4 c10 = getPixel(x1, y0);
    glm::vec4 c01 = getPixel(x0, y1);
    glm::vec4 c11 = getPixel(x1, y1);
    
    return glm::mix(glm::mix(c00, c10, tx), glm::mix(c01, c11, tx), ty);
}

// Helper: cubemap coordinate mapping
static glm::vec3 getDirectionForFaceTexel(int face, float u, float v) {
    switch (face) {
        case 0: return glm::normalize(glm::vec3(1.0f, -v, -u));   // +X
        case 1: return glm::normalize(glm::vec3(-1.0f, -v, u));   // -X
        case 2: return glm::normalize(glm::vec3(u, 1.0f, v));     // +Y
        case 3: return glm::normalize(glm::vec3(u, -1.0f, -v));   // -Y
        case 4: return glm::normalize(glm::vec3(u, -v, 1.0f));    // +Z
        case 5: return glm::normalize(glm::vec3(-u, -v, -1.0f));  // -Z
    }
    return glm::vec3(0.0f);
}

// Helper: sample cubemap linearly
static glm::vec4 sampleCubemapLinear(const std::vector<std::vector<float>>& cubemap, uint32_t size, const glm::vec3& d) {
    glm::vec3 ad = glm::abs(d);
    int face = 0;
    float ma = 0.0f;
    float u = 0.0f;
    float v = 0.0f;

    if (ad.x >= ad.y && ad.x >= ad.z) {
        face = d.x >= 0.0f ? 0 : 1;
        ma = ad.x;
        u = d.x >= 0.0f ? -d.z : d.z;
        v = -d.y;
    } else if (ad.y >= ad.x && ad.y >= ad.z) {
        face = d.y >= 0.0f ? 2 : 3;
        ma = ad.y;
        u = d.x;
        v = d.y >= 0.0f ? d.z : -d.z;
    } else {
        face = d.z >= 0.0f ? 4 : 5;
        ma = ad.z;
        u = d.z >= 0.0f ? d.x : -d.x;
        v = -d.y;
    }

    u = 0.5f * (u / ma + 1.0f);
    v = 0.5f * (v / ma + 1.0f);

    float fx = u * (size - 1);
    float fy = v * (size - 1);
    int x0 = std::clamp(static_cast<int>(std::floor(fx)), 0, static_cast<int>(size - 1));
    int y0 = std::clamp(static_cast<int>(std::floor(fy)), 0, static_cast<int>(size - 1));
    int x1 = std::clamp(x0 + 1, 0, static_cast<int>(size - 1));
    int y1 = std::clamp(y0 + 1, 0, static_cast<int>(size - 1));

    float tx = fx - x0;
    float ty = fy - y0;

    auto getPixel = [&](int px, int py) {
        int idx = (px + py * size) * 4;
        return glm::vec4(cubemap[face][idx], cubemap[face][idx+1], cubemap[face][idx+2], cubemap[face][idx+3]);
    };

    glm::vec4 c00 = getPixel(x0, y0);
    glm::vec4 c10 = getPixel(x1, y0);
    glm::vec4 c01 = getPixel(x0, y1);
    glm::vec4 c11 = getPixel(x1, y1);

    return glm::mix(glm::mix(c00, c10, tx), glm::mix(c01, c11, tx), ty);
}

// GGX Geometry functions for BRDF LUT
static float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0f);
    float k = (r * r) / 8.0f;
    return NdotV / (NdotV * (1.0f - k) + k);
}

static float GeometrySmith(const glm::vec3& N, const glm::vec3& V, const glm::vec3& L, float roughness) {
    float NdotV = std::max(glm::dot(N, V), 0.0f);
    float NdotL = std::max(glm::dot(N, L), 0.0f);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

// Low-discrepancy Hammersley sequence
static float RadicalInverse_VdC(uint32_t bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10f;
}

static glm::vec2 ImportanceSample_Hammersley(uint32_t i, uint32_t N) {
    return glm::vec2(float(i)/float(N), RadicalInverse_VdC(i));
}

static glm::vec3 ImportanceSampleGGX(const glm::vec2& Xi, const glm::vec3& N, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0f * glm::pi<float>() * Xi.x;
    float cosTheta = std::sqrt(std::max(0.0f, (1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y)));
    float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
    
    glm::vec3 H;
    H.x = std::cos(phi) * sinTheta;
    H.y = std::sin(phi) * sinTheta;
    H.z = cosTheta;
    
    glm::vec3 up = std::abs(N.z) < 0.999f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 tangent = glm::normalize(glm::cross(up, N));
    glm::vec3 bitangent = glm::cross(N, tangent);
    
    return glm::normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

EnvironmentMap* EnvironmentSystem::GetOrCreateEnvironment(const std::string& hdrPath, const EngineResources& res) {
    auto it = m_Cache.find(hdrPath);
    if (it != m_Cache.end()) {
        return it->second.get();
    }

    auto map = std::make_unique<EnvironmentMap>();
    if (ProcessHDR(hdrPath, *map, res)) {
        m_Cache[hdrPath] = std::move(map);
        return m_Cache[hdrPath].get();
    }
    return nullptr;
}

Texture* EnvironmentSystem::GetBRDFLUT(const EngineResources& res) {
    if (!m_BRDFLUT) {
        m_BRDFLUT = std::make_unique<Texture>();
        GenerateBRDFLUT(*m_BRDFLUT, res);
    }
    return m_BRDFLUT.get();
}

void EnvironmentSystem::Cleanup() {
    m_Cache.clear();
    m_BRDFLUT.reset();
}

bool EnvironmentSystem::ProcessHDR(const std::string& hdrPath, EnvironmentMap& outMap, const EngineResources& res) {
    int w, h, channels;
    float* pixels = stbi_loadf(hdrPath.c_str(), &w, &h, &channels, STBI_rgb_alpha);
    if (!pixels) {
        LOG_ERROR(("EnvironmentSystem: Failed to load HDR file: " + hdrPath).c_str());
        return false;
    }

    // 1. Convert Equirectangular to Cubemap (512x512 faces)
    uint32_t cubeSize = 512;
    std::vector<std::vector<float>> cubeFaces(6, std::vector<float>(cubeSize * cubeSize * 4));

    for (int face = 0; face < 6; ++face) {
        for (uint32_t y = 0; y < cubeSize; ++y) {
            float v = (static_cast<float>(y) / (cubeSize - 1)) * 2.0f - 1.0f;
            for (uint32_t x = 0; x < cubeSize; ++x) {
                float u = (static_cast<float>(x) / (cubeSize - 1)) * 2.0f - 1.0f;
                glm::vec3 dir = getDirectionForFaceTexel(face, u, v);
                glm::vec2 eqUV = getEquirectangularUV(dir);
                glm::vec4 color = sampleEquirectangular(pixels, w, h, eqUV);
                
                int idx = (x + y * cubeSize) * 4;
                cubeFaces[face][idx] = color.r;
                cubeFaces[face][idx+1] = color.g;
                cubeFaces[face][idx+2] = color.b;
                cubeFaces[face][idx+3] = 1.0f;
            }
        }
    }
    stbi_image_free(pixels);

    outMap.skyboxCube = std::make_unique<Texture>();
    std::vector<std::vector<std::vector<float>>> skyboxMips = { cubeFaces };
    outMap.skyboxCube->createCubemapFromData(skyboxMips, cubeSize, cubeSize, 1, res);

    // 2. Convolve Diffuse Irradiance (32x32 faces)
    uint32_t irrSize = 32;
    std::vector<std::vector<float>> irrFaces(6, std::vector<float>(irrSize * irrSize * 4));

    for (int face = 0; face < 6; ++face) {
        for (uint32_t y = 0; y < irrSize; ++y) {
            float v = (static_cast<float>(y) / (irrSize - 1)) * 2.0f - 1.0f;
            for (uint32_t x = 0; x < irrSize; ++x) {
                float u = (static_cast<float>(x) / (irrSize - 1)) * 2.0f - 1.0f;
                glm::vec3 N = getDirectionForFaceTexel(face, u, v);

                glm::vec3 tangent = glm::normalize(std::abs(N.z) < 0.999f ? glm::vec3(0, 0, 1) : glm::vec3(1, 0, 0));
                glm::vec3 up = glm::cross(N, tangent);
                glm::vec3 right = glm::cross(up, N);

                glm::vec3 irradiance(0.0f);
                float totalWeight = 0.0f;

                int samplesTheta = 16;
                int samplesPhi = 32;
                for (int phiIdx = 0; phiIdx < samplesPhi; ++phiIdx) {
                    float phi = (static_cast<float>(phiIdx) / samplesPhi) * 2.0f * glm::pi<float>();
                    for (int thetaIdx = 0; thetaIdx < samplesTheta; ++thetaIdx) {
                        float theta = (static_cast<float>(thetaIdx) / samplesTheta) * 0.5f * glm::pi<float>();
                        
                        glm::vec3 temp = glm::vec3(std::sin(theta) * std::cos(phi), std::sin(theta) * std::sin(phi), std::cos(theta));
                        glm::vec3 sampleDir = right * temp.x + up * temp.y + N * temp.z;
                        
                        irradiance += glm::vec3(sampleCubemapLinear(cubeFaces, cubeSize, sampleDir)) * std::cos(theta) * std::sin(theta);
                        totalWeight += std::cos(theta) * std::sin(theta);
                    }
                }
                irradiance = glm::pi<float>() * irradiance / totalWeight;

                int idx = (x + y * irrSize) * 4;
                irrFaces[face][idx] = irradiance.r;
                irrFaces[face][idx+1] = irradiance.g;
                irrFaces[face][idx+2] = irradiance.b;
                irrFaces[face][idx+3] = 1.0f;
            }
        }
    }

    outMap.irradianceCube = std::make_unique<Texture>();
    std::vector<std::vector<std::vector<float>>> irrMips = { irrFaces };
    outMap.irradianceCube->createCubemapFromData(irrMips, irrSize, irrSize, 1, res);

    // 3. GGX Specular Prefilter (128x128, 6 mips)
    uint32_t prefSize = 128;
    uint32_t mips = 6;
    std::vector<std::vector<std::vector<float>>> prefMips(mips);

    for (uint32_t mip = 0; mip < mips; ++mip) {
        uint32_t mipSize = std::max(1u, prefSize >> mip);
        prefMips[mip].resize(6, std::vector<float>(mipSize * mipSize * 4));
        float roughness = static_cast<float>(mip) / static_cast<float>(mips - 1);

        for (int face = 0; face < 6; ++face) {
            for (uint32_t y = 0; y < mipSize; ++y) {
                float v = (static_cast<float>(y) / std::max(1u, mipSize - 1)) * 2.0f - 1.0f;
                for (uint32_t x = 0; x < mipSize; ++x) {
                    float u = (static_cast<float>(x) / std::max(1u, mipSize - 1)) * 2.0f - 1.0f;
                    glm::vec3 N = getDirectionForFaceTexel(face, u, v);

                    glm::vec3 V = N;
                    glm::vec3 prefilteredColor(0.0f);
                    float totalWeight = 0.0f;

                    const uint32_t SAMPLE_COUNT = 64;
                    for (uint32_t i = 0; i < SAMPLE_COUNT; ++i) {
                        glm::vec2 Xi = ImportanceSample_Hammersley(i, SAMPLE_COUNT);
                        glm::vec3 H = ImportanceSampleGGX(Xi, N, roughness);
                        glm::vec3 L = glm::normalize(2.0f * glm::dot(V, H) * H - V);
                        
                        float NdotL = std::max(glm::dot(N, L), 0.0f);
                        if (NdotL > 0.0f) {
                            prefilteredColor += glm::vec3(sampleCubemapLinear(cubeFaces, cubeSize, L)) * NdotL;
                            totalWeight += NdotL;
                        }
                    }

                    if (totalWeight > 0.0f) {
                        prefilteredColor /= totalWeight;
                    }

                    int idx = (x + y * mipSize) * 4;
                    prefMips[mip][face][idx] = prefilteredColor.r;
                    prefMips[mip][face][idx+1] = prefilteredColor.g;
                    prefMips[mip][face][idx+2] = prefilteredColor.b;
                    prefMips[mip][face][idx+3] = 1.0f;
                }
            }
        }
    }

    outMap.prefilterCube = std::make_unique<Texture>();
    outMap.prefilterCube->createCubemapFromData(prefMips, prefSize, prefSize, mips, res);

    LOG_INFO(("EnvironmentSystem: Successfully convolved HDR environment: " + hdrPath).c_str());
    return true;
}

void EnvironmentSystem::GenerateBRDFLUT(Texture& lut, const EngineResources& res) {
    uint32_t size = 128;
    std::vector<float> pixels(size * size * 4);

    for (uint32_t y = 0; y < size; ++y) {
        float roughness = static_cast<float>(y) / (size - 1);
        for (uint32_t x = 0; x < size; ++x) {
            float NdotV = std::max(0.01f, static_cast<float>(x) / (size - 1));

            glm::vec3 V;
            V.x = std::sqrt(1.0f - NdotV * NdotV);
            V.y = 0.0f;
            V.z = NdotV;
            
            float A = 0.0f;
            float B = 0.0f;
            
            glm::vec3 N = glm::vec3(0.0f, 0.0f, 1.0f);
            
            const uint32_t SAMPLE_COUNT = 128;
            for (uint32_t i = 0; i < SAMPLE_COUNT; ++i) {
                glm::vec2 Xi = ImportanceSample_Hammersley(i, SAMPLE_COUNT);
                glm::vec3 H = ImportanceSampleGGX(Xi, N, roughness);
                glm::vec3 L = glm::normalize(2.0f * glm::dot(V, H) * H - V);
                
                float NdotL = std::max(L.z, 0.0f);
                float NdotH = std::max(H.z, 0.0f);
                float VdotH = std::max(glm::dot(V, H), 0.0f);
                
                if (NdotL > 0.0f) {
                    float G = GeometrySmith(N, V, L, roughness);
                    float G_Vis = (G * VdotH) / (NdotH * NdotV);
                    float Fc = std::pow(1.0f - VdotH, 5.0f);
                    
                    A += (1.0f - Fc) * G_Vis;
                    B += Fc * G_Vis;
                }
            }
            A /= float(SAMPLE_COUNT);
            B /= float(SAMPLE_COUNT);

            int idx = (x + y * size) * 4;
            pixels[idx] = A;
            pixels[idx+1] = B;
            pixels[idx+2] = 0.0f;
            pixels[idx+3] = 1.0f;
        }
    }

    lut.create2DTextureFromData(pixels, size, size, VK_FORMAT_R32G32B32A32_SFLOAT, res);
    LOG_INFO("EnvironmentSystem: Successfully generated BRDF integration LUT.");
}

bool EnvironmentSystem::ProcessRawCubemap(
    const std::vector<std::vector<float>>& cubeFaces,
    uint32_t cubeSize,
    EnvironmentMap& outMap,
    const EngineResources& res
) {
    // 1. Process Diffuse Irradiance (32x32)
    uint32_t irrSize = 32;
    std::vector<std::vector<float>> irrFaces(6, std::vector<float>(irrSize * irrSize * 4));

    for (int face = 0; face < 6; ++face) {
        for (uint32_t y = 0; y < irrSize; ++y) {
            float v = (static_cast<float>(y) / (irrSize - 1)) * 2.0f - 1.0f;
            for (uint32_t x = 0; x < irrSize; ++x) {
                float u = (static_cast<float>(x) / (irrSize - 1)) * 2.0f - 1.0f;
                glm::vec3 N = getDirectionForFaceTexel(face, u, v);

                glm::vec3 tangent = glm::normalize(std::abs(N.z) < 0.999f ? glm::vec3(0, 0, 1) : glm::vec3(1, 0, 0));
                glm::vec3 up = glm::cross(N, tangent);
                glm::vec3 right = glm::cross(up, N);

                glm::vec3 irradiance(0.0f);
                float totalWeight = 0.0f;

                int samplesTheta = 16;
                int samplesPhi = 32;
                for (int phiIdx = 0; phiIdx < samplesPhi; ++phiIdx) {
                    float phi = (static_cast<float>(phiIdx) / samplesPhi) * 2.0f * glm::pi<float>();
                    for (int thetaIdx = 0; thetaIdx < samplesTheta; ++thetaIdx) {
                        float theta = (static_cast<float>(thetaIdx) / samplesTheta) * 0.5f * glm::pi<float>();
                        
                        glm::vec3 temp = glm::vec3(std::sin(theta) * std::cos(phi), std::sin(theta) * std::sin(phi), std::cos(theta));
                        glm::vec3 sampleDir = right * temp.x + up * temp.y + N * temp.z;
                        
                        irradiance += glm::vec3(sampleCubemapLinear(cubeFaces, cubeSize, sampleDir)) * std::cos(theta) * std::sin(theta);
                        totalWeight += std::cos(theta) * std::sin(theta);
                    }
                }
                irradiance = glm::pi<float>() * irradiance / totalWeight;

                int idx = (x + y * irrSize) * 4;
                irrFaces[face][idx] = irradiance.r;
                irrFaces[face][idx+1] = irradiance.g;
                irrFaces[face][idx+2] = irradiance.b;
                irrFaces[face][idx+3] = 1.0f;
            }
        }
    }

    outMap.irradianceCube = std::make_unique<Texture>();
    std::vector<std::vector<std::vector<float>>> irrMips = { irrFaces };
    outMap.irradianceCube->createCubemapFromData(irrMips, irrSize, irrSize, 1, res);

    // 2. Process GGX Specular Prefilter (128x128, 6 mips)
    uint32_t prefSize = 128;
    uint32_t mips = 6;
    std::vector<std::vector<std::vector<float>>> prefMips(mips);

    for (uint32_t mip = 0; mip < mips; ++mip) {
        uint32_t mipSize = std::max(1u, prefSize >> mip);
        prefMips[mip].resize(6, std::vector<float>(mipSize * mipSize * 4));
        float roughness = static_cast<float>(mip) / static_cast<float>(mips - 1);

        for (int face = 0; face < 6; ++face) {
            for (uint32_t y = 0; y < mipSize; ++y) {
                float v = (static_cast<float>(y) / std::max(1u, mipSize - 1)) * 2.0f - 1.0f;
                for (uint32_t x = 0; x < mipSize; ++x) {
                    float u = (static_cast<float>(x) / std::max(1u, mipSize - 1)) * 2.0f - 1.0f;
                    glm::vec3 N = getDirectionForFaceTexel(face, u, v);

                    glm::vec3 V = N;
                    glm::vec3 prefilteredColor(0.0f);
                    float totalWeight = 0.0f;

                    const uint32_t SAMPLE_COUNT = 64;
                    for (uint32_t i = 0; i < SAMPLE_COUNT; ++i) {
                        glm::vec2 Xi = ImportanceSample_Hammersley(i, SAMPLE_COUNT);
                        glm::vec3 H = ImportanceSampleGGX(Xi, N, roughness);
                        glm::vec3 L = glm::normalize(2.0f * glm::dot(V, H) * H - V);
                        
                        float NdotL = std::max(glm::dot(N, L), 0.0f);
                        if (NdotL > 0.0f) {
                            prefilteredColor += glm::vec3(sampleCubemapLinear(cubeFaces, cubeSize, L)) * NdotL;
                            totalWeight += NdotL;
                        }
                    }

                    if (totalWeight > 0.0f) {
                        prefilteredColor /= totalWeight;
                    }

                    int idx = (x + y * mipSize) * 4;
                    prefMips[mip][face][idx] = prefilteredColor.r;
                    prefMips[mip][face][idx+1] = prefilteredColor.g;
                    prefMips[mip][face][idx+2] = prefilteredColor.b;
                    prefMips[mip][face][idx+3] = 1.0f;
                }
            }
        }
    }

    outMap.prefilterCube = std::make_unique<Texture>();
    outMap.prefilterCube->createCubemapFromData(prefMips, prefSize, prefSize, mips, res);

    return true;
}

} // namespace eng::renderer
