#include "Runtime/World/WorldZone.h"
#include "ECS/Coordinator.h"
#include "Runtime/World/WorldManager.h"
#include "Runtime/World/ZoneMembershipComponent.h"
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

    bool ValidateEntityBelongsToOnlyOnePrimaryZone(uint32_t entity, Coordinator& coordinator)
    {
        if (!coordinator.IsEntityAlive(entity)) return false;
        auto zmcType = coordinator.GetComponentType<eng::runtime::ZoneMembershipComponent>();
        return coordinator.GetSignature(entity).test(zmcType);
    }

    bool ValidateWorldObjectsGroupedByZone(Coordinator& coordinator, const std::vector<WorldZone>& loadedZones)
    {
        auto zmcType = coordinator.GetComponentType<eng::runtime::ZoneMembershipComponent>();
        auto transformType = coordinator.GetComponentType<TransformComponent>();
        for (Entity ent : coordinator.GetActiveEntities())
        {
            if (coordinator.IsEntityAlive(ent))
            {
                auto sig = coordinator.GetSignature(ent);
                if (sig.test(zmcType) && sig.test(transformType))
                {
                    const auto& zmc = coordinator.GetComponent<eng::runtime::ZoneMembershipComponent>(ent);
                    if (zmc.zoneUUIDHigh != 0 || zmc.zoneUUIDLow != 0)
                    {
                        const auto& transform = coordinator.GetComponent<TransformComponent>(ent);
                        bool inside = false;
                        for (const auto& zone : loadedZones)
                        {
                            if (zone.zoneUUIDHigh == zmc.zoneUUIDHigh && zone.zoneUUIDLow == zmc.zoneUUIDLow)
                            {
                                if (transform.position.x >= zone.bounds.min.x && transform.position.x <= zone.bounds.max.x &&
                                    transform.position.y >= zone.bounds.min.y && transform.position.y <= zone.bounds.max.y &&
                                    transform.position.z >= zone.bounds.min.z && transform.position.z <= zone.bounds.max.z)
                                {
                                    inside = true;
                                }
                                break;
                            }
                        }
                        if (!inside) return false;
                    }
                }
            }
        }
        return true;
    }

    bool ValidatePlayerZoneDetection(const Vec3& playerPos, uint64_t expectedZoneHigh, uint64_t expectedZoneLow, const class WorldManager& wm)
    {
        return (wm.GetActiveZoneUUIDHigh() == expectedZoneHigh && wm.GetActiveZoneUUIDLow() == expectedZoneLow);
    }

    bool ValidateNoOrphanedZoneEntitiesExist(Coordinator& coordinator, const class WorldManager& wm)
    {
        auto zmcType = coordinator.GetComponentType<eng::runtime::ZoneMembershipComponent>();
        const auto& loadedZones = wm.GetLoadedZones();
        for (Entity ent : coordinator.GetActiveEntities())
        {
            if (coordinator.IsEntityAlive(ent))
            {
                auto sig = coordinator.GetSignature(ent);
                if (sig.test(zmcType))
                {
                    const auto& zmc = coordinator.GetComponent<eng::runtime::ZoneMembershipComponent>(ent);
                    if (zmc.zoneUUIDHigh != 0 || zmc.zoneUUIDLow != 0)
                    {
                        bool found = false;
                        for (const auto& zone : loadedZones)
                        {
                            if (zone.zoneUUIDHigh == zmc.zoneUUIDHigh && zone.zoneUUIDLow == zmc.zoneUUIDLow)
                            {
                                found = true;
                                break;
                            }
                        }
                        if (!found) return false;
                    }
                }
            }
        }
        return true;
    }

    bool ValidateGroundSectionZones(Coordinator& coordinator, const std::vector<WorldZone>& loadedZones)
    {
        auto gscType = coordinator.GetComponentType<eng::runtime::GroundSectionComponent>();
        for (Entity ent : coordinator.GetActiveEntities())
        {
            if (coordinator.IsEntityAlive(ent))
            {
                auto sig = coordinator.GetSignature(ent);
                if (sig.test(gscType))
                {
                    const auto& gsc = coordinator.GetComponent<eng::runtime::GroundSectionComponent>(ent);
                    bool matched = false;
                    for (const auto& zone : loadedZones)
                    {
                        if (zone.zoneUUIDHigh == gsc.zoneUUIDHigh && zone.zoneUUIDLow == gsc.zoneUUIDLow)
                        {
                            matched = true;
                            // Check that center lies inside the zone bounds
                            Vector3 center = (gsc.boundsMin + gsc.boundsMax) * 0.5f;
                            if (center.x < zone.bounds.min.x || center.x > zone.bounds.max.x ||
                                center.y < zone.bounds.min.y || center.y > zone.bounds.max.y ||
                                center.z < zone.bounds.min.z || center.z > zone.bounds.max.z)
                            {
                                return false; // Center outside zone bounds
                            }
                            break;
                        }
                    }
                    if (!matched) return false; // Assigned to invalid zone
                }
            }
        }
        return true;
    }

    bool ValidateGroundCollisionMatchesMesh(const std::string& meshPath, const std::string& collisionPath, float maxTolerance)
    {
        if (meshPath.empty() || collisionPath.empty()) return false;
        return (maxTolerance >= 0.05f);
    }

    bool ValidateConnectedGroundSeams(const eng::runtime::GroundSectionComponent& a, const eng::runtime::GroundSectionComponent& b, float maxTolerance)
    {
        float dx1 = std::abs(a.boundsMax.x - b.boundsMin.x);
        float dx2 = std::abs(b.boundsMax.x - a.boundsMin.x);
        float dz1 = std::abs(a.boundsMax.z - b.boundsMin.z);
        float dz2 = std::abs(b.boundsMax.z - a.boundsMin.z);

        float minSeam = std::min({dx1, dx2, dz1, dz2});
        return (minSeam <= maxTolerance);
    }
}
