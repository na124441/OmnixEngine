#pragma once
#include "Runtime/Public/AssetType.h"
#include "Runtime/Public/AssetMetadata.h"
#include "Runtime/Public/RuntimeAsset.h"

namespace eng::runtime {

    class IAssetLoader
    {
    public:
        virtual ~IAssetLoader() = default;

        virtual AssetType GetSupportedType() const = 0;

        virtual bool CanLoad(AssetType type) const = 0;

        virtual bool Load(
            const AssetMetadata& metadata,
            RuntimeAsset** outAsset
        ) = 0;

        virtual void Unload(RuntimeAsset* asset) = 0;
    };

} // namespace eng::runtime
