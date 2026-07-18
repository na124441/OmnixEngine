#pragma once
#include "Runtime/IAssetLoader.h"

namespace eng::runtime {

    class TextureLoader : public IAssetLoader
    {
    public:
        TextureLoader() = default;
        virtual ~TextureLoader() = default;

        AssetType GetSupportedType() const override { return AssetType::Texture; }
        bool CanLoad(AssetType type) const override { return type == AssetType::Texture; }
        bool Load(const AssetMetadata& metadata, RuntimeAsset** outAsset) override;
        void Unload(RuntimeAsset* asset) override;
    };

    class MeshLoader : public IAssetLoader
    {
    public:
        MeshLoader() = default;
        virtual ~MeshLoader() = default;

        AssetType GetSupportedType() const override { return AssetType::Mesh; }
        bool CanLoad(AssetType type) const override { return type == AssetType::Mesh; }
        bool Load(const AssetMetadata& metadata, RuntimeAsset** outAsset) override;
        void Unload(RuntimeAsset* asset) override;
    };

    class MaterialLoader : public IAssetLoader
    {
    public:
        MaterialLoader() = default;
        virtual ~MaterialLoader() = default;

        AssetType GetSupportedType() const override { return AssetType::Material; }
        bool CanLoad(AssetType type) const override { return type == AssetType::Material; }
        bool Load(const AssetMetadata& metadata, RuntimeAsset** outAsset) override;
        void Unload(RuntimeAsset* asset) override;
    };

} // namespace eng::runtime
