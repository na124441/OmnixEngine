#pragma once
#include "Core/Events/GameEvent.h"

namespace Omnix {

    using EventType = eng::core::EventType;
    using ListenerHandle = eng::core::ListenerHandle;

    class GameEvent : public eng::core::GameEvent {
    protected:
        explicit GameEvent(uint8_t priority = 128)
            : eng::core::GameEvent(priority) {}
    };

} // namespace Omnix
