#pragma once

#include "Runtime/Public/Gameplay/GameplayEvent.h"
#include <string>

namespace eng::runtime {

    struct RuntimeContext;

    class CheckpointSystem
    {
    public:
        CheckpointSystem() = default;
        ~CheckpointSystem() = default;

        void Initialize(RuntimeContext* context);

    private:
        void HandleTriggerEnter(const GameplayEvent& event);

        RuntimeContext* m_Context = nullptr;
    };

} // namespace eng::runtime
