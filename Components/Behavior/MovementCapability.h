#pragma once

#include "ECS/Coordinator.h"
#include "ECS/SystemManager.h"

class MovementCapability : public System
{
public:
    void Update(Coordinator& coordinator, float dt);
};
