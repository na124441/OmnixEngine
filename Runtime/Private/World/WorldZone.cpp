#include "Runtime/Public/World/WorldZone.h"
#include <cmath>
#include <set>
#include <utility>

namespace Omnix
{
    bool ValidateZoneBounds(const WorldZone& zone)
    {
        // Check if all bounds coordinates are finite
        if (!std::isfinite(zone.bounds.min.x) || !std::isfinite(zone.bounds.min.y) || !std::isfinite(zone.bounds.min.z) ||
            !std::isfinite(zone.bounds.max.x) || !std::isfinite(zone.bounds.max.y) || !std::isfinite(zone.bounds.max.z))
        {
            return false;
        }

        // Check if min <= max
        if (zone.bounds.min.x > zone.bounds.max.x ||
            zone.bounds.min.y > zone.bounds.max.y ||
            zone.bounds.min.z > zone.bounds.max.z)
        {
            return false;
        }

        return true;
    }

    bool ValidateWorldZone(const WorldZone& zone)
    {
        // 1. Validate ID is non-zero
        if (zone.zoneUUIDHigh == 0 && zone.zoneUUIDLow == 0)
        {
            return false;
        }

        // 2. Validate Name is not empty
        if (zone.zoneName.empty())
        {
            return false;
        }

        // 3. Validate Scene reference is not empty
        if (zone.sceneAssetPath.empty())
        {
            return false;
        }

        // 4. Validate Bounds
        if (!ValidateZoneBounds(zone))
        {
            return false;
        }

        return true;
    }

    bool ValidateZoneIDsUnique(const std::vector<WorldZone>& zones)
    {
        std::set<std::pair<uint64_t, uint64_t>> uniqueIDs;
        for (const auto& zone : zones)
        {
            std::pair<uint64_t, uint64_t> idPair = { zone.zoneUUIDHigh, zone.zoneUUIDLow };
            if (uniqueIDs.count(idPair) > 0)
            {
                return false; // Found a duplicate ID
            }
            uniqueIDs.insert(idPair);
        }
        return true;
    }
}
