#pragma once

#include "ECS/SystemManager.h"
#include "ECS/ECSComponents.h"
#include "ECS/Coordinator.h"
#include "Runtime/Public/RuntimeContext.h"

namespace eng::runtime {

    class InteractionSystem : public System {
    public:
        InteractionSystem() = default;
        ~InteractionSystem() override = default;

        void Update(float dt, RuntimeContext& context);

        std::shared_ptr<System> Clone() const override {
            auto clone = std::make_shared<InteractionSystem>();
            clone->m_Entities = this->m_Entities;
            return clone;
        }
    };

} // namespace eng::runtime
