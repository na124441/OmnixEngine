//============================================================================
// BoundsUpdateSystem.h - Entity Bounds Update System
//
// Iterates entities that have both TransformComponent and BoundsComponent.
// For each entity whose bounds are dirty (transform changed), it:
//   1. Transforms the 8 local AABB corners through the TRS world matrix.
//   2. Computes the new world-space AABB (min/max of all 8 projected corners).
//   3. Optionally updates the bounding sphere (center + scaled radius).
//   4. Validates the resulting bounds and logs a warning for degenerate cases.
//
// Created: Week 5 — Entity Bounds System
//============================================================================

#pragma once

#include "ECS/SystemManager.h"
#include "ECSConfig.h"
#include "ECS/ECSComponents.h"
#include "ECS/Coordinator.h"
#include "Core/Logging/Logger.h"
#include <limits>
#include <cmath>

/**
 * @brief BoundsUpdateSystem
 *
 * System Signature:
 *   - TransformComponent (required)
 *   - BoundsComponent    (required)
 */
class BoundsUpdateSystem : public System {
public:

    /**
     * @brief Update all bounds that are dirty.
     *
     * Should be called once per frame, before rendering/culling.
     * Only processes entities whose BoundsComponent.dirty flag is set.
     *
     * @param deltaTime  Frame delta (unused — kept for consistency with other systems)
     * @param coordinator  ECS coordinator
     */
    void Update(float /*deltaTime*/, Coordinator& coordinator) {
        for (Entity entity : m_Entities) {
            auto& bounds    = coordinator.GetComponent<BoundsComponent>(entity);
            auto& transform = coordinator.GetComponent<TransformComponent>(entity);

            // Recompute whenever the transform is dirty, bounds are dirty, or both.
            if (!bounds.dirty && !transform.dirty) {
                continue;
            }

            // ----------------------------------------------------------------
            // 1. Build world-space TRS matrix
            // ----------------------------------------------------------------
            Matrix4x4 trs = Matrix4x4::TRS(
                transform.position,
                transform.rotation,
                transform.scale
            );

            // ----------------------------------------------------------------
            // 2. Transform 8 local AABB corners → world positions
            //    and find the new AABB extents.
            // ----------------------------------------------------------------
            const Vector3& lMin = bounds.localMin;
            const Vector3& lMax = bounds.localMax;

            // Degenerate local bounds check: if local bounds are all-zero,
            // initialise them to a unit box so the entity is at least visible.
            bool localDegenerate =
                (lMin.x == lMax.x) &&
                (lMin.y == lMax.y) &&
                (lMin.z == lMax.z);

            if (localDegenerate) {
                // A unit box centred at origin is a safe default.
                bounds.worldMin = {
                    transform.position.x - 0.5f,
                    transform.position.y - 0.5f,
                    transform.position.z - 0.5f
                };
                bounds.worldMax = {
                    transform.position.x + 0.5f,
                    transform.position.y + 0.5f,
                    transform.position.z + 0.5f
                };
            } else {
                // Generate all 8 corners of the local AABB
                Vector3 corners[8] = {
                    { lMin.x, lMin.y, lMin.z },
                    { lMax.x, lMin.y, lMin.z },
                    { lMin.x, lMax.y, lMin.z },
                    { lMax.x, lMax.y, lMin.z },
                    { lMin.x, lMin.y, lMax.z },
                    { lMax.x, lMin.y, lMax.z },
                    { lMin.x, lMax.y, lMax.z },
                    { lMax.x, lMax.y, lMax.z }
                };

                float wMinX =  std::numeric_limits<float>::max();
                float wMinY =  std::numeric_limits<float>::max();
                float wMinZ =  std::numeric_limits<float>::max();
                float wMaxX = -std::numeric_limits<float>::max();
                float wMaxY = -std::numeric_limits<float>::max();
                float wMaxZ = -std::numeric_limits<float>::max();

                for (int i = 0; i < 8; ++i) {
                    Vector3 w = trs.TransformPoint(corners[i]);
                    if (w.x < wMinX) wMinX = w.x;
                    if (w.y < wMinY) wMinY = w.y;
                    if (w.z < wMinZ) wMinZ = w.z;
                    if (w.x > wMaxX) wMaxX = w.x;
                    if (w.y > wMaxY) wMaxY = w.y;
                    if (w.z > wMaxZ) wMaxZ = w.z;
                }

                bounds.worldMin = { wMinX, wMinY, wMinZ };
                bounds.worldMax = { wMaxX, wMaxY, wMaxZ };
            }

            // ----------------------------------------------------------------
            // 3. Update bounding sphere (optional)
            // ----------------------------------------------------------------
            if (bounds.hasSphere) {
                // Transform the local sphere centre
                bounds.worldSphereCenter = trs.TransformPoint(bounds.sphereCenter);

                // Scale the radius by the largest world-space scale component
                float sx = std::fabs(transform.scale.x);
                float sy = std::fabs(transform.scale.y);
                float sz = std::fabs(transform.scale.z);
                float maxScale = sx > sy ? (sx > sz ? sx : sz) : (sy > sz ? sy : sz);
                bounds.worldSphereRadius = bounds.sphereRadius * maxScale;
            }

            // ----------------------------------------------------------------
            // 4. Validate final world-space AABB
            // ----------------------------------------------------------------
            bool valid = IsValid(bounds);
            if (!valid) {
                CORE_LOG_WARN(
                    "BoundsUpdateSystem: Entity {} produced invalid/NaN world bounds "
                    "— resetting to fallback.",
                    (uint32_t)entity
                );
                // Reset to a safe unit box at the entity's position
                bounds.worldMin = {
                    transform.position.x - 0.5f,
                    transform.position.y - 0.5f,
                    transform.position.z - 0.5f
                };
                bounds.worldMax = {
                    transform.position.x + 0.5f,
                    transform.position.y + 0.5f,
                    transform.position.z + 0.5f
                };
            }

            // Bounds are now up-to-date
            bounds.dirty = false;
        }
    }

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    /**
     * @brief Returns true if the world-space bounds are finite and non-degenerate
     *        (i.e. worldMin <= worldMax on all axes, no NaN/inf).
     */
    static bool IsValid(const BoundsComponent& bounds) {
        auto finite = [](float v) { return std::isfinite(v); };
        return finite(bounds.worldMin.x) && finite(bounds.worldMin.y) && finite(bounds.worldMin.z)
            && finite(bounds.worldMax.x) && finite(bounds.worldMax.y) && finite(bounds.worldMax.z)
            && bounds.worldMin.x <= bounds.worldMax.x
            && bounds.worldMin.y <= bounds.worldMax.y
            && bounds.worldMin.z <= bounds.worldMax.z;
    }

    std::shared_ptr<System> Clone() const override {
        auto clone = std::make_shared<BoundsUpdateSystem>();
        clone->m_Entities = this->m_Entities;
        return clone;
    }
};
