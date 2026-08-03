#pragma once
#include <glm/glm.hpp>
#include "Runtime/AssetHandle.h"

namespace eng::renderer {

    enum class MaterialBlendMode : uint32_t {
        Opaque = 0,
        Mask = 1,
        Blend = 2
    };

    enum class MaterialShadingModel : uint32_t {
        Lit = 0,
        Unlit = 1
    };

    struct MaterialAsset {
        AssetHandle albedoTexture;
        AssetHandle normalTexture;
        AssetHandle metallicRoughnessTexture;
        AssetHandle aoTexture;
        AssetHandle emissiveTexture;

        glm::vec4 baseColorFactor{0.65f, 0.65f, 0.65f, 1.0f};
        float metallicFactor = 0.0f;
        float roughnessFactor = 0.6f;
        float normalScale = 1.0f;
        float emissiveStrength = 1.0f;
        float clearcoatFactor = 0.0f;
        float clearcoatRoughness = 0.1f;
        MaterialBlendMode blendMode = MaterialBlendMode::Opaque;
        MaterialShadingModel shadingModel = MaterialShadingModel::Lit;
    };

    struct MaterialGPU {
        glm::vec4 baseColorFactor{0.65f, 0.65f, 0.65f, 1.0f};
        float roughnessFactor = 0.6f;
        float metallicFactor = 0.0f;
        float normalScale = 1.0f;
        float emissiveStrength = 1.0f;

        float clearcoatFactor = 0.0f;
        float clearcoatRoughness = 0.1f;
        float paddingFloat1 = 0.0f;
        float paddingFloat2 = 0.0f;

        float hasAlbedoMap = 0.0f;
        float useNormalMap = 0.0f;
        float hasMetallicRoughnessMap = 0.0f;
        float hasAOMap = 0.0f;

        float hasEmissiveMap = 0.0f;
        uint32_t blendMode = 0;
        uint32_t shadingModel = 0;
        uint32_t padding = 0;
    };

} // namespace eng::renderer
