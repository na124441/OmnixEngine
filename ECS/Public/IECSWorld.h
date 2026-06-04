#pragma once

#include <cstdint>
#include "ECS/ECSconfig.h"

class Coordinator;

namespace eng::runtime {

    class IECSWorld {
    public:
        virtual void Initialize() = 0;
        virtual void Shutdown() = 0;

        virtual void Update(float deltaTime) = 0;
        virtual Coordinator& getCoordinator() = 0;
        virtual std::unique_ptr<IECSWorld> Clone() const = 0;
    };

} // namespace eng::runtime
