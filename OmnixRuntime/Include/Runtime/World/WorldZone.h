#pragma once

#include "Runtime/OmnixMeshFormat.h"
#include "Runtime/World/WorldFileError.h"
#include "Runtime/World/WorldFileResult.h"
#include <string>
#include <vector>
#include <filesystem>
#include "Runtime/World/GroundSectionComponent.h"

class Coordinator;

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

    // Week 6 validations
    bool ValidateEntityBelongsToOnlyOnePrimaryZone(uint32_t entity, Coordinator& coordinator);
    bool ValidateWorldObjectsGroupedByZone(Coordinator& coordinator, const std::vector<WorldZone>& loadedZones);
    bool ValidatePlayerZoneDetection(const Vec3& playerPos, uint64_t expectedZoneHigh, uint64_t expectedZoneLow, const class WorldManager& wm);
    bool ValidateNoOrphanedZoneEntitiesExist(Coordinator& coordinator, const class WorldManager& wm);

    // Week 9 validations
    bool ValidateGroundSectionZones(Coordinator& coordinator, const std::vector<WorldZone>& loadedZones);
    bool ValidateGroundCollisionMatchesMesh(const std::string& meshPath, const std::string& collisionPath, float maxTolerance);
    bool ValidateConnectedGroundSeams(const eng::runtime::GroundSectionComponent& a, const eng::runtime::GroundSectionComponent& b, float maxTolerance);
}
