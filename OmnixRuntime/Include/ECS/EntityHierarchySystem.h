#pragma once

#include "ECS/SystemManager.h"
#include "ECS/Coordinator.h"
#include "ECS/ECSComponents.h"
#include <algorithm>
#include <vector>

namespace eng::runtime {

    static constexpr uint32_t INVALID_ENTITY = 0xFFFFFFFF;

    /**
     * @brief EntityHierarchySystem - Manages ECS entity parent/child linkages and tree depth propagation
     */
    class EntityHierarchySystem : public System {
    public:
        void Update(Coordinator& coordinator) {
            for (Entity ent : m_Entities) {
                if (!coordinator.IsEntityAlive(ent)) continue;
                const auto& hc = coordinator.GetComponent<HierarchyComponent>(ent);
                if (hc.parent == 0xFFFFFFFF || hc.parent == INVALID_ENTITY) {
                    UpdateChildHierarchy(ent, coordinator, 0);
                }
            }
        }

        static void AttachChild(Entity parent, Entity child, Coordinator& coordinator) {
            if (parent == INVALID_ENTITY || child == INVALID_ENTITY || parent == child) return;

            if (!coordinator.HasComponent<HierarchyComponent>(parent)) {
                coordinator.AddComponent<HierarchyComponent>(parent, HierarchyComponent(0xFFFFFFFF, 0));
            }
            if (!coordinator.HasComponent<HierarchyComponent>(child)) {
                coordinator.AddComponent<HierarchyComponent>(child, HierarchyComponent(parent, 1));
            }

            auto& parentHc = coordinator.GetComponent<HierarchyComponent>(parent);
            auto& childHc = coordinator.GetComponent<HierarchyComponent>(child);

            // Detach from previous parent if any
            if (childHc.parent != 0xFFFFFFFF && childHc.parent != parent) {
                DetachChild(childHc.parent, child, coordinator);
            }

            childHc.parent = parent;
            childHc.depth = parentHc.depth + 1;

            if (std::find(parentHc.children.begin(), parentHc.children.end(), child) == parentHc.children.end()) {
                parentHc.children.push_back(child);
            }
        }

        static void DetachChild(Entity parent, Entity child, Coordinator& coordinator) {
            if (parent == INVALID_ENTITY || child == INVALID_ENTITY) return;

            if (coordinator.HasComponent<HierarchyComponent>(parent)) {
                auto& parentHc = coordinator.GetComponent<HierarchyComponent>(parent);
                auto it = std::find(parentHc.children.begin(), parentHc.children.end(), child);
                if (it != parentHc.children.end()) {
                    parentHc.children.erase(it);
                }
            }

            if (coordinator.HasComponent<HierarchyComponent>(child)) {
                auto& childHc = coordinator.GetComponent<HierarchyComponent>(child);
                childHc.parent = 0xFFFFFFFF;
                childHc.depth = 0;
            }
        }

    private:
        void UpdateChildHierarchy(Entity parentEnt, Coordinator& coordinator, uint32_t currentDepth) {
            if (!coordinator.HasComponent<HierarchyComponent>(parentEnt)) return;
            auto& parentHc = coordinator.GetComponent<HierarchyComponent>(parentEnt);
            parentHc.depth = currentDepth;

            for (Entity childEnt : parentHc.children) {
                if (coordinator.IsEntityAlive(childEnt)) {
                    UpdateChildHierarchy(childEnt, coordinator, currentDepth + 1);
                }
            }
        }
    };
}
