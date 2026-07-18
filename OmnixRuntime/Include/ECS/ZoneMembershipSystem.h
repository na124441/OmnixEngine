#pragma once

#include "ECS/SystemManager.h"
#include "ECSConfig.h"
#include "ECS/ECSComponents.h"
#include "ECS/Coordinator.h"
#include "Runtime/World/WorldManager.h"
#include "Runtime/World/ZoneMembershipComponent.h"
#include <algorithm>

class ZoneMembershipSystem : public System {
public:
    void Update(float /*deltaTime*/, Coordinator& coordinator, Omnix::WorldManager* worldManager) {
        if (!worldManager) return;
        const auto& loadedZones = worldManager->GetLoadedZones();

        for (Entity entity : m_Entities) {
            if (!coordinator.IsEntityAlive(entity)) {
                continue;
            }

            auto& transform = coordinator.GetComponent<TransformComponent>(entity);
            auto& zmc = coordinator.GetComponent<eng::runtime::ZoneMembershipComponent>(entity);

            Vec3 pos = { transform.position.x, transform.position.y, transform.position.z };

            // Check if still in current zone
            bool insideCurrent = false;
            if (zmc.zoneUUIDHigh != 0 || zmc.zoneUUIDLow != 0) {
                for (const auto& zone : loadedZones) {
                    if (zone.zoneUUIDHigh == zmc.zoneUUIDHigh && zone.zoneUUIDLow == zmc.zoneUUIDLow) {
                        if (pos.x >= zone.bounds.min.x && pos.x <= zone.bounds.max.x &&
                            pos.y >= zone.bounds.min.y && pos.y <= zone.bounds.max.y &&
                            pos.z >= zone.bounds.min.z && pos.z <= zone.bounds.max.z) {
                            insideCurrent = true;
                        }
                        break;
                    }
                }
            }

            if (!insideCurrent) {
                // Moved outside or unassigned. Search for matching zone.
                bool found = false;
                for (const auto& zone : loadedZones) {
                    if (pos.x >= zone.bounds.min.x && pos.x <= zone.bounds.max.x &&
                        pos.y >= zone.bounds.min.y && pos.y <= zone.bounds.max.y &&
                        pos.z >= zone.bounds.min.z && pos.z <= zone.bounds.max.z) {
                        zmc.zoneUUIDHigh = zone.zoneUUIDHigh;
                        zmc.zoneUUIDLow = zone.zoneUUIDLow;
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    // Set to 0 to signify outside bounds (orphaned if it should be in a zone)
                    zmc.zoneUUIDHigh = 0;
                    zmc.zoneUUIDLow = 0;
                }
            }
        }
    }
};
