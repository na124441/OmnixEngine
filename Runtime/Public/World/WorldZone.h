#pragma once

#include "Runtime/Public/OmnixMeshFormat.h"
#include "Runtime/Public/World/WorldFileError.h"
#include "Runtime/Public/World/WorldFileResult.h"
#include <string>
#include <vector>
#include <filesystem>

namespace Omnix
{
    enum class ZoneState : uint32_t
    {
        Unloaded = 0,
        Loading,
        Loaded,
        Active,
        Inactive,
        Unloading,
        Failed
    };

    struct ZoneAssetDependency
    {
        uint64_t assetUUIDHigh = 0;
        uint64_t assetUUIDLow = 0;
        std::string assetPath;
        uint32_t assetType = 0;

        bool operator==(const ZoneAssetDependency& o) const {
            return assetUUIDHigh == o.assetUUIDHigh &&
                   assetUUIDLow == o.assetUUIDLow &&
                   assetPath == o.assetPath &&
                   assetType == o.assetType;
        }
    };

    struct ZoneNeighbor
    {
        uint64_t zoneUUIDHigh = 0;
        uint64_t zoneUUIDLow = 0;

        bool operator==(const ZoneNeighbor& o) const {
            return zoneUUIDHigh == o.zoneUUIDHigh && zoneUUIDLow == o.zoneUUIDLow;
        }
    };

    struct WorldZone
    {
        uint64_t zoneUUIDHigh = 0;
        uint64_t zoneUUIDLow = 0;
        std::string zoneName;
        BoundingBox bounds;
        std::string sceneAssetPath;
        std::vector<ZoneAssetDependency> assetDependencies;
        std::vector<ZoneNeighbor> neighbors;
        std::vector<std::string> gameplayTags;
        int32_t loadingPriority = 0;
        float activationRadius = 0.0f;
        ZoneState state = ZoneState::Unloaded;
    };

    // Validation functions
    bool ValidateZoneBounds(const WorldZone& zone);
    bool ValidateWorldZone(const WorldZone& zone);
    bool ValidateZoneIDsUnique(const std::vector<WorldZone>& zones);
}
