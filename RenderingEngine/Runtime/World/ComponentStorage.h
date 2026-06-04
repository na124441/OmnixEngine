#pragma once
#include <vector>
#include <cstdint>
#include "core/types/Result.h"
#include "core/containers/Array.h"
#include "core/containers/Bitset.h"
#include "runtime/world/Entity.h"

namespace eng::runtime {

    /**
     * @brief Sparse‑set storage for a component type `T`.
     *
     * The design follows the “sparse set” pattern used in most modern ECS
     * implementations (e.g., EnTT, Flecs).  It provides:
     *   - O(1) `Add`, `Remove`, `Get`.
     *   - Fast iteration over the **dense** component array.
     *
     * The storage does **not** own the `Entity` IDs – those are managed by `World`.
     */
    template <typename T>
    class ComponentStorage {
    public:
        using EntityId = Entity;

        ComponentStorage() = default;
        ~ComponentStorage() = default;

        /** Add or replace a component on the given entity. */
        eng::core::Result Add(EntityId e, const T& component) {
            if (Has(e)) {
                components_[sparse_[e.Get()]] = component;
                return eng::core::Result(); // Success – replace
            }
            // New entry
            if (sparse_.size() <= e.Get()) sparse_.resize(e.Get() + 1, InvalidIdx);
            sparse_[e.Get()] = static_cast<uint32_t>(dense_.size());
            dense_.push_back(e);
            components_.push_back(component);
            return eng::core::Result();
        }

        /** Remove component from entity (no‑op if absent). */
        eng::core::Result Remove(EntityId e) {
            if (!Has(e)) return eng::core::Result(eng::core::ResultCode::Failure);
            const uint32_t idx = sparse_[e.Get()];
            const EntityId lastEntity = dense_.back();
            // swap‑erase dense side
            dense_[idx] = lastEntity;
            components_[idx] = std::move(components_.back());
            sparse_[lastEntity.Get()] = idx;
            dense_.pop_back();
            components_.pop_back();
            sparse_[e.Get()] = InvalidIdx;
            return eng::core::Result();
        }

        /** Retrieve a pointer to the component, or nullptr if absent. */
        T* Get(EntityId e) {
            return Has(e) ? &components_[sparse_[e.Get()]] : nullptr;
        }
        const T* Get(EntityId e) const {
            return Has(e) ? &components_[sparse_[e.Get()]] : nullptr;
        }

        /** Fast test whether the entity has this component. */
        bool Has(EntityId e) const {
            return e.Get() < sparse_.size() && sparse_[e.Get()] != InvalidIdx;
        }

        /** Iterate over all components – returns pair(entity, component). */
        template <typename Fn>
        void ForEach(Fn&& fn) {
            for (size_t i = 0; i < dense_.size(); ++i) {
                fn(dense_[i], components_[i]);
            }
        }

    private:
        static constexpr uint32_t InvalidIdx = 0xFFFFFFFFu;
        std::vector<uint32_t>   sparse_;       // index = entity ID → dense index or InvalidIdx
        std::vector<EntityId>   dense_;        // dense packed list of entities
        std::vector<T>          components_;   // packed component data (parallel to dense_)
    };

} // namespace eng::runtime
