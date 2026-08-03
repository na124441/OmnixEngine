#pragma once
#include "Core/Events/EventQueue.h"

namespace Omnix {

    class EventQueue : public eng::core::EventQueue {
    public:
        using eng::core::EventQueue::EventQueue;
    };

} // namespace Omnix
