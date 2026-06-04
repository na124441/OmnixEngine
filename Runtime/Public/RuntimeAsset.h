#pragma once
#include "Runtime/Public/AssetHandle.h"
#include "Runtime/Public/AssetType.h"
#include "Runtime/Public/OmnixMeshFormat.h" // For BoundingBox, BoundingSphere, etc.
#include "Runtime/Public/OmnixMaterialFormat.h" // For parameters, slots, etc.
#include <string>
#include <vector>

namespace eng::runtime {

    struct RuntimeAsset
    {
        AssetHandle handle;
        AssetType type = AssetType::Unknown;
        std::string debugName;

        virtual ~RuntimeAsset() = default;
    };

    struct RuntimeTexture : public RuntimeAsset
    {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t channels = 0;
        uint32_t mipCount = 0;
        uint32_t runtimeFormat = 0;
        bool isSRGB = false;
        bool isCompressed = false;
        std::vector<uint8_t> pixelData;
    };

    struct RuntimeMesh : public RuntimeAsset
    {
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
        uint32_t vertexStride = 0;
        uint32_t submeshCount = 0;
        bool hasSkeleton = false;

        BoundingBox bounds;
        BoundingSphere sphere;

        // Keep a copy of data in CPU memory
        std::vector<OmnixVertex> vertices;
        std::vector<OmnixSkinnedVertex> skinnedVertices;
        std::vector<uint32_t> indices;
        std::vector<OmnixSubmesh> submeshes;
        std::vector<AssetHandle> materialSlots;
        std::string skeletonAssetPath;
    };

    struct RuntimeMaterial : public RuntimeAsset
    {
        AssetHandle shader;
        std::string materialName;
        std::vector<MaterialTextureBinding> textures;
        std::vector<MaterialScalarParameter> scalars;
        std::vector<MaterialVectorParameter> vectors;
    };

} // namespace eng::runtime
