#pragma once

#include <cstdint>

#ifndef OMNIX_ENTITY_DEFINED
#define OMNIX_ENTITY_DEFINED
using Entity = std::uint32_t;
constexpr Entity INVALID_ENTITY = 0;
#endif

namespace eng::runtime {

    struct PlayerStateComponent
    {
        Entity ActivePlayer = INVALID_ENTITY;

        float Health = 100.0f;
        float MaxHealth = 100.0f;
        bool IsAlive = true;

        Entity CurrentInteractionTarget = INVALID_ENTITY;

        bool MovementEnabled = true;

        void Reset() {
            ActivePlayer = INVALID_ENTITY;
            Health = 100.0f;
            MaxHealth = 100.0f;
            IsAlive = true;
            CurrentInteractionTarget = INVALID_ENTITY;
            MovementEnabled = true;
        }
    };

    struct PlayerTagComponent
    {
        bool active = true;
    };

} // namespace eng::runtime
