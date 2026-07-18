#pragma once

#include "Runtime/OmnixMeshFormat.h"
#include <cstdint>

namespace Omnix
{
    constexpr char OMNIX_ZONE_MAGIC[8] = { 'O', 'M', 'X', 'Z', 'O', 'N', 'E' };
    constexpr uint32_t OMNIX_ZONE_VERSION = 1;

    #pragma pack(push, 1)
    struct OmnixZoneHeader
    {
        char magic[8];
        uint32_t version;
        uint64_t zoneUUIDHigh;
        uint64_t zoneUUIDLow;
        char zoneName[128];
        char sceneAssetPath[256];
        BoundingBox bounds;
        int32_t loadingPriority;
        float activationRadius;
        
        uint32_t assetDependencyCount;
        uint32_t neighborCount;
        uint32_t tagCount;

        uint64_t assetDependencyTableOffset;
        uint64_t neighborTableOffset;
        uint64_t tagTableOffset;
        uint64_t checksumOffset;
        uint64_t fileSize;
        uint32_t headerSize;
        uint32_t reserved[14];
    };

    struct SerializedZoneAssetDependency
    {
        uint64_t assetUUIDHigh;
        uint64_t assetUUIDLow;
        char assetPath[256];
        uint32_t assetType;
    };

    struct SerializedZoneNeighbor
    {
        uint64_t zoneUUIDHigh;
        uint64_t zoneUUIDLow;
    };
    #pragma pack(pop)
}
