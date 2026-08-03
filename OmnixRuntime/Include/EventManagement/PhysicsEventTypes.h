#pragma once
#include "Core/Events/PhysicsEventTypes.h"

namespace Omnix {

    using PhysicsEvent = eng::core::PhysicsEvent;
    using CollisionEnterEvent = eng::core::CollisionEvent;
    using CollisionExitEvent = eng::core::CollisionExitEvent;
    using TriggerEnterEvent = eng::core::TriggerEnterEvent;
    using TriggerExitEvent = eng::core::TriggerExitEvent;
    using TriggerStayEvent = eng::core::TriggerStayEvent;
    using GameplayEvent = eng::core::GameplayEvent;

} // namespace Omnix
