#pragma once
#include "Core/Events/EventBus.h"

namespace eng::runtime {

    class EventBus : public eng::core::EventBus {
    public:
        using eng::core::EventBus::EventBus;
    };

    using IEventBusHandler = eng::core::IEventBusHandler;

    template<typename EventType>
    using EventBusHandler = eng::core::EventBusHandler<EventType>;

} // namespace eng::runtime
